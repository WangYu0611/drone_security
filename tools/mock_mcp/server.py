"""MCP entry point for the isolated Drone Security demo workspace."""

import argparse
import asyncio
from dataclasses import dataclass, field
import hashlib
import json
import os
from pathlib import Path
import re
import time
from uuid import uuid4

import httpx
from jsonschema import Draft202012Validator
from mcp import types
from mcp.server.lowlevel import Server
from mcp.server.stdio import stdio_server
from mcp.shared.exceptions import MCPError
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.responses import JSONResponse
import uvicorn

from plan_writes import APPROVAL_SCOPES, APPROVAL_TOOLS, PLAN_WRITE_TOOLS, ROUTE_WRITE_TOOLS, SIMULATION_SCOPES
from plan_writes import SIMULATION_TOOLS, VALIDATE, PlanWriter, load_fixtures
from protocol import COUNTER, ID, ID_PATTERN, STRING, TIMESTAMP, UUID, ToolFailure, object_schema, tool, utc_timestamp
from record_replay import RECORD_REPLAY_TOOLS, RecordReplayStore
from workspace import WORKSPACE_TOOLS, WorkspaceReader

READ = "mock:read"
REPLAY = "mock:replay"
DRAFT = "mock:plan:draft"
VALIDATION = "mock:plan:validate"
ROUTE_EDIT = "mock:route:edit"
BACKEND_WRITES = PLAN_WRITE_TOOLS | ROUTE_WRITE_TOOLS | APPROVAL_TOOLS | SIMULATION_TOOLS
TOKEN_PATTERN = r"[a-f0-9]{64}"
MAX_REQUEST = 8192
DEMO_ID = "demo-a"
DEMO_TITLE = "Demo A"
ALERTS = (
    {
        "alert_id": "alert-a-01", "severity": "warning",
        "code": "DEVICE_OFFLINE", "occurred_at": "2026-11-23T04:59:50Z",
    },
)


REPLAY_STATE = {"enum": ["playing", "paused"]}
SEVERITY = {"enum": ["info", "warning", "critical"]}
REPLAY_VALUE = object_schema({
    "replay_id": ID, "state": REPLAY_STATE, "position_ms": COUNTER, "version": COUNTER,
})
SCENARIO = object_schema({"scenario_id": ID, "title": STRING})


TOOLS = {
    item.name: item for item in (
        tool("list_demo_scenarios", "List fixed synthetic demo recordings.", {},
             {"scenarios": {"type": "array", "items": SCENARIO}}),
        tool("get_sim_status", "Get a synthetic recording and your demo playback status.",
             {"scenario_id": ID}, {
                 "scenario_id": ID,
                 "plan": {"anyOf": [object_schema({
                     "plan_id": ID, "title": STRING, "state": {"enum": ["draft", "active"]},
                 }), {"type": "null"}]},
                 "tasks": object_schema({"total": COUNTER, "completed": COUNTER}),
                 "devices": object_schema({"total": COUNTER, "online": COUNTER}),
                 "replay": {"anyOf": [REPLAY_VALUE, {"type": "null"}]},
             }),
        tool("list_sim_alerts", "List recorded alerts from a synthetic demo.",
             {"scenario_id": ID, "severity": SEVERITY,
              "limit": {"type": "integer", "minimum": 1, "maximum": 50, "default": 20}},
             {"scenario_id": ID, "alerts": {"type": "array", "items": object_schema({
                 "alert_id": ID, "severity": SEVERITY, "code": STRING, "occurred_at": TIMESTAMP,
             })}}, ("severity", "limit")),
        tool("set_replay_state", "Play or pause your synthetic demo replay.",
             {"replay_id": ID, "state": REPLAY_STATE, "expected_version": COUNTER},
             {"scenario_id": ID, "replay": REPLAY_VALUE}, read_only=False,
             result_alternatives=({"record_id": ID, "replay": REPLAY_VALUE},)),
        tool("create_security_plan_draft", "Save an empty-task draft in the isolated Mock Backend.",
             {"request_id": UUID, "name": {"type": "string", "minLength": 1},
              "description": {"type": "string", "maxLength": 1000, "default": ""}},
             {"request_id": UUID, "plan_id": ID, "name": STRING, "state": {"const": "draft"}},
             ("description",), read_only=False),
    )
}
TOOLS.update(WORKSPACE_TOOLS)
TOOLS.update(RECORD_REPLAY_TOOLS)
TOOLS.update(BACKEND_WRITES)
TOOL_SCOPE = {**dict.fromkeys(RECORD_REPLAY_TOOLS, REPLAY), **dict.fromkeys(PLAN_WRITE_TOOLS, DRAFT),
              **dict.fromkeys(ROUTE_WRITE_TOOLS, ROUTE_EDIT), **APPROVAL_SCOPES, **SIMULATION_SCOPES,
              "set_replay_state": REPLAY, "create_security_plan_draft": DRAFT, VALIDATE: VALIDATION}
