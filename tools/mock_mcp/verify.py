"""Run Mock MCP protocol acceptance against an owned, isolated Backend."""

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

from approve import save_approval
from operator_queue import GET_HANDOFF, LIST_HANDOFF, OPERATOR_SCOPES, OPERATOR_TOOLS
from operator_queue import REQUEST_HANDOFF, REQUEST_RELEASE, REQUEST_RESET, RESET_SCOPE
from plan_writes import APPROVAL_SCOPES, APPROVAL_TOOLS, BEGIN_ROUTE, DELETE_MISSION, DELETE_PLAN, DEPLOY, END_ROUTE
from plan_writes import CREATE_DEVICES, PLAN_WRITE_TOOLS, REMOVE_DEVICE, REVIEW, REVIEW_REHEARSAL, ROUTE_WRITE_TOOLS
from plan_writes import SAVE_ROUTE, SIMULATION_SCOPES, SIMULATION_TOOLS, START_REHEARSAL, UPDATE_DEVICE, VALIDATE
from plan_writes import CANCEL_SCHEDULED, EXECUTION_SCOPES, EXECUTION_TOOLS, REQUEST_EXECUTION
from plan_writes import RESCHEDULE, SET_EXECUTION, START_SCHEDULED
from plan_writes import CANCEL_TRANSLATION, COMMIT_TRANSLATION, PREVIEW_TRANSLATION
from plan_writes import CANCEL_FLEET, CONFIRM_FLEET, CONFIRM_TAKEOVER, FLEET_SCOPES, FLEET_TOOLS, PREVIEW_FLEET, PREVIEW_TAKEOVER
from protocol import utc_timestamp
from record_replay import CLOSE, OPEN, SEEK, RECORD_REPLAY_TOOLS
from session_alerts import ACK_ALERT, ALERT_SCOPE, LIST_ALERTS
from workspace import LANGUAGE, SELECT, VIDEO, VIEW_SCOPES, VIEW_TOOLS, WORKSPACE_TOOLS

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
        "all": (secrets.token_hex(32), ["mock:read", "mock:plan:draft", "mock:replay", "mock:plan:validate", "mock:route:edit",
                                       ALERT_SCOPE, *sorted(set(APPROVAL_SCOPES.values()) | set(SIMULATION_SCOPES.values()) |
                                               set(VIEW_SCOPES.values()) | set(EXECUTION_SCOPES.values()) |
                                               set(FLEET_SCOPES.values()) | set(OPERATOR_SCOPES.values()))]),
        "schedule-only": (secrets.token_hex(32), ["mock:read", EXECUTION_SCOPES[RESCHEDULE]]),
        "reset-without-role": (secrets.token_hex(32), ["mock:read", RESET_SCOPE]),
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
    startup = {"direction": "east", "spacing_m": 20,
               "origin": {"latitude": 0.1, "longitude": 0.1001, "altitude": 15}}
    (workspace / "mock_execution.json").write_text(json.dumps({"simulation": True, "startup_formation": startup, "uavs": {
        "UAV-01": {"home_position": {"latitude": 0.10, "longitude": 0.1000, "altitude": 15}},
        "UAV-02": {"home_position": {"latitude": 0.10, "longitude": 0.1001, "altitude": 15}},
    }}), encoding="utf-8")
    (workspace / "drones.json").write_text(json.dumps([{"id": "2", "slot": 2, "name": "Synthetic operator QA"}]),
                                          encoding="utf-8")
    accounts = workspace / "identities.json"
    accounts.write_text(json.dumps({"identities": [
        {"id": name, "token_sha256": hashlib.sha256(token.encode()).hexdigest(), "scopes": scopes,
         "role": "test_admin" if name == "all" else "client"}
        for name, (token, scopes) in identities.items()
    ]}), encoding="utf-8")
    env = {**os.environ, "DRONE_MOCK_MCP_BRIDGE_TOKEN": bridge}
    command_token, pilot_token = secrets.token_hex(32), secrets.token_hex(32)
    pilot_credentials = workspace / "pilot-qa.json"
    pilot_credentials.write_text(json.dumps({"credentials": [
        {"id": "qa-command", "role": "Command", "token": command_token},
        {"id": "qa-pilot", "role": "Pilot", "token": pilot_token},
    ]}), encoding="utf-8")
    env["DRONE_PILOT_CONTROL_CREDENTIALS"] = str(pilot_credentials)
    fixtures = workspace / "map-fixtures.json"
    fixtures.write_text(json.dumps({"maps": {"map-qa": {
        "point-a": {"latitude": 0.1, "longitude": 0.1000, "altitude": 15},
        "point-b": {"latitude": 0.1, "longitude": 0.1001, "altitude": 15},
        "point-c": {"latitude": 0.1, "longitude": 0.0995, "altitude": 15},
        "shift-a": {"latitude": 0.1002, "longitude": 0.1000, "altitude": 15},
        "shift-b": {"latitude": 0.1002, "longitude": 0.1001, "altitude": 15},
    }}, "devices": {"setup-qa": {"latitude": 0.1, "longitude": 0.1, "altitude": 15}},
        "video_sources": {"video-qa": {"device_id": "UAV-01", "title": "QA clip", "availability": "available"}},
        "session_alerts": {"session-qa": {"device_id": "UAV-02", "severity": "warning", "code": "DEVICE_OFFLINE",
                                          "occurred_at": "2026-11-23T07:00:00Z"}},
        "translations": {
            key: {"map_fixture_id": "map-qa", "replacements": {
                f"point-{suffix}": {"target_id": f"shift-{suffix}", "before_location": {"x": 0, "y": y, "z": 0},
                                    "after_location": {"x": 2200, "y": y, "z": 0}} for suffix, y in members}}
            for key, members in (("shift-qa", (("a", 0), ("b", 1113))), ("shift-blocked", (("a", 0),)))
        },
        "motions": {"shift-demo": {"map_fixture_id": "map-qa", "waypoint_fixture_id": "shift-a"},
                    "single-demo": {"map_fixture_id": "map-qa", "waypoint_fixture_id": "point-c"}},
    }), encoding="utf-8")
    backend_command = [str(backend_exe), str(config)]
    mcp_command = [sys.executable, str(SERVER), "--identities", str(accounts), "--backend-port", str(backend_port),
                   "--fixtures", str(fixtures)]
    backend_process = start(backend_command, env, workspace, "backend.log")
    mcp_process = None
    try:
        mcp_process = start(mcp_command + ["--dedicated-workspace", "--http-port", str(mcp_port)], env, workspace, "mcp.log")
        async with httpx.AsyncClient(trust_env=False, timeout=5) as http:
            await wait_ready(backend_process, http, backend + "/", 200)
            await wait_ready(mcp_process, http, endpoint, 401)
            response = await http.post(backend + "/api/mock-mcp/draft", json={})
            assert response.status_code == 403
            for name, scope in ((DRAFT_TOOL, "mock:plan:draft"), (REPLAY_TOOL, "mock:replay"),
                                (ACK_ALERT, ALERT_SCOPE), *VIEW_SCOPES.items(), *EXECUTION_SCOPES.items(),
                                *FLEET_SCOPES.items(), *OPERATOR_SCOPES.items()):
                response = await http.post(endpoint, headers=rpc_headers(identities["read-only"][0], name),
                                           json=rpc(name, {}))
                assert response.status_code == 403 and scope in response.headers["WWW-Authenticate"]
            invalid = await http.post(endpoint, headers=rpc_headers(secrets.token_hex(32), DRAFT_TOOL),
                                      json=rpc(DRAFT_TOOL, {}))
            assert invalid.status_code == 401
            reset_headers = rpc_headers(identities["reset-without-role"][0], REQUEST_RESET)
            forbidden = await http.post(endpoint, headers=reset_headers, json=rpc(REQUEST_RESET, {}))
            assert forbidden.status_code == 403
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
                readers = READ_TOOLS | WORKSPACE_TOOLS.keys() | {LIST_ALERTS, LIST_HANDOFF, GET_HANDOFF}
                replay_tools = {REPLAY_TOOL, *RECORD_REPLAY_TOOLS}
                plan_tools = {DRAFT_TOOL, *(name for name in PLAN_WRITE_TOOLS if name != VALIDATE)}
                replay_schema = await listing(replay, readers | replay_tools)
                other_schema = await listing(other, readers | replay_tools)
                draft_schema = await listing(draft, readers | plan_tools)
                full_schema = await listing(all_tools, readers | plan_tools | {VALIDATE} | replay_tools |
                                            ROUTE_WRITE_TOOLS.keys() | APPROVAL_TOOLS.keys() | SIMULATION_TOOLS.keys() |
                                            VIEW_TOOLS.keys() | EXECUTION_TOOLS.keys() | FLEET_TOOLS.keys() |
                                            OPERATOR_TOOLS.keys() | {ACK_ALERT})
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

                session = {"session_id": "mock-session"}
                language = {**session, "expected_version": 0, "language": "zh-Hans", "request_id": str(uuid4())}
                changed_language = await call(all_tools, full_schema, LANGUAGE, language)
                assert changed_language["version"] == 1
                assert (await http.get(backend + "/api/ui-preferences")).json()["language"] == "zh-Hans"
                assert (await call(replay, replay_schema, "get_mock_workspace_preferences", session))["language"] == "zh-Hans"
                await call(all_tools, full_schema, LANGUAGE, {**language, "language": "en"}, "REQUEST_ID_CONFLICT")
                await call(all_tools, full_schema, LANGUAGE,
                           {**language, "request_id": str(uuid4())}, "VERSION_CONFLICT")
                await call(all_tools, full_schema, LANGUAGE, {**language, "session_id": "foreign"}, "SESSION_EXPIRED")

                selected_args = {**session, "expected_version": 0, "plan_id": None, "mission_id": None,
                                 "device_ids": ["UAV-01", "UAV-02"], "primary_device_id": "UAV-02",
                                 "request_id": str(uuid4())}
                await call(all_tools, full_schema, SELECT, {**selected_args, "primary_device_id": "outside"},
                           "INVALID_ARGUMENT")
                selected = await call(all_tools, full_schema, SELECT, selected_args)
                assert selected["version"] == 1 and selected["video_target_device_id"] is None
                context = await call(replay, replay_schema, "get_mock_workspace_context", session)
                assert context["primary_device_id"] == "UAV-02" and context["device_ids"] == ["UAV-01", "UAV-02"]
                sources = await call(replay, replay_schema, "list_mock_video_sources", {"device_id": "UAV-01"})
                assert sources["items"] == [{"source_id": "video-qa", "device_id": "UAV-01",
                                             "title": "QA clip", "availability": "available"}]
                assert (await call(replay, replay_schema, "list_mock_video_sources", {"device_id": "UAV-02"}))["items"] == []
                video = {**session, "expected_version": 0, "device_id": "UAV-01", "request_id": str(uuid4())}
                await call(all_tools, full_schema, VIDEO, {**video, "device_id": "UAV-02", "source_id": "video-qa"},
                           "INVALID_ARGUMENT")
                first = await call(all_tools, full_schema, VIDEO, video)
                assert first["source_id"] == "video-qa" and first["playback_state"] == "unreported"
                second = await call(all_tools, full_schema, VIDEO,
                                    {**video, "expected_version": 1, "device_id": "UAV-02", "request_id": str(uuid4())})
                assert second["source_id"] is None and second["playback_state"] == "no_source"
                view = await call(replay, replay_schema, "get_mock_video_view", session)
                assert view["target_device_id"] == "UAV-02" and view["playback_observed_at"] is None
                cleared = await call(all_tools, full_schema, SELECT,
                                     {**selected_args, "expected_version": 1, "device_ids": [],
                                      "primary_device_id": None, "request_id": str(uuid4())})
                assert cleared["device_ids"] == [] and cleared["video_target_device_id"] == "UAV-02"
                check("versioned selection, isolated shared language, fixture-only video and independent view state")

                alert_query = {**session, "handled": False, "severity": "warning"}
                fixed_alerts = (await call(replay, replay_schema, "list_sim_alerts", alert_args))["alerts"]
                pending = await call(replay, replay_schema, LIST_ALERTS, alert_query)
                assert [row["alert_id"] for row in pending["items"]] == ["session-qa"]
                acknowledgment = {**session, "alert_id": "session-qa", "expected_version": 1,
                                  "request_id": str(uuid4())}
                missing = {**acknowledgment, "alert_id": "foreign"}
                await call(all_tools, full_schema, ACK_ALERT, missing, "NOT_FOUND")
                ack = await call(all_tools, full_schema, ACK_ALERT, acknowledgment)
                assert ack["handled"] and ack["version"] == 2
                assert await call(all_tools, full_schema, ACK_ALERT, acknowledgment) == ack
                assert (await call(replay, replay_schema, LIST_ALERTS, alert_query))["items"] == []
                await call(all_tools, full_schema, ACK_ALERT, {**acknowledgment, "request_id": str(uuid4())},
                           "VERSION_CONFLICT")
                await call(all_tools, full_schema, ACK_ALERT, {**acknowledgment, "session_id": "elsewhere"},
                           "SESSION_EXPIRED")
                assert (await call(replay, replay_schema, "list_sim_alerts", alert_args))["alerts"] == fixed_alerts
                other_port, = free_ports(1)
                second_gateway = start(mcp_command + ["--session-id", "other-session", "--http-port", str(other_port)],
                                       env, workspace, "mcp-second.log")
                second_endpoint = f"http://127.0.0.1:{other_port}/mcp"
                try:
                    await wait_ready(second_gateway, http, second_endpoint, 401)
                    async with Client(transport(second_endpoint, identities["replay-a"][0]), mode="2026-07-28") as second_session:
                        different = await call(second_session, replay_schema, LIST_ALERTS,
                                               {**alert_query, "session_id": "other-session"})
                        assert different["items"] == pending["items"]
                finally:
                    stop(second_gateway)
                check("session alert acknowledgment, replay fixture preservation and per-session isolation")

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
                assert await call(all_tools, full_schema, LANGUAGE, language) == changed_language
                assert await call(all_tools, full_schema, VIDEO, video) == first
                assert await call(all_tools, full_schema, SELECT, selected_args) == selected
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
                assignment_args = write_args(**task, expected_plan_version=rename["plan_version"], device_ids=["UAV-99"])
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

                def confirmed(name, params, owner="all"):
                    props = {key: value for key, value in params.items() if key != "request_id"}
                    return {**params, "approval_id": save_approval(workspace, owner, name, props, 120)}

                copy_id = {"plan_id": copied["plan_id"]}
                copy_task = {**copy_id, "mission_id": copy_plan["missions"][0]["mission_id"]}
                move_lease = await call(all_tools, full_schema, "begin_mock_plan_edit",
                                        write_args(**copy_id, expected_plan_version=copy_plan["plan_version"]))
                moving = {**copy_id, "configuration_id": move_lease["configuration_id"],
                          "expected_plan_version": move_lease["plan_version"]}
                await call(all_tools, full_schema, PREVIEW_TRANSLATION,
                           write_args(**moving, translation_fixture_id="foreign"), "FIXTURE_NOT_FOUND")
                blocked = await call(all_tools, full_schema, PREVIEW_TRANSLATION,
                                     write_args(**moving, translation_fixture_id="shift-blocked"))
                assert blocked["state"] == "blocked" and blocked["checks"][0]["code"] == "FIXTURE_NOT_FOUND", blocked
                cancel = write_args(preview_id=blocked["preview_id"])
                await call(all_tools, full_schema, COMMIT_TRANSLATION, cancel, "PREVIEW_BLOCKED")
                canceled_preview = await call(all_tools, full_schema, CANCEL_TRANSLATION, cancel)
                assert await call(all_tools, full_schema, CANCEL_TRANSLATION, cancel) == canceled_preview
                preview_args = write_args(**moving, translation_fixture_id="shift-qa")
                preview = await call(all_tools, full_schema, PREVIEW_TRANSLATION, preview_args)
                assert preview["state"] == "ready" and preview["affected_mission_ids"] == [copy_task["mission_id"]], preview
                assert await call(all_tools, full_schema, PREVIEW_TRANSLATION, preview_args) == preview
                original_route = await call(all_tools, full_schema, "get_mock_mission_route", copy_task)
                assert original_route["route_version"] == copy_route["route_version"]
                apply_move = write_args(preview_id=preview["preview_id"])
                await call(all_tools, full_schema, COMMIT_TRANSLATION, apply_move, "APPROVAL_REQUIRED")
                await call(all_tools, full_schema, COMMIT_TRANSLATION,
                           confirmed(COMMIT_TRANSLATION, apply_move, "foreign"), "APPROVAL_INVALID")
                moved = await call(all_tools, full_schema, COMMIT_TRANSLATION, confirmed(COMMIT_TRANSLATION, apply_move))
                assert moved["state"] == "committed"
                assert await call(all_tools, full_schema, COMMIT_TRANSLATION, apply_move) == moved
                await call(all_tools, full_schema, CANCEL_TRANSLATION, apply_move, "INVALID_STATE")
                shifted = await call(all_tools, full_schema, "get_mock_mission_route", copy_task)
                assert shifted["route"]["waypoint_fixture_ids"] == ["shift-a", "shift-b"]
                assert shifted["route_version"] == original_route["route_version"] + 1
                await call(all_tools, full_schema, "end_mock_plan_edit",
                           write_args(configuration_id=moving["configuration_id"]))
                copy_plan = await call(all_tools, full_schema, "get_mock_security_plan", copy_id)
                check("Mock plan translation preview isolation, blocked or canceled paths, atomic approved commit")

                delete_args = write_args(plan_id=copied["plan_id"], expected_plan_version=copy_plan["plan_version"])
                await call(all_tools, full_schema, DELETE_PLAN, delete_args, "APPROVAL_REQUIRED")
                await call(all_tools, full_schema, DELETE_PLAN, confirmed(DELETE_PLAN, delete_args, "other"), "APPROVAL_INVALID")
                approved_delete = confirmed(DELETE_PLAN, delete_args)
                deleted = await call(all_tools, full_schema, DELETE_PLAN, approved_delete)
                assert deleted["deleted"] and await call(all_tools, full_schema, DELETE_PLAN, delete_args) == deleted
                await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": copied["plan_id"]}, "NOT_FOUND")

                new_lease = await call(all_tools, full_schema, "begin_mock_plan_edit",
                                       write_args(plan_id=plan_id, expected_plan_version=checked["plan_version"]))
                reopened_handle = {"plan_id": plan_id, "configuration_id": new_lease["configuration_id"]}
                device_args = write_args(device_fixture_id="setup-qa", count=1)
                await call(all_tools, full_schema, CREATE_DEVICES, {**device_args, "device_fixture_id": "outside"}, "FIXTURE_NOT_FOUND")
                device_batch = await call(all_tools, full_schema, CREATE_DEVICES, device_args)
                assert device_batch["created_count"] == 1 and await call(all_tools, full_schema, CREATE_DEVICES, device_args) == device_batch
                simulator = device_batch["devices"][0]
                label_args = write_args(device_id=simulator["device_id"], expected_version=simulator["version"], label="QA display")
                labeled = await call(all_tools, full_schema, UPDATE_DEVICE, label_args)
                assert labeled["label"] == "QA display" and labeled["version"] > simulator["version"]
                assigned = await call(all_tools, full_schema, "set_mock_mission_assignment",
                                      write_args(**reopened_handle, mission_id=added["mission_id"],
                                                 expected_plan_version=new_lease["plan_version"], device_ids=[simulator["device_id"]]))
                passed = await call(all_tools, full_schema, VALIDATE,
                                    write_args(**reopened_handle, expected_plan_version=assigned["plan_version"]))
                assert passed["passed"] and passed["state"] == "ready", passed
                review_args = write_args(**reopened_handle, validation_id=passed["validation_id"],
                                         expected_plan_version=passed["plan_version"])
                review = await call(all_tools, full_schema, REVIEW, confirmed(REVIEW, review_args))
                deploy_args = write_args(**reopened_handle, review_id=review["review_id"],
                                         expected_plan_version=review["plan_version"])
                await call(all_tools, full_schema, DEPLOY, deploy_args, "APPROVAL_REQUIRED")
                deployed = await call(all_tools, full_schema, DEPLOY, confirmed(DEPLOY, deploy_args))
                assert deployed["state"] == "deployed" and deployed["configuration_released"]
                assert await call(all_tools, full_schema, DEPLOY, deploy_args) == deployed
                deployments = await call(all_tools, full_schema, "list_mock_plan_deployments", {"plan_id": plan_id})
                assert [row["deployment_id"] for row in deployments["items"]] == [deployed["deployment_id"]]
                assert (await http.get(backend + "/api/security-plans")).json()["executions"] == {}
                run_args = write_args(plan_id=plan_id, mission_id=added["mission_id"],
                                      expected_content_revision=deployed["content_revision"])
                started = await call(all_tools, full_schema, START_REHEARSAL, run_args)
                assert await call(all_tools, full_schema, START_REHEARSAL, run_args) == started
                async with asyncio.timeout(18):
                    while True:
                        rehearsal = await call(all_tools, full_schema, "get_mock_rehearsal", {"rehearsal_id": started["rehearsal_id"]})
                        if rehearsal["state"] == "completed":
                            break
                        assert rehearsal["state"] not in {"failed", "aborted", "cancelled"}, rehearsal
                        await asyncio.sleep(0.15)
                assert rehearsal["input_content_revision"] == deployed["content_revision"] and rehearsal["record_id"]
                rehearsal_args = write_args(rehearsal_id=started["rehearsal_id"], expected_version=rehearsal["version"])
                await call(all_tools, full_schema, REVIEW_REHEARSAL, rehearsal_args, "APPROVAL_REQUIRED")
                reviewed = await call(all_tools, full_schema, REVIEW_REHEARSAL, confirmed(REVIEW_REHEARSAL, rehearsal_args))
                assert reviewed["review_state"] == "approved"
                remove_args = write_args(device_id=simulator["device_id"], expected_version=labeled["version"])
                device_approval = confirmed(REMOVE_DEVICE, remove_args)
                await call(all_tools, full_schema, REMOVE_DEVICE, device_approval, "OBJECT_IN_USE")
                reopened = await call(all_tools, full_schema, "reopen_mock_security_plan",
                                      write_args(plan_id=plan_id, expected_plan_version=deployed["plan_version"]))
                assert reopened["previous_deployment_id"] == deployed["deployment_id"]
                last_lease = await call(all_tools, full_schema, "begin_mock_plan_edit",
                                        write_args(plan_id=plan_id, expected_plan_version=reopened["plan_version"]))
                remove_task = write_args(plan_id=plan_id, configuration_id=last_lease["configuration_id"],
                                         expected_plan_version=reopened["plan_version"], mission_id=added["mission_id"])
                removed = await call(all_tools, full_schema, DELETE_MISSION, confirmed(DELETE_MISSION, remove_task))
                assert removed["deleted"]
                await call(all_tools, full_schema, "end_mock_plan_edit", write_args(configuration_id=last_lease["configuration_id"]))
                assert (await call(all_tools, full_schema, "list_mock_plan_deployments", {"plan_id": plan_id}))["items"] == deployments["items"]
                check("operator approvals, isolated deployment, draft removal and immutable deployment history")
                removed_device = await call(all_tools, full_schema, REMOVE_DEVICE, device_approval)
                assert removed_device["removed"] and await call(all_tools, full_schema, REMOVE_DEVICE, remove_args) == removed_device
                await call(all_tools, full_schema, "get_mock_device_status", {"device_id": simulator["device_id"]}, "NOT_FOUND")
                check("simulator fixture creation, metadata, completed rehearsal and operator review")

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

                simulated_plan = await call(all_tools, full_schema, DRAFT_TOOL, write_args(name="Scheduled scenario"))
                sim_handle = await call(all_tools, full_schema, "begin_mock_plan_edit",
                                        write_args(plan_id=simulated_plan["plan_id"], expected_plan_version=1))
                owner = {key: sim_handle[key] for key in ("configuration_id", "plan_id")}
                mission = await call(all_tools, full_schema, "add_mock_plan_mission",
                                     write_args(**owner, name="Isolated run", expected_plan_version=sim_handle["plan_version"]))
                sim_task = {**owner, "mission_id": mission["mission_id"]}
                await call(all_tools, full_schema, "set_mock_mission_assignment",
                           write_args(**sim_task, expected_plan_version=mission["plan_version"], device_ids=["UAV-01"]))
                sim_edit = await call(all_tools, full_schema, BEGIN_ROUTE, write_args(**sim_task, expected_route_version=0))
                sim_route = await call(all_tools, full_schema, SAVE_ROUTE,
                                       write_args(route_edit_id=sim_edit["route_edit_id"], expected_route_version=0,
                                                  map_fixture_id="map-qa", waypoint_fixture_ids=["point-a", "point-c"]))
                await call(all_tools, full_schema, END_ROUTE,
                           write_args(route_edit_id=sim_edit["route_edit_id"], expected_route_version=sim_route["route_version"],
                                      disposition="finish"))
                current = await call(all_tools, full_schema, "get_mock_security_plan", {"plan_id": owner["plan_id"]})
                checked_sim = await call(all_tools, full_schema, VALIDATE,
                                         write_args(**owner, expected_plan_version=current["plan_version"]))
                assert checked_sim["passed"], checked_sim
                review_sim = write_args(**owner, expected_plan_version=checked_sim["plan_version"],
                                       validation_id=checked_sim["validation_id"])
                accepted = await call(all_tools, full_schema, REVIEW, confirmed(REVIEW, review_sim))
                publish_sim = write_args(**owner, expected_plan_version=accepted["plan_version"], review_id=accepted["review_id"])
                published = await call(all_tools, full_schema, DEPLOY, confirmed(DEPLOY, publish_sim))
                scheduled_time = utc_timestamp(time.time() + 600)[:19] + "Z"
                execution_args = write_args(deployment_id=published["deployment_id"], mission_id=mission["mission_id"],
                                            mode="scheduled", scheduled_for=scheduled_time)
                await call(all_tools, full_schema, REQUEST_EXECUTION, execution_args, "APPROVAL_REQUIRED")
                reservation = await call(all_tools, full_schema, REQUEST_EXECUTION, confirmed(REQUEST_EXECUTION, execution_args))
                assert reservation["state"] == "scheduled" and reservation["version"] == 1
                assert await call(all_tools, full_schema, REQUEST_EXECUTION, execution_args) == reservation
                run_id = {"execution_id": reservation["execution_id"]}
                await call(all_tools, full_schema, RESCHEDULE, write_args(**run_id, expected_version=0,
                                                                         scheduled_for=scheduled_time), "VERSION_CONFLICT")
                async with Client(transport(endpoint, identities["schedule-only"][0]), mode="2026-07-28") as scheduler:
                    await listing(scheduler, readers | {RESCHEDULE, CANCEL_SCHEDULED})
                    headers = rpc_headers(identities["schedule-only"][0], START_SCHEDULED)
                    response = await http.post(endpoint, headers=headers, json=rpc(START_SCHEDULED, {}))
                    assert response.status_code == 403
                revised_time = utc_timestamp(time.time() + 750)[:19] + "Z"
                revised = await call(all_tools, full_schema, RESCHEDULE,
                                     write_args(**run_id, expected_version=reservation["version"], scheduled_for=revised_time))
                assert revised["execution_id"] == reservation["execution_id"] and revised["state"] == "scheduled"
                canceled = await call(all_tools, full_schema, CANCEL_SCHEDULED,
                                      write_args(**run_id, expected_version=revised["version"]))
                assert canceled["state"] == "canceled"
                assert (await call(all_tools, full_schema, "get_mock_execution", run_id))["state"] == "canceled"
                second_args = {**execution_args, "request_id": str(uuid4())}
                second_run = await call(all_tools, full_schema, REQUEST_EXECUTION, confirmed(REQUEST_EXECUTION, second_args))
                assert second_run["execution_id"] != reservation["execution_id"]
                active_id = {"execution_id": second_run["execution_id"]}
                kick = write_args(**active_id, expected_version=second_run["version"])
                await call(all_tools, full_schema, START_SCHEDULED, kick, "APPROVAL_REQUIRED")
                kicked = await call(all_tools, full_schema, START_SCHEDULED, confirmed(START_SCHEDULED, kick))
                assert kicked["state"] == "starting" and kicked["scheduled_for"] is None
                async with asyncio.timeout(8):
                    while True:
                        active_run = await call(all_tools, full_schema, "get_mock_execution", active_id)
                        if active_run["state"] == "executing":
                            break
                        assert active_run["state"] not in {"failed", "aborted", "completed"}, active_run
                        await asyncio.sleep(0.05)
                paused_sim = await call(all_tools, full_schema, SET_EXECUTION,
                                        write_args(**active_id, expected_version=active_run["version"], action="pause"))
                assert paused_sim["state"] == "paused"
                resumed = await call(all_tools, full_schema, SET_EXECUTION,
                                     write_args(**active_id, expected_version=paused_sim["version"], action="resume"))
                assert resumed["state"] == "executing"
                abort = write_args(**active_id, expected_version=resumed["version"], action="abort")
                await call(all_tools, full_schema, SET_EXECUTION, abort, "APPROVAL_REQUIRED")
                approved_abort = await call(all_tools, full_schema, SET_EXECUTION, confirmed(SET_EXECUTION, abort))
                assert approved_abort["state"] == "aborted"
                assert await call(all_tools, full_schema, START_SCHEDULED, kick) == kicked
                assert await call(all_tools, full_schema, SET_EXECUTION, abort) == approved_abort
                execution_events = await call(all_tools, full_schema, "list_mock_operation_events",
                                              {"category": "execution", "object_id": active_id["execution_id"]})
                assert "MISSION_ABORTED" in {row["code"] for row in execution_events["items"]}
                check("Mock execution reservation, ownership, separate scopes, immediate start and control receipts")

                idle_snapshot = (await http.get(backend + "/api/security-plans")).json()["executions"]
                formation = write_args(device_ids=["UAV-01", "UAV-02"], motion_fixture_id="shift-demo")
                await call(all_tools, full_schema, PREVIEW_FLEET, {**formation, "motion_fixture_id": "foreign"},
                           "FIXTURE_NOT_FOUND")
                team = await call(all_tools, full_schema, PREVIEW_FLEET, formation)
                assert team["state"] == "ready" and team["device_ids"] == formation["device_ids"], team
                assert (await http.get(backend + "/api/security-plans")).json()["executions"] == idle_snapshot
                delayed = await call(all_tools, full_schema, PREVIEW_FLEET, {**formation, "request_id": str(uuid4())})
                disposable = await call(all_tools, full_schema, PREVIEW_FLEET,
                                        write_args(device_ids=["UAV-01"], motion_fixture_id="single-demo"))
                cancel_team = write_args(preview_id=disposable["preview_id"])
                canceled_team = await call(all_tools, full_schema, CANCEL_FLEET, cancel_team)
                assert await call(all_tools, full_schema, CANCEL_FLEET, cancel_team) == canceled_team
                team_args = write_args(preview_id=team["preview_id"])
                await call(all_tools, full_schema, CONFIRM_FLEET, team_args, "APPROVAL_REQUIRED")
                team_run = await call(all_tools, full_schema, CONFIRM_FLEET, confirmed(CONFIRM_FLEET, team_args))
                assert team_run["state"] == "starting" and team_run["version"] == 1
                assert await call(all_tools, full_schema, CONFIRM_FLEET, team_args) == team_run
                late = write_args(preview_id=delayed["preview_id"])
                await call(all_tools, full_schema, CONFIRM_FLEET, confirmed(CONFIRM_FLEET, late), "PREVIEW_STALE")
                team_key = {"execution_id": team_run["execution_id"]}
                team_live = await call(all_tools, full_schema, "get_mock_execution", team_key)
                assert team_live["kind"] == "fleet_move"
                assert {member["device_id"] for member in team_live["members"]} == {"UAV-01", "UAV-02"}
                end_team = write_args(**team_key, expected_version=team_live["version"], action="abort")
                for _ in range(6):
                    try:
                        await call(all_tools, full_schema, SET_EXECUTION, confirmed(SET_EXECUTION, end_team))
                        break
                    except AssertionError:
                        team_live = await call(all_tools, full_schema, "get_mock_execution", team_key)
                        end_team = write_args(**team_key, expected_version=team_live["version"], action="abort")
                else:
                    raise AssertionError("Could not stop the checked fleet simulation")
                direct = write_args(deployment_id=published["deployment_id"], mission_id=mission["mission_id"], mode="now")
                direct_run = await call(all_tools, full_schema, REQUEST_EXECUTION, confirmed(REQUEST_EXECUTION, direct))
                direct_key = {"execution_id": direct_run["execution_id"]}
                async with asyncio.timeout(8):
                    while True:
                        source = await call(all_tools, full_schema, "get_mock_execution", direct_key)
                        if source["state"] == "executing":
                            break
                        assert source["state"] not in {"failed", "aborted", "completed"}, source
                        await asyncio.sleep(0.05)
                handover_args = write_args(**direct_key, device_id="UAV-01", motion_fixture_id="single-demo",
                                           expected_execution_version=source["version"])
                await call(all_tools, full_schema, PREVIEW_TAKEOVER, {**handover_args, "expected_execution_version": 0},
                           "VERSION_CONFLICT")
                handover = await call(all_tools, full_schema, PREVIEW_TAKEOVER, handover_args)
                assert handover["state"] == "ready" and handover["affected_device_id"] == "UAV-01", handover
                assert (await call(all_tools, full_schema, "get_mock_execution", direct_key))["state"] == "executing"
                replace_args = write_args(preview_id=handover["preview_id"])
                replacement = await call(all_tools, full_schema, CONFIRM_TAKEOVER, confirmed(CONFIRM_TAKEOVER, replace_args))
                assert replacement["source_execution_id"] == direct_key["execution_id"] and replacement["device_id"] == "UAV-01"
                assert (await call(all_tools, full_schema, "get_mock_execution", direct_key))["state"] == "aborted"
                child = await call(all_tools, full_schema, "get_mock_execution", {"execution_id": replacement["execution_id"]})
                assert child["kind"] == "single_takeover" and child["execution_id"] != direct_key["execution_id"]
                check("fixture fleet preview and approval, stale confirmation, and software-only single-device transition")

                async def operator(*args, response):
                    command = [sys.executable, str(SERVER.with_name("operator_queue.py")), "--state-db",
                               str(accounts.with_name("mock-mcp-replay.sqlite3")), "--workspace", "mock-local", *args]
                    completed = await asyncio.to_thread(subprocess.run, command, input=response + "\n", text=True,
                                                        capture_output=True, env=env, timeout=10)
                    assert completed.returncode == 0, completed.stderr
                    return completed.stdout

                current_health = await call(all_tools, full_schema, "get_mock_workspace_health")
                restore_request = write_args(workspace_id="mock-local", reason="QA fixture restore",
                                             expected_workspace_version=current_health["version"])
                await call(all_tools, full_schema, REQUEST_RESET, restore_request, "INVALID_STATE")
                child_key = {"execution_id": child["execution_id"]}
                async with asyncio.timeout(20):
                    while (await call(all_tools, full_schema, "get_mock_execution", child_key))["state"] not in {
                            "completed", "failed", "aborted"}:
                        await asyncio.sleep(0.08)
                aircraft = {"device_id": "UAV-02"}
                initial_authority = await call(all_tools, full_schema, GET_HANDOFF, aircraft)
                assert initial_authority["authority_state"] == "unassigned" and initial_authority["latest_request"] is None
                handoff_request = write_args(**aircraft, expected_version=initial_authority["version"], note="Review requested")
                await call(all_tools, full_schema, REQUEST_RELEASE, handoff_request, "INVALID_STATE")
                requested = await call(all_tools, full_schema, REQUEST_HANDOFF, handoff_request)
                assert await call(all_tools, full_schema, REQUEST_HANDOFF, handoff_request) == requested
                assert (await call(all_tools, full_schema, GET_HANDOFF, aircraft))["authority_state"] == "unassigned"
                pending_handoff = await call(replay, replay_schema, LIST_HANDOFF, {"state": "pending_operator", **aircraft})
                assert [row["handoff_request_id"] for row in pending_handoff["items"]] == [requested["handoff_request_id"]]
                await call(all_tools, full_schema, REQUEST_HANDOFF, {**handoff_request, "request_id": str(uuid4())},
                           "OBJECT_IN_USE")
                await operator("review", requested["handoff_request_id"], response="APPROVE")
                approved_handoff = await call(all_tools, full_schema, GET_HANDOFF, aircraft)
                assert approved_handoff["latest_request"]["state"] == "approved"
                assert approved_handoff["authority_state"] == "unassigned"
                command_headers = {"Authorization": f"Bearer {command_token}"}
                async with connect(f"ws://127.0.0.1:{ws_port}/ws/pilot-control",
                                   additional_headers={"Authorization": f"Bearer {pilot_token}"}) as native_pilot:
                    await native_pilot.send(json.dumps({"type": "subscribe_pilot_control"}))
                    subscribed = json.loads(await asyncio.wait_for(native_pilot.recv(), timeout=5))
                    assert subscribed["type"] == "pilot_control_subscribed"
                    snapshot = (await http.get(backend + "/api/pilot-control", headers=command_headers)).json()
                    native_row = next(row for row in snapshot["aircraft"] if row["uav_id"] == aircraft["device_id"])
                    grant = await http.post(backend + "/api/pilot-control", headers=command_headers, json=write_args(
                        action="grant", pilot_id="qa-pilot", uav_ids=[aircraft["device_id"]],
                        expected_revisions={aircraft["device_id"]: native_row["revision"]}))
                    assert grant.status_code == 200 and grant.json()["results"][0]["ok"], grant.text
                    assigned = await call(all_tools, full_schema, GET_HANDOFF, aircraft)
                    assert assigned["authority_state"] == "assigned" and assigned["session_state"] == "inactive"
                    assert assigned["latest_request"]["state"] == "completed"
                    release_args = write_args(**aircraft, expected_version=assigned["version"])
                    release = await call(all_tools, full_schema, REQUEST_RELEASE, release_args)
                    assert (await call(all_tools, full_schema, GET_HANDOFF, aircraft))["authority_state"] == "assigned"
                    await operator("review", release["handoff_request_id"], response="APPROVE")
                    revoke = await http.post(backend + "/api/pilot-control", headers=command_headers, json=write_args(
                        action="revoke", uav_id=aircraft["device_id"], grant_id=grant.json()["results"][0]["grant_id"]))
                    assert revoke.status_code == 200 and revoke.json()["results"][0]["ok"], revoke.text
                    released = await call(all_tools, full_schema, GET_HANDOFF, aircraft)
                    assert released["authority_state"] == "unassigned" and released["latest_request"]["state"] == "completed"
                check("Pilot requests and local operator decisions with authoritative grant and release readback")

                pre_restore = (await http.get(backend + "/api/security-plans")).json()
                current_health = await call(all_tools, full_schema, "get_mock_workspace_health")
                restore_request["expected_workspace_version"] = current_health["version"]
                pending_reset = await call(all_tools, full_schema, REQUEST_RESET, restore_request)
                assert (await http.get(backend + "/api/security-plans")).json() == pre_restore
                assert await call(all_tools, full_schema, REQUEST_RESET, restore_request) == pending_reset
                reset_id = pending_reset["reset_request_id"]
                unauth_reset = await http.post(backend + "/api/mock-executions/reset", json={
                    "request_id": reset_id, "require_idle": True, "simulation": True,
                })
                assert unauth_reset.status_code == 403
                await operator("review", reset_id, response="APPROVE")
                await operator("apply-reset", reset_id, "--backend-port", str(backend_port), response="RESTORE")
                restored_health = await call(all_tools, full_schema, "get_mock_workspace_health")
                assert restored_health["reset_request"] == {"reset_request_id": reset_id, "state": "completed"}
                assert await call(all_tools, full_schema, REQUEST_RESET, restore_request) == pending_reset
                after_reset = (await http.get(backend + "/api/security-plans")).json()
                assert after_reset["mock_position_baseline"]["request_id"] == reset_id
                assert after_reset["plans"] == pre_restore["plans"] and after_reset["deployments"] == pre_restore["deployments"]
                after_id = write_args(workspace_id="mock-local", reason="Old version", expected_workspace_version=current_health["version"])
                await call(all_tools, full_schema, REQUEST_RESET, after_id, "VERSION_CONFLICT")
                check("dedicated workspace reset queue, idle and admin guards, human restore and replay-stable receipt")
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
