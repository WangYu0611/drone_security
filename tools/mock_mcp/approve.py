"""Issue a short-lived approval in an operator-owned Mock workspace."""

import argparse
import json
import os
from pathlib import Path
import re
import time
from uuid import uuid4

from jsonschema import Draft202012Validator

from plan_writes import APPROVAL_TOOLS, COMMIT_TRANSLATION, EXECUTION_TOOLS, REQUEST_EXECUTION
from plan_writes import ROUTE_WRITE_TOOLS, SET_EXECUTION, START_SCHEDULED
from protocol import ID_PATTERN


def save_approval(workspace, identity, tool, arguments, ttl):
    entry = {"identity": identity, "tool": tool, "arguments": arguments,
             "expires_at": time.time() + ttl}
    approval_id = "approval-" + uuid4().hex
    directory = workspace / "mock_mcp_approvals"
    directory.mkdir(exist_ok=True)
    pending = directory / f"{approval_id}.pending"
    with pending.open("x", encoding="utf-8") as file:
        json.dump(entry, file, ensure_ascii=False)
        file.flush()
        os.fsync(file.fileno())
    pending.replace(directory / f"{approval_id}.json")
    return approval_id


def main():
    tools = APPROVAL_TOOLS | {name: EXECUTION_TOOLS[name] for name in (REQUEST_EXECUTION, START_SCHEDULED, SET_EXECUTION)}
    tools[COMMIT_TRANSLATION] = ROUTE_WRITE_TOOLS[COMMIT_TRANSLATION]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--identity", required=True)
    parser.add_argument("--tool", required=True, choices=sorted(tools))
    parser.add_argument("--arguments", required=True, type=Path, help="JSON arguments without request_id or approval_id")
    parser.add_argument("--ttl", type=int, default=180)
    args = parser.parse_args()
    if not re.fullmatch(ID_PATTERN, args.identity) or not 1 <= args.ttl <= 300:
        parser.error("Use a configured identity ID and a lifetime of 1–300 seconds")
    workspace = args.workspace.resolve(strict=True)
    if (workspace / "real_execution.json").exists() or not workspace.is_dir():
        parser.error("Select an isolated Mock workspace")
    try:
        config = json.loads((workspace / "mock_execution.json").read_text(encoding="utf-8"))
        arguments = json.loads(args.arguments.read_text(encoding="utf-8"))
        if config["simulation"] is not True or not isinstance(arguments, dict):
            raise ValueError("Expected a Mock workspace and a JSON argument object")
        if "approval_id" in arguments or "request_id" in arguments:
            raise ValueError("Do not include approval_id or request_id")
        sample = {**arguments, "request_id": str(uuid4()), "approval_id": "approval-to-issue"}
        if not Draft202012Validator(tools[args.tool].input_schema).is_valid(sample):
            raise ValueError("The arguments do not match this tool schema")
        if args.tool == SET_EXECUTION and arguments["action"] != "abort":
            raise ValueError("Only abort requires approval for this tool")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    print(json.dumps({"identity": args.identity, "tool": args.tool, "arguments": arguments},
                     ensure_ascii=False, indent=2))
    if input('Type APPROVE to issue this single-use Mock approval: ') != "APPROVE":
        parser.exit(1, "No approval issued.\n")
    print(save_approval(workspace, args.identity, args.tool, arguments, args.ttl))


if __name__ == "__main__":
    main()
