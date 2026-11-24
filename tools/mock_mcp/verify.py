"""Run the five-tool MCP acceptance suite against an owned, isolated Backend."""

import argparse
import asyncio
from contextlib import asynccontextmanager
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
from tempfile import TemporaryDirectory
import time
from uuid import uuid4

import httpx
import httpx2
from jsonschema import Draft202012Validator
from mcp.client import Client
from mcp.client.stdio import StdioServerParameters
from mcp.client.streamable_http import streamable_http_client
from websockets.asyncio.client import connect

from plan_writes import BEGIN_ROUTE, END_ROUTE, PLAN_WRITE_TOOLS, ROUTE_WRITE_TOOLS, SAVE_ROUTE, VALIDATE
from record_replay import CLOSE, OPEN, SEEK, RECORD_REPLAY_TOOLS
from workspace import WORKSPACE_TOOLS

SERVER = Path(__file__).with_name("server.py").resolve()
READ_TOOLS = {"list_demo_scenarios", "get_sim_status", "list_sim_alerts"}
DRAFT_TOOL = "create_security_plan_draft"
REPLAY_TOOL = "set_replay_state"


def check(label):
    print(f"PASS {label}", flush=True)


def free_ports(count):
    sockets = [socket.socket() for _ in range(count)]
    try:
        for sock in sockets:
            sock.bind(("127.0.0.1", 0))
        return [sock.getsockname()[1] for sock in sockets]
    finally:
        for sock in sockets:
            sock.close()


def start(command, env, workspace, log):
    with (workspace / log).open("ab") as output:
        return subprocess.Popen(command, cwd=workspace, env=env, stdin=subprocess.DEVNULL,
                                stdout=output, stderr=subprocess.STDOUT)


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(5)


async def wait_ready(process, http, url, status):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and process.poll() is None:
        try:
            if (await http.get(url)).status_code == status:
                return
        except httpx.RequestError:
            pass
        await asyncio.sleep(0.1)
    raise RuntimeError(f"Process did not start (exit={process.poll()})")


@asynccontextmanager
async def transport(url, token):
    async with httpx2.AsyncClient(headers={"Authorization": "Bearer " + token}, trust_env=False) as http:
        async with streamable_http_client(url, http_client=http) as streams:
            yield streams


async def listing(client, expected):
    listed = {tool.name: tool for tool in (await client.list_tools()).tools}
    assert set(listed) == expected, set(listed)
    for tool in listed.values():
        Draft202012Validator.check_schema(tool.input_schema)
        Draft202012Validator.check_schema(tool.output_schema)
    return listed


async def call(client, schema, name, args=None, error=None):
    result = await client.call_tool(name, args or {})
    data = result.structured_content
    assert bool(result.is_error) == bool(error), (name, data)
    Draft202012Validator(schema[name].output_schema).validate(data)
    assert json.loads(result.content[0].text) == data
    if error:
        assert data["error"]["code"] == error, data
    return data


def rpc(name, args):
    return {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {
        "name": name, "arguments": args,
        "_meta": {
            "io.modelcontextprotocol/protocolVersion": "2026-07-28",
            "io.modelcontextprotocol/clientInfo": {"name": "drone-mock-mcp-qa", "version": "1"},
            "io.modelcontextprotocol/clientCapabilities": {},
        },
    }}


def rpc_headers(token, name):
    return {"Authorization": "Bearer " + token, "Accept": "application/json, text/event-stream",
            "MCP-Protocol-Version": "2026-07-28", "Mcp-Method": "tools/call", "Mcp-Name": name}


