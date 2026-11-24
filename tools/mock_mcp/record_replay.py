"""Durable playback controls over completed records, independent of execution."""

from contextlib import closing, contextmanager
import json
import sqlite3
import time
from uuid import uuid4

from protocol import COUNTER, ID, UUID, ToolFailure, utc_timestamp, tool
from workspace import RECORD_REPLAY

OPEN = "open_mock_record_replay"
SEEK = "seek_mock_replay"
CLOSE = "close_mock_replay"
RECORD_REPLAY_TOOLS = {
    item.name: item for item in (
        tool(OPEN, "Open your playback session for a completed Mock record.", {"record_id": ID, "request_id": UUID},
             {"request_id": UUID, "record_id": ID, "duration_ms": COUNTER, "replay": RECORD_REPLAY}, read_only=False),
        tool(SEEK, "Seek within your playback session.",
             {"replay_id": ID, "expected_version": COUNTER, "position_ms": COUNTER, "request_id": UUID},
             {"request_id": UUID, "record_id": ID, "replay": RECORD_REPLAY}, read_only=False),
        tool(CLOSE, "Close your playback session without changing the business record.",
             {"replay_id": ID, "expected_version": COUNTER, "request_id": UUID},
             {"request_id": UUID, "record_id": ID, "replay_id": ID, "closed": {"const": True}}, read_only=False),
    )
}


class RecordReplayStore:
    def __init__(self, filename, workspace):
        self.filename = filename
        self.workspace = workspace
        with closing(sqlite3.connect(filename)) as db:
            db.executescript("""
                CREATE TABLE IF NOT EXISTS replay (
                    workspace TEXT NOT NULL, owner TEXT NOT NULL, record_id TEXT NOT NULL, replay_id TEXT NOT NULL,
                    duration INTEGER NOT NULL, state TEXT NOT NULL, position INTEGER NOT NULL,
                    version INTEGER NOT NULL, changed_at REAL NOT NULL,
                    PRIMARY KEY (workspace, owner, record_id), UNIQUE (workspace, replay_id)
                );
                CREATE TABLE IF NOT EXISTS replay_receipt (
                    workspace TEXT NOT NULL, owner TEXT NOT NULL, tool TEXT NOT NULL, request_id TEXT NOT NULL,
                    arguments TEXT NOT NULL, observed_at TEXT NOT NULL, result TEXT NOT NULL,
                    PRIMARY KEY (workspace, owner, tool, request_id)
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
            raise ToolFailure("MOCK_UNAVAILABLE", "Playback state is unavailable; retain the same request_id") from error

    @staticmethod
    def value(row, now):
        position = row["position"]
        if row["state"] == "playing":
            position += max(0, int((now - row["changed_at"]) * 1000))
        return {"replay_id": row["replay_id"], "state": row["state"], "version": row["version"],
                "position_ms": min(row["duration"], position)}

    def list_owned(self, owner):
        with self.connection() as db:
            rows = db.execute("SELECT * FROM replay WHERE workspace = ? AND owner = ?", (self.workspace, owner))
            now = time.time()
            return {row["record_id"]: self.value(row, now) for row in rows}

    def by_id(self, db, owner, replay_id):
        return db.execute("SELECT * FROM replay WHERE workspace = ? AND owner = ? AND replay_id = ?",
                          (self.workspace, owner, replay_id)).fetchone()

    @staticmethod
    def check(row, records, arguments):
        if row is None or row["record_id"] not in records:
            raise ToolFailure("REPLAY_NOT_FOUND", "Open or list your own playback session")
        if row["version"] != arguments["expected_version"]:
            raise ToolFailure("REVISION_CONFLICT", "Read the current replay version")

    def write(self, owner, name, arguments, records):
        request_id = arguments["request_id"]
        receipt_key = (self.workspace, owner, name, request_id)
        fingerprint = json.dumps({key: value for key, value in arguments.items() if key != "request_id"},
                                 sort_keys=True, separators=(",", ":"))
        with self.connection(write=True) as db:
            old = db.execute("SELECT arguments, observed_at, result FROM replay_receipt "
                             "WHERE workspace = ? AND owner = ? AND tool = ? AND request_id = ?", receipt_key).fetchone()
            if old is not None:
                if old["arguments"] != fingerprint:
                    raise ToolFailure("REQUEST_ID_CONFLICT", "Use the original parameters for this request_id")
                return json.loads(old["result"]), old["observed_at"]

            now = time.time()
            result = {"request_id": request_id}
            if name == OPEN:
                record_id = arguments["record_id"]
                record = records.get(record_id)
                if record is None:
                    raise ToolFailure("NOT_FOUND", "List completed records for this workspace")
                db.execute("INSERT OR IGNORE INTO replay VALUES (?, ?, ?, ?, ?, 'paused', 0, 1, ?)",
                           (self.workspace, owner, record_id, "record-replay-" + uuid4().hex, record["duration_ms"], now))
                row = db.execute("SELECT * FROM replay WHERE workspace = ? AND owner = ? AND record_id = ?",
                                 (self.workspace, owner, record_id)).fetchone()
                result.update(record_id=record_id, duration_ms=row["duration"], replay=self.value(row, now))
            else:
                replay_id = arguments["replay_id"]
                row = self.by_id(db, owner, replay_id)
                self.check(row, records, arguments)
                result["record_id"] = row["record_id"]
                identity = (self.workspace, owner, replay_id)
                if name == CLOSE:
                    db.execute("DELETE FROM replay WHERE workspace = ? AND owner = ? AND replay_id = ?", identity)
                    result.update(replay_id=replay_id, closed=True)
                else:
                    position = arguments["position_ms"]
                    if position > row["duration"]:
                        raise ToolFailure("INVALID_ARGUMENT", "Seek position exceeds the recording duration")
                    db.execute("UPDATE replay SET position = ?, changed_at = ?, version = version + 1 "
                               "WHERE workspace = ? AND owner = ? AND replay_id = ?", (position, now, *identity))
                    result["replay"] = self.value(self.by_id(db, owner, replay_id), now)
            observed_at = utc_timestamp(now)
            db.execute("INSERT INTO replay_receipt VALUES (?, ?, ?, ?, ?, ?, ?)",
                       (*receipt_key, fingerprint, observed_at, json.dumps(result)))
            return result, observed_at

    def set_state(self, owner, arguments, records):
        replay_id = arguments["replay_id"]
        with self.connection(write=True) as db:
            row = self.by_id(db, owner, replay_id)
            self.check(row, records, arguments)
            now = time.time()
            state = arguments["state"]
            if row["state"] != state:
                position = self.value(row, now)["position_ms"]
                db.execute("UPDATE replay SET state = ?, position = ?, changed_at = ?, version = version + 1 "
                           "WHERE workspace = ? AND owner = ? AND replay_id = ?",
                           (state, position, now, self.workspace, owner, replay_id))
                row = self.by_id(db, owner, replay_id)
            return {"record_id": row["record_id"], "replay": self.value(row, now)}, utc_timestamp(now)
