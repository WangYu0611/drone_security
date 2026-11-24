"""Mock operator requests and a local console for human review."""

import argparse
from contextlib import closing, contextmanager
import json
import os
from pathlib import Path
import re
import sqlite3
import time
from uuid import uuid4

import httpx

from protocol import COUNTER, ID, ID_PATTERN, TIMESTAMP, UUID, ToolFailure
from protocol import choices, nullable, object_schema, tool, utc_timestamp
from workspace import OPERATOR_STATE, paged_tool

LIST_HANDOFF = "list_mock_pilot_handoff_requests"
GET_HANDOFF = "get_mock_pilot_handoff_status"
REQUEST_HANDOFF = "request_mock_pilot_handoff"
REQUEST_RELEASE = "request_mock_pilot_handoff_release"
REQUEST_RESET = "request_mock_workspace_reset"
HEALTH = "get_mock_workspace_health"
PILOT_SCOPE = "mock:pilot:request"
RESET_SCOPE = "mock:sandbox:reset"
OPERATOR_SCOPES = {REQUEST_HANDOFF: PILOT_SCOPE, REQUEST_RELEASE: PILOT_SCOPE, REQUEST_RESET: RESET_SCOPE}
KINDS = choices("handoff", "release")
PENDING = {"const": "pending_operator"}
ACTIVE_REQUEST = {"pending_operator", "approved"}
DIRECT_REQUEST = {"device_id": ID, "expected_version": COUNTER, "request_id": UUID,
                  "note": {"type": "string", "maxLength": 512}}
REQUEST_INFO = {"handoff_request_id": ID, "kind": KINDS, "state": OPERATOR_STATE}
OPERATOR_TOOLS = {item.name: item for item in (
    paged_tool(LIST_HANDOFF, "List operator requests from the authorized software workspace.",
               {"device_id": ID, "kind": KINDS, "state": OPERATOR_STATE},
               {**REQUEST_INFO, "device_id": ID, "created_at": TIMESTAMP}),
    tool(GET_HANDOFF, "Read the software device's authority and session status independently.",
         {"device_id": ID}, {"device_id": ID, "version": COUNTER,
          "authority_state": choices("assigned", "unassigned"), "session_state": choices("active", "inactive"),
          "latest_request": nullable(object_schema(REQUEST_INFO))}),
    *(tool(name, f"Queue a human review for software Pilot {kind}.", DIRECT_REQUEST,
           {**REQUEST_INFO, "kind": {"const": kind}, "device_id": ID,
            "state": PENDING, "version": COUNTER, "request_id": UUID}, ("note",), read_only=False)
      for name, kind in ((REQUEST_HANDOFF, "handoff"), (REQUEST_RELEASE, "release"))),
    tool(REQUEST_RESET, "Queue an operator review of the dedicated Mock workspace's startup restore.",
         {"workspace_id": ID, "expected_workspace_version": COUNTER, "request_id": UUID,
          "reason": {"type": "string", "minLength": 1, "maxLength": 512}},
         {"workspace_id": ID, "workspace_version": COUNTER, "reset_request_id": ID,
          "request_id": UUID, "state": PENDING}, read_only=False),
)}