INPUT_CHECKS = {name: Draft202012Validator(info.input_schema) for name, info in TOOLS.items()}


@dataclass(frozen=True)
class Identity:
    name: str
    scopes: frozenset[str]

    def allows(self, tool_name):
        return READ in self.scopes and TOOL_SCOPE.get(tool_name, READ) in self.scopes


def load_identities(path):
    accounts = {}
    identities = set()
    for row in json.loads(path.read_text(encoding="utf-8"))["identities"]:
        identity, digest, scopes = row["id"], row["token_sha256"], frozenset(row["scopes"])
        if (not re.fullmatch(ID_PATTERN, identity) or not re.fullmatch(TOKEN_PATTERN, digest)
                or identity in identities or digest in accounts or READ not in scopes
                or not scopes <= {READ, *TOOL_SCOPE.values()}):
            raise ValueError("Invalid or duplicate MCP identity")
        accounts[digest] = Identity(identity, scopes)
        identities.add(identity)
    if not accounts:
        raise ValueError("At least one MCP identity is required")
    return accounts


def authenticate(accounts, token):
    if not re.fullmatch(TOKEN_PATTERN, token):
        return None
    return accounts.get(hashlib.sha256(token.encode("ascii")).hexdigest())


def reply(payload, *, error=False, observed_at=None):
    structured = {
        "contract_version": "0.2", "simulation": True,
        "observed_at": observed_at or utc_timestamp(), **payload,
    }
    return types.CallToolResult(
        content=[types.TextContent(type="text", text=json.dumps(structured, ensure_ascii=False))],
        structured_content=structured, is_error=error,
    )


@dataclass
class Replay:
    replay_id: str = field(default_factory=lambda: "rpl-" + uuid4().hex)
    state: str = "playing"
    version: int = 3
    position_ms: int = 40000
    changed_at: float = field(default_factory=time.monotonic)

    def position(self, now):
        if self.state == "playing":
            return self.position_ms + int((now - self.changed_at) * 1000)
        return self.position_ms

    def snapshot(self, now):
        return {
            "replay_id": self.replay_id, "state": self.state,
            "position_ms": self.position(now), "version": self.version,
        }

    def set_state(self, target, version, now):
        if version != self.version:
            raise ToolFailure("REVISION_CONFLICT", "Read the current replay state and version")
        if self.state != target:
            self.position_ms = self.position(now)
            self.changed_at = now
            self.state = target
            self.version += 1
        return self.snapshot(now)