async def run(backend_exe, workspace):
    backend_port, ws_port, mcp_port = free_ports(3)
    backend = f"http://127.0.0.1:{backend_port}"
    endpoint = f"http://127.0.0.1:{mcp_port}/mcp"
    bridge = secrets.token_hex(32)
    identities = {
        "read-only": (secrets.token_hex(32), ["mock:read"]),
        "replay-a": (secrets.token_hex(32), ["mock:read", "mock:replay"]),
        "replay-b": (secrets.token_hex(32), ["mock:read", "mock:replay"]),
        "draft-a": (secrets.token_hex(32), ["mock:read", "mock:plan:draft"]),
        "all": (secrets.token_hex(32), ["mock:read", "mock:plan:draft", "mock:replay", "mock:plan:validate", "mock:route:edit"]),
    }
    config = workspace / "backend.yaml"
    config.write_text(f"""server:
  http_port: {backend_port}
  ws_port: {ws_port}
  debug: false
storage:
  path: drones.json
  video_metadata_path: metadata
log:
  level: warn
""", encoding="utf-8")
    (workspace / "mock_execution.json").write_text(json.dumps({"simulation": True, "uavs": {
        "UAV-01": {"home_position": {"latitude": 0.10, "longitude": 0.1000, "altitude": 15}},
        "UAV-02": {"home_position": {"latitude": 0.10, "longitude": 0.1001, "altitude": 15}},
    }}), encoding="utf-8")
    (workspace / "drones.json").write_text("[]", encoding="utf-8")
    accounts = workspace / "identities.json"
    accounts.write_text(json.dumps({"identities": [
        {"id": name, "token_sha256": hashlib.sha256(token.encode()).hexdigest(), "scopes": scopes}
        for name, (token, scopes) in identities.items()
    ]}), encoding="utf-8")
    env = {**os.environ, "DRONE_MOCK_MCP_BRIDGE_TOKEN": bridge}
    env.pop("DRONE_PILOT_CONTROL_CREDENTIALS", None)
    fixtures = workspace / "map-fixtures.json"
    fixtures.write_text(json.dumps({"maps": {"map-qa": {
        "point-a": {"latitude": 0.1, "longitude": 0.1000, "altitude": 15},
        "point-b": {"latitude": 0.1, "longitude": 0.1001, "altitude": 15},
    }}}), encoding="utf-8")
    backend_command = [str(backend_exe), str(config)]
    mcp_command = [sys.executable, str(SERVER), "--identities", str(accounts), "--backend-port", str(backend_port),
                   "--fixtures", str(fixtures)]
    backend_process = start(backend_command, env, workspace, "backend.log")
    mcp_process = None
    try:
        mcp_process = start(mcp_command + ["--http-port", str(mcp_port)], env, workspace, "mcp.log")
        async with httpx.AsyncClient(trust_env=False, timeout=5) as http:
            await wait_ready(backend_process, http, backend + "/", 200)
            await wait_ready(mcp_process, http, endpoint, 401)
            response = await http.post(backend + "/api/mock-mcp/draft", json={})
            assert response.status_code == 403
            for name, scope in ((DRAFT_TOOL, "mock:plan:draft"), (REPLAY_TOOL, "mock:replay")):
                response = await http.post(endpoint, headers=rpc_headers(identities["read-only"][0], name),
                                           json=rpc(name, {}))
                assert response.status_code == 403 and scope in response.headers["WWW-Authenticate"]
            invalid = await http.post(endpoint, headers=rpc_headers(secrets.token_hex(32), DRAFT_TOOL),
                                      json=rpc(DRAFT_TOOL, {}))
            assert invalid.status_code == 401
            headers = rpc_headers(identities["draft-a"][0], DRAFT_TOOL)
            headers["Origin"] = "http://untrusted.invalid"
            response = await http.post(endpoint, headers=headers, json=rpc(DRAFT_TOOL, {}))
            assert response.status_code == 403, response.text
            check("HTTP authorization, scope enforcement, Origin and private bridge")

            stdio = StdioServerParameters(
                command=sys.executable, args=mcp_command[1:],
                env={"DRONE_MOCK_MCP_CLIENT_TOKEN": identities["read-only"][0]},
            )
            async with Client(stdio, mode="legacy", read_timeout_seconds=10) as reader:
                assert reader.session.protocol_version == "2025-11-25"
                schema = await listing(reader, READ_TOOLS)
                assert (await call(reader, schema, "list_demo_scenarios"))["scenarios"][0]["scenario_id"] == "demo-a"
                assert (await call(reader, schema, "get_sim_status", {"scenario_id": "demo-a"}))["replay"] is None
                denied = await reader.call_tool(DRAFT_TOOL, {})
                assert denied.is_error and denied.structured_content["error"]["code"] == "FORBIDDEN"
            check("stdio discovery and permission gate with the 2025-11-25 handshake")

            async with (
                Client(transport(endpoint, identities["replay-a"][0]), mode="2026-07-28") as replay,
                Client(transport(endpoint, identities["replay-b"][0]), mode="2026-07-28") as other,
                Client(transport(endpoint, identities["draft-a"][0]), mode="2026-07-28") as draft,
                Client(transport(endpoint, identities["all"][0]), mode="2026-07-28") as all_tools,
            ):
                readers = READ_TOOLS | WORKSPACE_TOOLS.keys()
                replay_tools = {REPLAY_TOOL, *RECORD_REPLAY_TOOLS}
                plan_tools = {DRAFT_TOOL, *(name for name in PLAN_WRITE_TOOLS if name != VALIDATE)}
                replay_schema = await listing(replay, readers | replay_tools)
                other_schema = await listing(other, readers | replay_tools)
                draft_schema = await listing(draft, readers | plan_tools)
                full_schema = await listing(all_tools, readers | plan_tools | {VALIDATE} | replay_tools | ROUTE_WRITE_TOOLS.keys())
                await call(replay, replay_schema, "list_demo_scenarios", {"unknown": 1}, "INVALID_ARGUMENT")
                await call(replay, replay_schema, "get_sim_status", {"scenario_id": "demo-a\n"},
                           "INVALID_ARGUMENT")
                unknown = await call(replay, replay_schema, "get_sim_status", {"scenario_id": "missing"},
                                     "SCENARIO_NOT_FOUND")
                assert unknown["simulation"] is True
                alert_args = {"scenario_id": "demo-a"}
                for severity, expected in (("warning", ["alert-a-01"]), ("critical", [])):
                    data = await call(replay, replay_schema, "list_sim_alerts", {**alert_args, "severity": severity})
                    assert [row["alert_id"] for row in data["alerts"]] == expected
                await call(replay, replay_schema, "list_sim_alerts", {**alert_args, "limit": 0}, "INVALID_ARGUMENT")
                check("five schemas, role-specific listing, fixed scenario status and alert filtering")

                denied_workspace = await http.get(backend + "/api/mock-mcp/workspace")
                assert denied_workspace.status_code == 403
                for name in ("list_mock_security_plans", "list_mock_executions", "list_mock_rehearsals",
                             "list_mock_replay_records", "list_mock_operation_events"):
                    page = await call(replay, replay_schema, name)
                    assert page["items"] == [] and page["next_cursor"] is None
                health = await call(replay, replay_schema, "get_mock_workspace_health")
                assert health["state"] == "ready" and health["workspace_id"] == "mock-local"
                for name in ("get_mock_workspace_context", "get_mock_workspace_preferences", "get_mock_video_view"):
                    value = await call(replay, replay_schema, name, {"session_id": "mock-session"})
                    assert value["session_id"] == "mock-session"
                    await call(replay, replay_schema, name, {"session_id": "other-session"}, "NOT_FOUND")
                devices = await call(replay, replay_schema, "list_mock_devices", {"kind": "test_fixture", "limit": 1})
                device_id = devices["items"][0]["device_id"]
                device = await call(replay, replay_schema, "get_mock_device_status", {"device_id": device_id})
                assert device["kind"] == "test_fixture" and device["position_fixture_id"] is None
                await call(replay, replay_schema, "get_mock_device_status", {"device_id": "outside"}, "NOT_FOUND")
                device_cursor = devices["next_cursor"]
                assert device_cursor
                next_devices = await call(replay, replay_schema, "list_mock_devices",
                                          {"kind": "test_fixture", "limit": 1, "cursor": device_cursor})
                assert next_devices["items"][0]["device_id"] != device_id
                await call(other, other_schema, "list_mock_devices",
                           {"kind": "test_fixture", "cursor": device_cursor}, "CURSOR_INVALID")
                await call(replay, replay_schema, "list_mock_devices",
                           {"kind": "simulated", "cursor": device_cursor}, "CURSOR_INVALID")
                check("authorized workspace and session reads, canonical device projection and identity-bound cursors")

                original = (await call(replay, replay_schema, "get_sim_status", alert_args))["replay"]
                write = {"replay_id": original["replay_id"], "state": "paused", "expected_version": 3}
                before_replay = (await http.get(backend + "/api/security-plans")).json()
                await call(other, other_schema, REPLAY_TOOL, write, "REPLAY_NOT_FOUND")
                paused = (await call(replay, replay_schema, REPLAY_TOOL, write))["replay"]
                assert paused["state"] == "paused" and paused["version"] == 4
                await call(replay, replay_schema, REPLAY_TOOL, write, "REVISION_CONFLICT")
                await asyncio.sleep(0.08)
                assert (await call(replay, replay_schema, "get_sim_status", alert_args))["replay"] == paused
                active = (await call(replay, replay_schema, REPLAY_TOOL,
                                     {**write, "state": "playing", "expected_version": 4}))["replay"]
                assert active["version"] == 5 and active["position_ms"] == paused["position_ms"]
                await asyncio.sleep(0.08)
                assert (await call(replay, replay_schema, "get_sim_status", alert_args))["replay"]["position_ms"] > active["position_ms"]
                assert (await http.get(backend + "/api/security-plans")).json()["executions"] == before_replay["executions"]
                check("replay ownership, pause stability, version conflicts and resume")

                args = {"request_id": str(uuid4()), "name": " " * 160 + "Mock acceptance draft "}
                await call(draft, draft_schema, DRAFT_TOOL, {**args, "name": "演" * 54}, "INVALID_ARGUMENT")
                command_id = str(uuid4())
                registration = await http.post(backend + "/api/context/clients", json={
                    "instance_id": command_id, "client_id": "MockMcpQA", "client_role": "Command",
                    "hostname": "SYNTHETIC-QA", "app_version": "MCP-QA",
                })
                registration.raise_for_status()
                async with connect(f"ws://127.0.0.1:{ws_port}/ws", proxy=None) as ws:
                    await ws.send(json.dumps({"type": "subscribe_context", "instance_id": command_id}))
                    async with asyncio.timeout(5):
                        while json.loads(await ws.recv()).get("type") != "context_subscribed":
                            pass

                    async def plan_action(action, **parameters):
                        response = await http.post(backend + "/api/security-plans", json={
                            "instance_id": command_id, "action": action, **parameters,
                        })
                        response.raise_for_status()
                        return response.json()["state"]

                    locked = await plan_action("begin_configuration")
                    await call(draft, draft_schema, DRAFT_TOOL, args, "CONFIGURATION_IN_PROGRESS")
                    await call(draft, draft_schema, DRAFT_TOOL, {**args, "description": "changed while blocked"},
                               "REQUEST_ID_CONFLICT")
                    after_conflict = (await http.get(backend + "/api/security-plans")).json()
                    assert after_conflict["plans"] == locked["plans"]
                    assert after_conflict["configuration_session"] == locked["configuration_session"]
                    await plan_action("end_configuration")
                    results = await asyncio.gather(*[
                        call(draft, draft_schema, DRAFT_TOOL, args) for _ in range(5)
                    ])
                    created = results[0]
                    assert all(item == created for item in results)
                    assert created["name"] == "Mock acceptance draft"
                    plan_id = created["plan_id"]
                    live = (await http.get(backend + "/api/security-plans")).json()
                    assert "mock_mcp_draft_receipts" not in live and live["configuration_session"] is None
                    assert len(live["plans"]) == len(locked["plans"]) + 1
                    assert live["plans"][plan_id]["status"] == "DRAFT"
                    assert live["plans"][plan_id]["mission_ids"] == []
                    assert live["executions"] == before_replay["executions"]
                    plan_args = {"plan_id": plan_id}
                    plan = await call(replay, replay_schema, "get_mock_security_plan", plan_args)
                    assert plan["configuration"]["state"] == "idle" and plan["missions"] == []
                    plan_page = await call(replay, replay_schema, "list_mock_security_plans", {"state": "draft"})
                    assert [item["plan_id"] for item in plan_page["items"]] == [plan_id]
                    deployments = await call(replay, replay_schema, "list_mock_plan_deployments", plan_args)
                    assert deployments["items"] == [] and deployments["plan_id"] == plan_id
                    events = await call(replay, replay_schema, "list_mock_operation_events",
                                        {"category": "plan", "object_id": plan_id})
                    assert any(row["code"] == "PLAN_CREATED" for row in events["items"])
                    async with asyncio.timeout(5):
                        while True:
                            message = json.loads(await ws.recv())
                            if message.get("type") == "SecurityPlansChanged" and plan_id in message["payload"]["plans"]:
                                assert "mock_mcp_draft_receipts" not in message["payload"]
                                break
                    handed_off = await plan_action("begin_configuration", plan_id=plan_id)
                    occupied = await call(replay, replay_schema, "get_mock_security_plan", plan_args)
                    assert occupied["configuration"] == {"state": "busy", "configuration_id": None, "lease_expires_at": None}
                    assert handed_off["configuration_session"]["owner_instance_id"] == command_id
                    assert await call(draft, draft_schema, DRAFT_TOOL, {**args, "description": ""}) == created
                    await call(draft, draft_schema, DRAFT_TOOL, {**args, "description": "different"}, "REQUEST_ID_CONFLICT")
                    assert (await http.get(backend + "/api/security-plans")).json()["configuration_session"] == handed_off["configuration_session"]
                    await plan_action("end_configuration", plan_id=plan_id)
                    other_draft = await call(all_tools, full_schema, DRAFT_TOOL, args)
                    assert other_draft["plan_id"] != plan_id
                check("configuration conflict, concurrent draft deduplication and Command protocol handoff")

                # Deliberately close the HTTP connection without reading the draft response.
                lost = {"request_id": str(uuid4()), "name": "Discarded response draft"}
                encoded = json.dumps(rpc(DRAFT_TOOL, lost)).encode("utf-8")
                request_headers = {
                    **rpc_headers(identities["draft-a"][0], DRAFT_TOOL),
                    "Host": f"127.0.0.1:{mcp_port}", "Content-Type": "application/json",
                    "Content-Length": str(len(encoded)), "Connection": "close",
                }
                _read, writer = await asyncio.open_connection("127.0.0.1", mcp_port)
                try:
                    writer.write(("POST /mcp HTTP/1.1\r\n" +
                                  "".join(f"{k}: {v}\r\n" for k, v in request_headers.items()) +
                                  "\r\n").encode("ascii") + encoded)
                    await writer.drain()
                    # Wait for durable visibility separately while leaving the response unread.
                    async with asyncio.timeout(5):
                        while True:
                            snapshot = (await http.get(backend + "/api/security-plans")).json()
                            matching = [row for row in snapshot["plans"].values() if row["name"] == lost["name"]]
                            if matching:
                                break
                            await asyncio.sleep(0.03)
                finally:
                    writer.close()
                    await writer.wait_closed()
                stop(backend_process)
                await call(draft, draft_schema, DRAFT_TOOL, args, "MOCK_UNAVAILABLE")
                backend_process = start(backend_command, env, workspace, "backend.log")
                await wait_ready(backend_process, http, backend + "/", 200)
                assert await call(draft, draft_schema, DRAFT_TOOL, args) == created
                restored = await call(draft, draft_schema, DRAFT_TOOL, lost)
                assert restored["plan_id"] == matching[0]["id"]
                final = (await http.get(backend + "/api/security-plans")).json()
                assert len(final["plans"]) == len(locked["plans"]) + 3
                assert final["configuration_session"] is None and final["executions"] == before_replay["executions"]
                check("unread response, Backend restart, stable durable receipt and no duplicate plan")

                def write_args(**props):
                    return {"request_id": str(uuid4()), **props}

                denied_write = await http.post(backend + "/api/mock-mcp/plan-write", json={})
                assert denied_write.status_code == 403
                forged_owner = await http.post(backend + "/api/context/clients", json={
                    "instance_id": "mock-mcp-all", "client_id": "Other", "client_role": "Command",
                    "hostname": "SYNTHETIC-QA", "app_version": "MCP-QA",
                })
                assert forged_owner.status_code == 403
                plan = await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": plan_id})
                begin_args = write_args(plan_id=plan_id, expected_plan_version=plan["plan_version"])
                begin = await call(all_tools, full_schema, "begin_mock_plan_edit", begin_args)
                assert await call(all_tools, full_schema, "begin_mock_plan_edit", begin_args) == begin
                await call(draft, draft_schema, "begin_mock_plan_edit", begin_args, "CONFIGURATION_IN_PROGRESS")
                owned = await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": plan_id})
                assert owned["configuration"]["configuration_id"] == begin["configuration_id"]
                stranger = await call(draft, draft_schema, "get_mock_security_plan", {"plan_id": plan_id})
                assert stranger["configuration"]["state"] == "busy" and stranger["configuration"]["configuration_id"] is None
                handle = {"plan_id": plan_id, "configuration_id": begin["configuration_id"]}
                updates = write_args(**handle, expected_plan_version=owned["plan_version"], description="Only edited by Mock MCP")
                updated = await call(all_tools, full_schema, "update_mock_security_plan_draft", updates)
                assert updated["name"] == plan["name"] and updated["description"] == updates["description"]
                await call(all_tools, full_schema, "update_mock_security_plan_draft", {**updates, "name": "Different"}, "REQUEST_ID_CONFLICT")
                await call(all_tools, full_schema, "update_mock_security_plan_draft",
                           {**updates, "request_id": str(uuid4())}, "VERSION_CONFLICT")
                add_args = write_args(**handle, expected_plan_version=updated["plan_version"], name="Synthetic QA task")
                added = await call(all_tools, full_schema, "add_mock_plan_mission", add_args)
                assert added["device_ids"] == [] and added["route_version"] == 0
                assert await call(all_tools, full_schema, "add_mock_plan_mission", add_args) == added
                task = {**handle, "mission_id": added["mission_id"]}
                rename = await call(all_tools, full_schema, "rename_mock_plan_mission",
                                    write_args(**task, expected_plan_version=added["plan_version"], name="Updated QA task"))
                assignment_args = write_args(**task, expected_plan_version=rename["plan_version"], device_ids=["UAV-01"])
                await call(all_tools, full_schema, "set_mock_mission_assignment", assignment_args, "NOT_FOUND")
                cleared = await call(all_tools, full_schema, "set_mock_mission_assignment", {**assignment_args, "device_ids": []})
                assert cleared["device_ids"] == []
                checked = await call(all_tools, full_schema, VALIDATE, write_args(**handle, expected_plan_version=cleared["plan_version"]))
                assert not checked["passed"] and any(issue["code"] == "NO_UAV_ASSIGNED" for issue in checked["checks"])
                end_args = write_args(configuration_id=handle["configuration_id"])
                route_key = {"plan_id": plan_id, "mission_id": added["mission_id"]}
                empty_route = await call(all_tools, full_schema, "get_mock_mission_route", route_key)
                assert empty_route["route_version"] == 0 and empty_route["route"] is None
                editing_args = write_args(**task, expected_route_version=0)
                editing = await call(all_tools, full_schema, BEGIN_ROUTE, editing_args)
                assert editing["route_version"] == 0
                assert await call(all_tools, full_schema, BEGIN_ROUTE, editing_args) == editing
                await call(all_tools, full_schema, "end_mock_plan_edit", end_args, "EDIT_IN_PROGRESS")
                save_args = write_args(route_edit_id=editing["route_edit_id"], expected_route_version=0,
                                       map_fixture_id="map-qa", waypoint_fixture_ids=["point-a", "point-b"])
                await call(all_tools, full_schema, SAVE_ROUTE, {**save_args, "waypoint_fixture_ids": ["foreign"]}, "FIXTURE_NOT_FOUND")
                route = await call(all_tools, full_schema, SAVE_ROUTE, save_args)
                assert route["edit_open"] and route["route_version"] == 1 and route["waypoint_count"] == 2
                assert await call(all_tools, full_schema, SAVE_ROUTE, {**save_args, "closed_loop": False}) == route
                read_route = await call(all_tools, full_schema, "get_mock_mission_route", route_key)
                assert read_route["route"]["waypoint_fixture_ids"] == save_args["waypoint_fixture_ids"]
                finish_args = write_args(route_edit_id=editing["route_edit_id"], disposition="finish", expected_route_version=1)
                await call(all_tools, full_schema, END_ROUTE, {**finish_args, "expected_route_version": 0}, "VERSION_CONFLICT")
                finished = await call(all_tools, full_schema, END_ROUTE, finish_args)
                assert not finished["edit_open"] and finished["route_version"] == 1
                checked = await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": plan_id})
                assert checked["configuration"]["configuration_id"] == handle["configuration_id"]
                check("Mock route fixture validation, saved version checks and parent lease isolation")
                await call(draft, draft_schema, "end_mock_plan_edit", end_args, "CONFIGURATION_EXPIRED")
                ended = await call(all_tools, full_schema, "end_mock_plan_edit", end_args)
                assert ended["released"] and await call(all_tools, full_schema, "end_mock_plan_edit", end_args) == ended
                await call(all_tools, full_schema, "reopen_mock_security_plan",
                           write_args(plan_id=plan_id, expected_plan_version=checked["plan_version"]), "INVALID_STATE")
                copy_args = write_args(source_plan_id=plan_id, expected_source_plan_version=checked["plan_version"])
                copied = await call(all_tools, full_schema, "copy_mock_security_plan_draft", copy_args)
                assert copied["plan_id"] != plan_id and copied["mission_count"] == 1
                copy_plan = await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": copied["plan_id"]})
                assert copy_plan["missions"][0]["mission_id"] != added["mission_id"]
                copy_route = await call(all_tools, full_schema, "get_mock_mission_route", {
                    "plan_id": copied["plan_id"], "mission_id": copy_plan["missions"][0]["mission_id"],
                })
                assert copy_route["route"]["route_id"] != read_route["route"]["route_id"]
                check("Mock plan lease ownership, version gates, edits, assignment exclusion and copy isolation")

                stop(backend_process)
                saved_path = workspace / "security_plans.json"
                saved = json.loads(saved_path.read_text(encoding="utf-8"))
                synthetic_run = {"execution_id": "execution-qa", "uav_id": "UAV-1001", "group_id": "group-qa",
                                 "state": "COMPLETED", "control_version": 3, "adapter": "Mock", "simulation": True,
                                 "created_at": 1795410200, "updated_at": 1795410260, "elapsed_seconds": 60,
                                 "progress": 1, "replay_samples": [{"elapsed_seconds": 0}, {"elapsed_seconds": 60}]}
                saved["executions"] = {synthetic_run["execution_id"]: synthetic_run,
                                       "outside-qa": {**synthetic_run, "execution_id": "outside-qa", "uav_id": "UAV-99",
                                                      "simulation": False, "adapter": "Real"}}
                saved_path.write_text(json.dumps(saved), encoding="utf-8")
                backend_process = start(backend_command, env, workspace, "backend.log")
                await wait_ready(backend_process, http, backend + "/", 200)
                assert await call(all_tools, full_schema, "copy_mock_security_plan_draft", copy_args) == copied
                assert await call(all_tools, full_schema, "add_mock_plan_mission", add_args) == added
                executions = await call(replay, replay_schema, "list_mock_executions")
                assert [item["execution_id"] for item in executions["items"]] == ["execution-qa"]
                await call(replay, replay_schema, "get_mock_execution", {"execution_id": "outside-qa"}, "NOT_FOUND")
                execution = await call(replay, replay_schema, "get_mock_execution", {"execution_id": "execution-qa"})
                assert execution["elapsed_ms"] == 60000 and execution["members"][0]["device_id"] == "UAV-1001"
                listing_args = {"execution_id": "execution-qa"}
                records = await call(replay, replay_schema, "list_mock_replay_records", listing_args)
                record = records["items"][0]
                assert record["duration_ms"] == 60000 and record["replay"] is None
                open_args = {"record_id": record["record_id"], "request_id": str(uuid4())}
                opened = await call(replay, replay_schema, OPEN, open_args)
                assert opened["replay"]["version"] == 1 and opened["replay"]["position_ms"] == 0
                assert await call(replay, replay_schema, OPEN, open_args) == opened
                assert (await call(other, other_schema, "list_mock_replay_records", listing_args))["items"][0]["replay"] is None
                seek = {"replay_id": opened["replay"]["replay_id"], "position_ms": 12000,
                        "expected_version": 1, "request_id": str(uuid4())}
                await call(other, other_schema, SEEK, seek, "REPLAY_NOT_FOUND")
                sought = await call(replay, replay_schema, SEEK, seek)
                assert sought["replay"]["version"] == 2 and sought["replay"]["position_ms"] == 12000
                await call(replay, replay_schema, SEEK, {**seek, "position_ms": 13000}, "REQUEST_ID_CONFLICT")
                await call(replay, replay_schema, SEEK, {**seek, "request_id": str(uuid4())}, "REVISION_CONFLICT")
                playing = await call(replay, replay_schema, REPLAY_TOOL,
                                     {"replay_id": seek["replay_id"], "expected_version": 2, "state": "playing"})
                assert playing["record_id"] == record["record_id"] and playing["replay"]["version"] == 3
                close = {"replay_id": seek["replay_id"], "expected_version": 3, "request_id": str(uuid4())}
                closed = await call(replay, replay_schema, CLOSE, close)
                assert closed["closed"] and await call(replay, replay_schema, CLOSE, close) == closed
                assert (await call(replay, replay_schema, "list_mock_replay_records", listing_args))["items"][0]["replay"] is None
                assert (await http.get(backend + "/api/security-plans")).json()["executions"] == saved["executions"]
                check("stored workspace playback, cross-identity rejection and no execution mutation")
    except Exception:
        for name in ("backend.log", "mcp.log"):
            path = workspace / name
            if path.exists():
                print(path.read_text(encoding="utf-8", errors="replace")[-5000:], file=sys.stderr)
        raise
    finally:
        if mcp_process:
            stop(mcp_process)
        stop(backend_process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", type=Path, required=True)
    args = parser.parse_args()
    executable = args.backend.resolve(strict=True)
    with TemporaryDirectory(prefix="drone-mock-mcp-") as workspace:
        asyncio.run(run(executable, Path(workspace)))


if __name__ == "__main__":
    main()