class OperatorRequests:
    def __init__(self, filename, workspace):
        self.filename = filename
        self.workspace = workspace
        with self.connection() as db:
            db.executescript("""
                CREATE TABLE IF NOT EXISTS operator_request (
                    id TEXT PRIMARY KEY, workspace TEXT NOT NULL, target TEXT NOT NULL, kind TEXT NOT NULL,
                    identity TEXT NOT NULL, request_id TEXT NOT NULL, fingerprint TEXT NOT NULL, note TEXT NOT NULL,
                    created REAL NOT NULL, state TEXT NOT NULL, receipt TEXT,
                    UNIQUE (workspace, identity, kind, request_id)
                );
                CREATE TABLE IF NOT EXISTS operator_version (
                    workspace TEXT NOT NULL, target TEXT NOT NULL, stamp TEXT NOT NULL, version INTEGER NOT NULL,
                    PRIMARY KEY (workspace, target)
                );
            """)

    @contextmanager
    def connection(self, write=False):
        try:
            with closing(sqlite3.connect(self.filename, timeout=2)) as db, db:
                db.row_factory = sqlite3.Row
                if write:
                    db.execute("BEGIN IMMEDIATE")
                yield db
        except sqlite3.Error as error:
            raise ToolFailure("MOCK_UNAVAILABLE", "Operator request storage is unavailable") from error

    def rows(self, db):
        return db.execute("SELECT * FROM operator_request WHERE workspace = ? ORDER BY rowid DESC",
                          (self.workspace,)).fetchall()

    @staticmethod
    def handoff(row):
        return {"handoff_request_id": row["id"], "kind": row["kind"], "state": row["state"]}

    def reconcile(self, db, data):
        rows = self.rows(db)
        now = time.time()
        for row in rows:
            if row["state"] not in ACTIVE_REQUEST:
                continue
            pilot = data["pilot_authority"].get(row["target"])
            reset = row["kind"] == "reset"
            completed = (data["reset_marker"] == row["id"] if reset else pilot is not None and
                         pilot["assigned"] == (row["kind"] == "handoff") and
                         (row["kind"] != "release" or not pilot["active"]))
            new_state = None
            if row["state"] == "approved" and completed:
                new_state = "completed"
            elif now - row["created"] >= 900:
                new_state = "expired"
            if new_state:
                db.execute("UPDATE operator_request SET state = ? WHERE id = ?", (new_state, row["id"]))
        return self.rows(db)

    def revision(self, db, target, marker, latest):
        stamp = json.dumps([marker, [latest["id"], latest["state"]] if latest else None], sort_keys=True)
        current = db.execute("SELECT stamp, version FROM operator_version WHERE workspace = ? AND target = ?",
                             (self.workspace, target)).fetchone()
        if current and current["stamp"] == stamp:
            return current["version"]
        version = current["version"] + 1 if current else 1
        db.execute("INSERT INTO operator_version VALUES (?, ?, ?, ?) ON CONFLICT (workspace, target) "
                   "DO UPDATE SET stamp = excluded.stamp, version = excluded.version",
                   (self.workspace, target, stamp, version))
        return version

    def request(self, db, identity, name, arguments, data, reader, rows, dedicated):
        reset = name == REQUEST_RESET
        kind = "reset" if reset else "handoff" if name == REQUEST_HANDOFF else "release"
        target = "$workspace" if reset else arguments["device_id"]
        text_field = "reason" if reset else "note"
        version_field = "expected_workspace_version" if reset else "expected_version"
        note = arguments.get(text_field, "").strip()
        try:
            note.encode("utf-8")
        except UnicodeError as error:
            raise ToolFailure("INVALID_ARGUMENT", "Use valid UTF-8 text") from error
        expected = int(arguments[version_field])
        fingerprint = json.dumps({**arguments, text_field: note, version_field: expected}, sort_keys=True)
        request_id = arguments["request_id"]
        previous = db.execute("SELECT fingerprint, receipt FROM operator_request WHERE workspace = ? AND "
                              "identity = ? AND kind = ? AND request_id = ?",
                              (self.workspace, identity.name, kind, request_id)).fetchone()
        if previous:
            if previous["fingerprint"] != fingerprint:
                raise ToolFailure("REQUEST_ID_CONFLICT", "Use the original parameters for this request_id")
            result, observed = json.loads(previous["receipt"])
            return result, observed

        latest = next((row for row in rows if row["target"] == target), None)
        if reset:
            if arguments["workspace_id"] != self.workspace:
                raise ToolFailure("NOT_FOUND", "Choose the assigned Mock workspace")
            if not note:
                raise ToolFailure("INVALID_ARGUMENT", "Provide a reason for the restore")
            if not dedicated or data["reset_blocked"]:
                raise ToolFailure("INVALID_STATE", "Restore requests need an idle, dedicated Mock workspace")
            marker = self.workspace_marker(data)
        else:
            if target not in reader.devices(data):
                raise ToolFailure("NOT_FOUND", "Choose a device from the Mock workspace")
            marker = data["pilot_authority"].get(target)
            if marker is None or (kind == "handoff" and (marker["assigned"] or not marker["eligible"])) or (
                    kind == "release" and not marker["assigned"]):
                raise ToolFailure("INVALID_STATE", "The software device is ineligible for this request")
        if latest and latest["state"] in ACTIVE_REQUEST:
            raise ToolFailure("OBJECT_IN_USE", "An operator request is already open")
        if self.revision(db, target, marker, latest) != expected:
            raise ToolFailure("VERSION_CONFLICT", "Read the current state before submitting")
        ident = ("reset-" if reset else "handoff-") + uuid4().hex
        now = time.time()
        observed = utc_timestamp(now)
        db.execute("INSERT INTO operator_request VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending_operator', NULL)",
                   (ident, self.workspace, target, kind, identity.name, request_id, fingerprint, note, now))
        current = {"id": ident, "state": "pending_operator"}
        version = self.revision(db, target, marker, current)
        result = {"state": current["state"], "request_id": request_id}
        if reset:
            result.update(workspace_id=self.workspace, workspace_version=version, reset_request_id=ident)
        else:
            result.update(device_id=target, version=version, handoff_request_id=ident, kind=kind)
        db.execute("UPDATE operator_request SET receipt = ? WHERE id = ?", (json.dumps([result, observed]), ident))
        return result, observed

    @staticmethod
    def workspace_marker(data):
        return [data["version"], data["reset_marker"], data["reset_blocked"]]

    async def call(self, reader, identity, name, arguments, dedicated):
        data = await reader.snapshot()
        data["executions"] = {key: row for key, row in data["executions"].items()
                              if row.get("simulation") is True and row.get("adapter") == "Mock"}
        with self.connection(write=True) as db:
            rows = self.reconcile(db, data)
            if name in OPERATOR_SCOPES:
                return self.request(db, identity, name, arguments, data, reader, rows, dedicated)
            if name == HEALTH:
                latest = next((row for row in rows if row["kind"] == "reset"), None)
                result = reader.get_health(data, arguments)
                result["version"] = self.revision(db, "$workspace", self.workspace_marker(data), latest)
                if latest:
                    result["reset_request"] = {"reset_request_id": latest["id"], "state": latest["state"]}
            elif name == GET_HANDOFF:
                target = arguments["device_id"]
                if target not in reader.devices(data):
                    raise ToolFailure("NOT_FOUND", "Choose a device from the Mock workspace")
                latest = next((row for row in rows if row["target"] == target), None)
                known = data["pilot_authority"].get(target, {})
                result = {"device_id": target, "version": self.revision(db, target, known or None, latest),
                          "authority_state": "assigned" if known.get("assigned") else "unassigned",
                          "session_state": "active" if known.get("active") else "inactive",
                          "latest_request": self.handoff(latest) if latest else None}
            else:
                values = []
                for row in rows:
                    if row["kind"] == "reset":
                        continue
                    entry = {**self.handoff(row), "device_id": row["target"], "created_at": utc_timestamp(row["created"])}
                    if reader.matching(arguments, entry, ("device_id", "kind", "state")):
                        values.append((row["created"], row["id"], entry))
                result = reader.page(identity, name, arguments, values)
            return result, utc_timestamp(data["observed_at"])

    def decision(self, ident, state):
        with self.connection(write=True) as db:
            changed = db.execute("UPDATE operator_request SET state = ? WHERE workspace = ? AND id = ? "
                                 "AND state = 'pending_operator' AND created > ?",
                                 (state, self.workspace, ident, time.time() - 900))
            if not changed.rowcount:
                raise ValueError("The request has already been reviewed or expired")