class MockTools:
    def __init__(self, backend_port, bridge_token, workspace_id, session_id, state_db, fixtures):
        self.draft_endpoint = f"http://127.0.0.1:{backend_port}/api/mock-mcp/draft"
        self.bridge_token = bridge_token
        self.workspace = WorkspaceReader(backend_port, bridge_token, workspace_id, session_id) if bridge_token else None
        self.plan_writer = PlanWriter(backend_port, bridge_token, fixtures) if bridge_token else None
        self.record_replays = RecordReplayStore(state_db, workspace_id) if self.workspace and state_db else None
        self.replays = {}

    def available(self, name):
        if name in RECORD_REPLAY_TOOLS:
            return self.record_replays is not None
        if name in BACKEND_WRITES:
            return self.plan_writer is not None
        return name not in WORKSPACE_TOOLS or self.workspace is not None

    async def workspace_replay(self, identity, name, arguments):
        if not self.record_replays:
            raise ToolFailure("MOCK_UNAVAILABLE", "The workspace recorder is not configured")
        records = await self.workspace.records()
        if name == "set_replay_state":
            payload, observed_at = self.record_replays.set_state(identity.name, arguments, records)
        else:
            payload, observed_at = self.record_replays.write(identity.name, name, arguments, records)
        return reply(payload, observed_at=observed_at)

    def replay(self, identity):
        if REPLAY not in identity.scopes:
            return None
        if identity.name not in self.replays:
            self.replays[identity.name] = Replay()
        return self.replays[identity.name]

    async def create_draft(self, identity, arguments):
        name = arguments["name"].strip(" \t\r\n\f\v")
        description = arguments.get("description", "")
        try:
            name_bytes = len(name.encode("utf-8"))
            description_bytes = len(description.encode("utf-8"))
        except UnicodeError as error:
            raise ToolFailure("INVALID_ARGUMENT", "Use valid UTF-8 text") from error
        if not 1 <= name_bytes <= 160 or description_bytes > 1000:
            raise ToolFailure("INVALID_ARGUMENT", "Name must be 1–160 UTF-8 bytes and description at most 1,000")
        try:
            async with httpx.AsyncClient(timeout=5, trust_env=False) as client:
                response = await client.post(
                    self.draft_endpoint, headers={"Authorization": f"Bearer {self.bridge_token}"},
                    json={"identity": identity.name, "request_id": arguments["request_id"],
                          "name": name, "description": description},
                )
            payload = response.json()
            if response.status_code == 200:
                observed_at = utc_timestamp(payload.pop("created_at"))
                return reply(payload, observed_at=observed_at)
            code = payload.get("code")
            if code in {"CONFIGURATION_IN_PROGRESS", "REQUEST_ID_CONFLICT"}:
                raise ToolFailure(code, payload["message"])
            if response.status_code == 400:
                raise ToolFailure("INVALID_ARGUMENT", "Check the draft arguments")
        except (httpx.HTTPError, ValueError, TypeError, KeyError, AttributeError):
            pass
        raise ToolFailure("MOCK_UNAVAILABLE", "Mock Backend unavailable; retry with the same request_id")

    async def call(self, identity, name, arguments):
        if name not in TOOLS:
            raise MCPError(-32602, "Unknown tool")
        if not identity.allows(name):
            return reply({"error": {"code": "FORBIDDEN", "message": "Missing tool scope"}}, error=True)
        try:
            if not INPUT_CHECKS[name].is_valid(arguments):
                raise ToolFailure("INVALID_ARGUMENT", "Arguments do not match the tool input schema")
            if name in WORKSPACE_TOOLS:
                if not self.workspace:
                    raise ToolFailure("MOCK_UNAVAILABLE", "This process has no authorized Mock Backend bridge")
                replays = (self.record_replays.list_owned(identity.name) if name == "list_mock_replay_records" and
                           self.record_replays and REPLAY in identity.scopes else None)
                payload, observed_at = await self.workspace.call(identity, name, arguments, replays)
                return reply(payload, observed_at=observed_at)
            if name in RECORD_REPLAY_TOOLS or (name == "set_replay_state" and arguments["replay_id"].startswith("record-replay-")):
                return await self.workspace_replay(identity, name, arguments)
            if name in BACKEND_WRITES:
                if not self.plan_writer:
                    raise ToolFailure("MOCK_UNAVAILABLE", "This process has no authorized Mock Backend bridge")
                payload, observed_at = await self.plan_writer.call(identity, name, arguments)
                return reply(payload, observed_at=observed_at)
            if name == "create_security_plan_draft":
                return await self.create_draft(identity, arguments)
            if arguments.get("scenario_id", DEMO_ID) != DEMO_ID:
                raise ToolFailure("SCENARIO_NOT_FOUND", "Choose an accessible demo scenario")
            if name == "list_demo_scenarios":
                return reply({"scenarios": [{"scenario_id": DEMO_ID, "title": DEMO_TITLE}]})
            if name == "list_sim_alerts":
                severity = arguments.get("severity")
                alerts = [alert for alert in ALERTS if severity is None or alert["severity"] == severity]
                return reply({"scenario_id": DEMO_ID, "alerts": alerts[:int(arguments.get("limit", 20))]})
            replay = self.replay(identity)
            now = time.monotonic()
            if name == "set_replay_state":
                if replay is None or arguments["replay_id"] != replay.replay_id:
                    raise ToolFailure("REPLAY_NOT_FOUND", "Read your own demo replay session")
                state = replay.set_state(arguments["state"], arguments["expected_version"], now)
                return reply({"scenario_id": DEMO_ID, "replay": state})
            return reply({
                "scenario_id": DEMO_ID, "plan": {"plan_id": "plan-a", "title": DEMO_TITLE, "state": "active"},
                "tasks": {"total": 2, "completed": 1}, "devices": {"total": 2, "online": 1},
                "replay": replay.snapshot(now) if replay else None,
            })
        except ToolFailure as error:
            return reply({"error": {"code": error.code, "message": str(error)}}, error=True)


def make_server(service, stdio_identity=None):
    def caller(context):
        if stdio_identity is not None:
            return stdio_identity
        return context.request.state.mock_identity

    async def list_tools(context, _params):
        identity = caller(context)
        return types.ListToolsResult(
            tools=[info for name, info in TOOLS.items() if identity.allows(name) and service.available(name)], ttl_ms=0,
        )

    async def call_tool(context, params):
        arguments = params.arguments if params.arguments is not None else {}
        return await service.call(caller(context), params.name, arguments)

    return Server("Drone Security Mock", version="0.2", on_list_tools=list_tools, on_call_tool=call_tool)


