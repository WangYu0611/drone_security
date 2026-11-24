"""Verify an MCP-created draft through the rendered Unreal Command controls."""

import argparse
import asyncio
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
from tempfile import TemporaryDirectory
from uuid import uuid4

import httpx
from mcp.client import Client

from verify import DRAFT_TOOL, SERVER, call, free_ports, start, stop, transport, wait_ready

ROOT = Path(__file__).resolve().parents[2]
TEST = "DroneOps.MockMcp.DraftUi"


async def read(http, origin, path):
    response = await http.get(origin + path)
    response.raise_for_status()
    return response.json()


async def watch_lease(http, origin, plan_id, instance, editor):
    first_lease = None
    while editor.poll() is None:
        plans = await read(http, origin, "/api/security-plans")
        lease = plans["configuration_session"]
        if lease is not None:
            assert lease["plan_id"] == plan_id and lease["owner_instance_id"] == instance, lease
            first_lease = lease
        await asyncio.sleep(0.02)
    return first_lease


async def run(args, workspace):
    http_port, ws_port, mcp_port = free_ports(3)
    origin = f"http://127.0.0.1:{http_port}"
    endpoint = f"http://127.0.0.1:{mcp_port}/mcp"
    token = secrets.token_hex(32)
    environment = dict(os.environ, DRONE_MOCK_MCP_BRIDGE_TOKEN=secrets.token_hex(32))
    environment.pop("DRONE_PILOT_CONTROL_CREDENTIALS", None)
    config = workspace / "backend.yaml"
    config.write_text(f"server:\n  http_port: {http_port}\n  ws_port: {ws_port}\n  debug: false\n"
                      "storage:\n  path: drones.json\n  video_metadata_path: metadata\nlog:\n  level: warn\n",
                      encoding="utf-8")
    (workspace / "mock_execution.json").write_text('{"simulation":true}', encoding="utf-8")
    (workspace / "drones.json").write_text("[]", encoding="utf-8")
    identities = workspace / "identities.json"
    identities.write_text(json.dumps({"identities": [{
        "id": "mock-mcp-ui-qa", "scopes": ["mock:read", "mock:plan:draft"],
        "token_sha256": hashlib.sha256(token.encode()).hexdigest(),
    }]}), encoding="utf-8")

    with ExitStack() as processes:
        backend = start([str(args.backend), str(config)], environment, workspace, "backend.log")
        processes.callback(stop, backend)
        mcp = start([sys.executable, str(SERVER), "--identities", str(identities),
                     "--backend-port", str(http_port), "--http-port", str(mcp_port)],
                    environment, workspace, "mcp.log")
        processes.callback(stop, mcp)
        async with httpx.AsyncClient(trust_env=False) as http:
            try:
                await wait_ready(backend, http, origin + "/", 200)
                await wait_ready(mcp, http, endpoint, 401)
            except RuntimeError:
                for logfile in ("backend.log", "mcp.log"):
                    print((workspace / logfile).read_text(encoding="utf-8", errors="replace")[-2000:], file=sys.stderr)
                raise
            name = f"Mock MCP UI QA {uuid4().hex[:12]}"
            async with Client(transport(endpoint, token), mode="2026-07-28") as client:
                schema = {tool.name: tool for tool in (await client.list_tools()).tools}
                receipt = await call(client, schema, DRAFT_TOOL, {"request_id": str(uuid4()), "name": name})
            assert receipt["simulation"] is True and receipt["state"] == "draft", receipt
            plan_id = receipt["plan_id"]
            before = await read(http, origin, "/api/security-plans")
            assert set(before["plans"]) == {plan_id} and before["configuration_session"] is None

            instance = str(uuid4())
            report_directory = workspace / "report"
            editor_log = workspace / "editor.log"
            editor = subprocess.Popen([
                str(args.editor), str(ROOT / "UE5DroneControl.uproject"), "/Game/Tests/P55OfflineMap",
                "-P55OfflineVisualQA", "-unattended", "-nosplash", "-NoSound", "-NoLiveCoding",
                "-DisablePlugins=SimpleWebSocket", "-ClientRole=Command", f"-P5Instance={instance}",
                f"-P1Http={origin}", f"-P1Ws=ws://127.0.0.1:{ws_port}/ws", f"-UserDir={workspace / 'UE'}",
                f"-MockMcpDraftId={plan_id}", f"-MockMcpDraftName={name}", f"-MockMcpEvidence={args.evidence}",
                f"-ExecCmds=t.MaxFPS 30,Automation RunTests {TEST}", "-TestExit=Automation Test Queue Empty",
                f"-ReportExportPath={report_directory}", f"-abslog={editor_log}",
            ], cwd=workspace, env=environment, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
            processes.callback(stop, editor)
            try:
                async with asyncio.timeout(args.timeout):
                    lease = await watch_lease(http, origin, plan_id, instance, editor)
                report = json.loads((report_directory / "index.json").read_text(encoding="utf-8-sig"))
                tests = report["tests"]
                assert editor.returncode == 0 and len(tests) == 1, report
                assert tests[0]["fullTestPath"] == TEST and tests[0]["state"] == "Success", tests
                assert lease is not None, "Backend never observed the editor acquire the draft lease"
                after = await read(http, origin, "/api/security-plans")
                context = await read(http, origin, "/api/context")
                assert context["active_security_plan_id"] == plan_id, context
                assert after["configuration_session"] is None, after["configuration_session"]
                assert set(after["plans"]) == {plan_id} and after["plans"][plan_id]["status"] == "DRAFT"
                assert after["plans"][plan_id]["mission_ids"] == [] and after["executions"] == before["executions"] == {}
                screenshots = ["01-opened-and-leased.png", "02-finished-and-released.png"]
                for image in screenshots:
                    assert (args.evidence / image).stat().st_size > 0
                summary = {"plan_id": plan_id, "name": name, "lease_plan_id": lease["plan_id"],
                           "lease_owner_is_command": lease["owner_instance_id"] == instance,
                           "lease_released": True, "missions": 0, "executions": 0,
                           "input": "rendered Slate hit testing and pointer routing", "screenshots": screenshots}
                (args.evidence / "result.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
                print(json.dumps(summary, ensure_ascii=False), flush=True)
            except Exception:
                if editor_log.exists():
                    print(editor_log.read_text(encoding="utf-8", errors="replace")[-7000:], file=sys.stderr)
                raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", required=True, type=Path)
    parser.add_argument("--editor", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path, help="New directory for screenshots and result.json")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    args.editor = args.editor.resolve(strict=True)
    args.backend = args.backend.resolve(strict=True)
    args.evidence = args.evidence.resolve()
    args.evidence.mkdir(parents=True)
    with TemporaryDirectory(prefix="drone-mock-mcp-ui-") as workspace:
        asyncio.run(run(args, Path(workspace)))


if __name__ == "__main__":
    main()