def apply_reset(store, request_id, backend_port):
    with store.connection() as db:
        row = db.execute("SELECT * FROM operator_request WHERE workspace = ? AND id = ? AND kind = 'reset'",
                         (store.workspace, request_id)).fetchone()
    if not row or row["state"] not in {"approved", "completed"} or time.time() - row["created"] >= 900:
        raise ValueError("An operator must approve a current dedicated-workspace restore")
    secret = os.environ.get("DRONE_MOCK_MCP_BRIDGE_TOKEN", "")
    if not re.fullmatch(r"[0-9a-f]{64}", secret) or not 1 <= backend_port <= 65535:
        raise ValueError("Select the operator-owned loopback Mock Backend")
    with httpx.Client(base_url=f"http://127.0.0.1:{backend_port}", headers={"Authorization": f"Bearer {secret}"},
                      trust_env=False, timeout=10) as http:
        check = http.get("/api/mock-mcp/workspace")
        if check.status_code != 200 or check.json().get("simulation") is not True:
            raise ValueError("The selected Backend did not authenticate as an isolated Mock workspace")
        response = http.post("/api/mock-executions/reset", json={
            "request_id": request_id, "simulation": True, "require_idle": True,
        })
        if response.status_code != 200:
            raise ValueError("The Mock Backend refused the restore. Verify its state and startup fixture.")
    return response.json()["reset_epoch"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-db", required=True, type=Path)
    parser.add_argument("--workspace", required=True)
    commands = parser.add_subparsers(dest="action", required=True)
    commands.add_parser("list")
    review = commands.add_parser("review")
    review.add_argument("request_id")
    restore = commands.add_parser("apply-reset")
    restore.add_argument("request_id")
    restore.add_argument("--backend-port", required=True, type=int)
    args = parser.parse_args()
    if not re.fullmatch(ID_PATTERN, args.workspace) or not args.state_db.is_file():
        parser.error("Choose the configured workspace and its existing operator database")
    store = OperatorRequests(args.state_db, args.workspace)
    with store.connection() as db:
        rows = [{key: row[key] for key in ("id", "identity", "target", "kind", "note", "state", "created")}
                for row in store.rows(db) if args.action == "list" or row["id"] == args.request_id]
    print(json.dumps(rows, indent=2, ensure_ascii=False))
    if args.action == "list":
        return
    if not rows:
        parser.error("The request was not found")
    try:
        if args.action == "review":
            decision = input("Type APPROVE or REJECT: ")
            if decision not in {"APPROVE", "REJECT"}:
                raise ValueError("No decision recorded")
            store.decision(args.request_id, "approved" if decision == "APPROVE" else "rejected")
        elif input("Type RESTORE to apply the approved startup fixture: ") == "RESTORE":
            print(f"Mock startup epoch {apply_reset(store, args.request_id, args.backend_port)}")
        else:
            raise ValueError("No restore applied")
    except (ToolFailure, httpx.HTTPError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