def make_http_app(service, accounts):
    app = make_server(service).streamable_http_app(
        stateless_http=True, json_response=True, max_request_body_size=MAX_REQUEST,
    )

    class MockAuthorization(BaseHTTPMiddleware):
        async def dispatch(self, request, call_next):
            if request.url.path != "/mcp":
                return await call_next(request)
            scheme, _, token = request.headers.get("authorization", "").partition(" ")
            identity = authenticate(accounts, token) if scheme.lower() == "bearer" else None
            if identity is None:
                return JSONResponse(
                    {"error": "invalid_token"}, status_code=401,
                    headers={"WWW-Authenticate": 'Bearer error="invalid_token"'},
                )
            request.state.mock_identity = identity
            if request.method == "POST":
                # Require a bounded body before inspecting a direct tool call.
                # Requests from the SDK and stdlib HTTP clients include Content-Length.
                try:
                    length = int(request.headers.get("content-length", ""))
                except ValueError:
                    return JSONResponse({"error": "content_length_required"}, status_code=411)
                if not 0 <= length <= MAX_REQUEST or "transfer-encoding" in request.headers:
                    return JSONResponse({"error": "request_too_large"}, status_code=413)
                try:
                    message = json.loads(await request.body())
                except (ValueError, UnicodeError, RecursionError):
                    return await call_next(request)
                if isinstance(message, dict) and message.get("method") == "tools/call":
                    params = message.get("params")
                    name = params.get("name") if isinstance(params, dict) else None
                    if isinstance(name, str) and name in TOOLS and not identity.allows(name):
                        return JSONResponse(
                            {"error": "insufficient_scope"}, status_code=403,
                            headers={"WWW-Authenticate": f'Bearer error="insufficient_scope", scope="{TOOL_SCOPE[name]}"'},
                        )
            return await call_next(request)

    app.add_middleware(MockAuthorization)
    return app


async def run_stdio(server):
    async with stdio_server() as (read, write):
        await server.run(read, write, server.create_initialization_options())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identities", required=True, type=Path)
    parser.add_argument("--backend-port", required=True, type=int)
    parser.add_argument("--http-port", type=int, help="Use localhost Streamable HTTP instead of stdio")
    parser.add_argument("--workspace-id", default="mock-local", help="ID of the isolated workspace assigned to these identities")
    parser.add_argument("--session-id", default="mock-session", help="ID of the shared demo session assigned to these identities")
    parser.add_argument("--state-db", type=Path, help="Workspace-local playback database (defaults next to identities)")
    parser.add_argument("--fixtures", type=Path, help="Administrator-approved synthetic fixture JSON")
    args = parser.parse_args()
    for port in (args.backend_port, args.http_port):
        if port is not None and not 1 <= port <= 65535:
            parser.error("ports must be between 1 and 65535")
    if not all(re.fullmatch(ID_PATTERN, value) for value in (args.workspace_id, args.session_id)):
        parser.error("workspace and session IDs must match the tool ID format")
    try:
        accounts = load_identities(args.identities)
    except (OSError, ValueError, KeyError, TypeError):
        parser.error("could not load the local MCP identities file")
    try:
        fixtures = load_fixtures(args.fixtures)
    except (OSError, ValueError, KeyError, TypeError):
        parser.error("could not load the local synthetic fixtures")
    identity = None
    if args.http_port is None:
        identity = authenticate(accounts, os.environ.get("DRONE_MOCK_MCP_CLIENT_TOKEN", ""))
        if identity is None:
            parser.error("set DRONE_MOCK_MCP_CLIENT_TOKEN to a configured identity token")
    token = os.environ.get("DRONE_MOCK_MCP_BRIDGE_TOKEN", "")
    if token or identity is None or (identity.scopes - {READ, REPLAY}):
        if not re.fullmatch(TOKEN_PATTERN, token):
            parser.error("set a 32-byte hex DRONE_MOCK_MCP_BRIDGE_TOKEN")
        if hashlib.sha256(token.encode("ascii")).hexdigest() in accounts:
            parser.error("bridge token must be distinct from MCP identity tokens")
    state_db = args.state_db or args.identities.with_name("mock-mcp-replay.sqlite3")
    service = MockTools(args.backend_port, token, args.workspace_id, args.session_id, state_db, fixtures)
    if args.http_port is not None:
        uvicorn.run(make_http_app(service, accounts), host="127.0.0.1", port=args.http_port, workers=1)
    else:
        asyncio.run(run_stdio(make_server(service, identity)))


if __name__ == "__main__":
    main()
