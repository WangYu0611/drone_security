"""Session handling for synthetic alert sources, independent of recorded events."""

from contextlib import closing, contextmanager
import sqlite3

from protocol import BOOLEAN, COUNTER, ID, STRING, TIMESTAMP, UUID, ToolFailure
from protocol import choices, nullable, tool, utc_epoch, utc_timestamp
from workspace import belongs_in_workspace, id_or_none, paged_tool

LIST_ALERTS = "list_mock_session_alerts"
ACK_ALERT = "acknowledge_mock_session_alert"
ALERT_SCOPE = "mock:alerts:ack"
SEVERITY = choices("info", "warning", "critical")
ALERT_SOURCE = {"severity": SEVERITY, "code": {"type": "string", "pattern": r"^[A-Z][A-Z0-9_]{0,63}$"},
                "device_id": nullable(ID), "occurred_at": TIMESTAMP}
ALERT_FIELDS = {"alert_id": ID, "version": COUNTER, **ALERT_SOURCE, "handled": BOOLEAN}
SESSION_ALERT_TOOLS = {
    LIST_ALERTS: paged_tool(LIST_ALERTS, "List alerts and handling marks within this Mock session.",
                            {"session_id": ID, "severity": SEVERITY, "handled": BOOLEAN},
                            ALERT_FIELDS, required=("session_id",), session_id=ID),
    ACK_ALERT: tool(ACK_ALERT, "Mark a synthetic alert handled only in the current session.",
                    {"session_id": ID, "alert_id": ID, "expected_version": COUNTER, "request_id": UUID},
                    {"session_id": ID, "alert_id": ID, "version": COUNTER, "handled": BOOLEAN, "request_id": UUID},
                    read_only=False),
}


class SessionAlerts:
    def __init__(self, filename, workspace, fixtures):
        self.filename = filename
        self.workspace = workspace
        self.scope = (workspace.workspace_id, workspace.session_id)
        self.fixtures = fixtures
        with self.connection() as db:
            db.executescript("""
                CREATE TABLE IF NOT EXISTS alert_handled (
                    workspace TEXT NOT NULL, session TEXT NOT NULL, alert_id TEXT NOT NULL,
                    PRIMARY KEY (workspace, session, alert_id)
                );
                CREATE TABLE IF NOT EXISTS alert_receipt (
                    workspace TEXT NOT NULL, session TEXT NOT NULL, identity TEXT NOT NULL, request_id TEXT NOT NULL,
                    alert_id TEXT NOT NULL, expected_version INTEGER NOT NULL, version INTEGER NOT NULL,
                    observed_at TEXT NOT NULL,
                    PRIMARY KEY (workspace, session, identity, request_id)
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
            raise ToolFailure("MOCK_UNAVAILABLE", "Session alerts are unavailable; retain the same request_id") from error

    def sources(self, data):
        executions = {key: row for key, row in data["executions"].items() if belongs_in_workspace(row)}
        devices = set(data["simulation_uavs"]) | set(data["fixture_devices"])
        rows = {key: (utc_epoch(row["occurred_at"]), row) for key, row in self.fixtures.items()
                if row["device_id"] is None or row["device_id"] in devices}
        severity = {"MISSION_FAILED": "warning", "SAFETY_CONFLICT": "critical"}
        for event in data["events"]:
            code = event.get("event_type")
            execution = executions.get(event.get("target_id"))
            when = event.get("timestamp", 0)
            if code in severity and execution and when > 0:
                key = "mission-alert-" + str(event["sequence"])
                rows[key] = (when, {"severity": severity[code], "code": code, "occurred_at": utc_timestamp(when),
                                    "device_id": id_or_none(execution.get("uav_id"))})
        return rows

    async def call(self, identity, name, arguments):
        if arguments["session_id"] != self.workspace.session_id:
            raise ToolFailure("SESSION_EXPIRED", "Choose the session assigned to this service")
        data = await self.workspace.snapshot()
        sources = self.sources(data)
        if name == ACK_ALERT:
            return self.acknowledge(identity.name, arguments, sources)
        with self.connection() as db:
            handled = {row["alert_id"] for row in db.execute(
                "SELECT alert_id FROM alert_handled WHERE workspace = ? AND session = ?", self.scope)}
        rows = []
        for key, (occurred_at, source) in sources.items():
            row = {**source, "alert_id": key, "handled": key in handled, "version": 2 if key in handled else 1}
            if self.workspace.matching(arguments, row, ("severity", "handled")):
                rows.append((occurred_at, key, row))
        page = self.workspace.page(identity, name, arguments, rows)
        return {"session_id": self.workspace.session_id, **page}, utc_timestamp(data["observed_at"])

    def acknowledge(self, identity, arguments, sources):
        alert_id, expected, request = (arguments[key] for key in ("alert_id", "expected_version", "request_id"))
        receipt_key = (*self.scope, identity, request)
        with self.connection(write=True) as db:
            previous = db.execute("SELECT * FROM alert_receipt WHERE workspace = ? AND session = ? "
                                  "AND identity = ? AND request_id = ?", receipt_key).fetchone()
            if previous:
                if (alert_id, expected) != (previous["alert_id"], previous["expected_version"]):
                    raise ToolFailure("REQUEST_ID_CONFLICT", "Use the original arguments for this request_id")
                version, observed = previous["version"], previous["observed_at"]
            else:
                if alert_id not in sources:
                    raise ToolFailure("NOT_FOUND", "List alerts from the assigned Mock session")
                key = (*self.scope, alert_id)
                handled = db.execute(
                    "SELECT 1 FROM alert_handled WHERE workspace = ? AND session = ? AND alert_id = ?", key).fetchone()
                if expected != (2 if handled else 1):
                    raise ToolFailure("VERSION_CONFLICT", "Read the current alert version")
                db.execute("INSERT OR IGNORE INTO alert_handled VALUES (?, ?, ?)", key)
                version, observed = 2, utc_timestamp()
                db.execute("INSERT INTO alert_receipt VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                           (*receipt_key, alert_id, expected, version, observed))
        return {"session_id": self.workspace.session_id, "alert_id": alert_id, "version": version,
                "handled": True, "request_id": request}, observed
