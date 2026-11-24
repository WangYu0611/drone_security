"""Project authorized Mock Backend snapshots into the MCP read contract."""

from datetime import datetime
import base64
import hashlib
import hmac
import json
import re

import httpx

from protocol import BOOLEAN, COUNTER, ID, ID_PATTERN, STRING, TIMESTAMP, UUID, ToolFailure
from protocol import array, choices, nullable, object_schema, tool, utc_timestamp

PAGE_INPUT = {"limit": {"type": "integer", "minimum": 1, "maximum": 50, "default": 20},
              "cursor": {"type": "string", "minLength": 1, "maxLength": 512}}
PLAN_STATE = choices("draft", "ready", "deployed")
RUN_STATE = choices("created", "preflight", "starting", "start_requested", "waiting_ack", "scheduled",
                    "executing", "paused", "returning", "completed", "failed", "aborted", "cancelled")
DEVICE_KIND = choices("simulated", "test_fixture")
DEVICE_AVAILABILITY = choices("available", "busy", "offline")
CATEGORY = choices("plan", "execution", "rehearsal", "fleet", "video", "handoff", "system")
PERCENT = {"type": "number", "minimum": 0, "maximum": 100}
PLAN_FIELDS = {"plan_id": ID, "name": STRING, "state": PLAN_STATE, "plan_version": COUNTER,
               "content_revision": COUNTER}
DEPLOYMENT_FIELDS = {"deployment_id": ID, "content_revision": COUNTER, "created_at": TIMESTAMP,
                     "mission_count": COUNTER}
RUN_FIELDS = {"execution_id": ID, "kind": choices("mission", "fleet_move", "single_takeover"),
              "plan_id": nullable(ID), "mission_id": nullable(ID), "state": RUN_STATE, "version": COUNTER,
              "scheduled_for": nullable(TIMESTAMP)}
DEVICE_FIELDS = {"device_id": ID, "name": STRING, "kind": DEVICE_KIND,
                 "availability": DEVICE_AVAILABILITY, "version": COUNTER}
REHEARSAL_FIELDS = {"rehearsal_id": ID, "plan_id": ID, "mission_id": ID, "state": RUN_STATE,
                    "version": COUNTER, "review_state": choices("pending", "approved", "stale"),
                    "record_id": nullable(ID)}
RECORD_REPLAY = object_schema({"replay_id": ID, "state": choices("playing", "paused"),
                               "position_ms": COUNTER, "version": COUNTER})
EVENT_FIELDS = {"event_id": ID, "category": CATEGORY, "object_id": nullable(ID), "code": STRING,
                "occurred_at": TIMESTAMP, "message": STRING}
SELECTION = {"plan_id": nullable(ID), "mission_id": nullable(ID),
             "device_ids": array(ID, uniqueItems=True, maxItems=20), "primary_device_id": nullable(ID)}
CONTEXT_FIELDS = {"session_id": ID, "version": COUNTER, **SELECTION, "video_target_device_id": nullable(ID)}
PREFERENCE_FIELDS = {"session_id": ID, "version": COUNTER, "language": choices("en", "zh-Hans")}
VIDEO_FIELDS = {"session_id": ID, "version": COUNTER, "target_device_id": nullable(ID), "source_id": nullable(ID),
                "playback_state": choices("unreported", "no_source", "loading", "playing", "paused", "error"),
                "playback_observed_at": nullable(TIMESTAMP)}
VIDEO_SOURCE = {"device_id": ID, "title": {"type": "string", "minLength": 1, "maxLength": 160},
                "availability": choices("available", "unavailable")}
SELECT = "set_mock_workspace_selection"
LANGUAGE = "set_mock_workspace_language"
VIDEO = "set_mock_video_target"
VIEW_SCOPES = {SELECT: "mock:context:select", LANGUAGE: "mock:context:preferences", VIDEO: "mock:video:select"}
VERSIONED_VIEW = {"session_id": ID, "expected_version": COUNTER, "request_id": UUID}
VIEW_TOOLS = {item.name: item for item in (
    tool(SELECT, "Replace the current Mock selection without changing video.", {**VERSIONED_VIEW, **SELECTION},
         {**CONTEXT_FIELDS, "request_id": UUID}, read_only=False),
    tool(LANGUAGE, "Change the shared Mock session language.",
         {**VERSIONED_VIEW, "language": PREFERENCE_FIELDS["language"]},
         {**PREFERENCE_FIELDS, "request_id": UUID}, read_only=False),
    tool(VIDEO, "Choose a synthetic target and one authorized fixture source, or clear the target.",
         {**VERSIONED_VIEW, "device_id": nullable(ID), "source_id": ID},
         {**VIDEO_FIELDS, "request_id": UUID}, ("source_id",), read_only=False),
)}


def paged_tool(name, description, filters, fields, required=(), **outer):
    return tool(name, description, {**filters, **PAGE_INPUT},
                {**outer, "items": array(object_schema(fields)), "next_cursor": nullable(STRING)},
                optional=tuple((set(filters) - set(required)) | set(PAGE_INPUT)))


WORKSPACE_TOOLS = {
    item.name: item for item in (
        paged_tool("list_mock_security_plans", "List plans in the isolated Mock workspace.",
                   {"state": PLAN_STATE}, {**PLAN_FIELDS, "mission_count": COUNTER}),
        tool("get_mock_security_plan", "Read a Mock plan and its current configuration status.", {"plan_id": ID},
             {**PLAN_FIELDS, "description": STRING, "missions": array(object_schema({
                 "mission_id": ID, "name": STRING, "device_ids": array(ID), "route_version": COUNTER,
             })), "configuration": object_schema({"state": choices("idle", "busy", "owned"),
                 "configuration_id": nullable(ID), "lease_expires_at": nullable(TIMESTAMP)})}),
        tool("get_mock_mission_route", "Get the saved route version and its synthetic fixture references.",
             {"plan_id": ID, "mission_id": ID}, {"plan_id": ID, "mission_id": ID, "route_version": COUNTER,
              "route": nullable(object_schema({"route_id": ID, "map_fixture_id": ID, "waypoint_fixture_ids": array(ID),
                                               "closed_loop": BOOLEAN, "state": {"const": "saved"}}))}),
        paged_tool("list_mock_plan_deployments", "List the deployment snapshots of a Mock plan.",
                   {"plan_id": ID}, DEPLOYMENT_FIELDS, required=("plan_id",), plan_id=ID),
        paged_tool("list_mock_executions", "List software-only execution records.",
                   {"plan_id": ID, "mission_id": ID, "state": RUN_STATE}, RUN_FIELDS),
        tool("get_mock_execution", "Read a software-only execution and its member status.", {"execution_id": ID},
             {**RUN_FIELDS, "deployment_id": nullable(ID), "progress_pct": PERCENT, "elapsed_ms": COUNTER,
              "members": array(object_schema({"device_id": ID, "state": RUN_STATE})), "reason_code": nullable(STRING)}),
        paged_tool("list_mock_devices", "List devices sourced from the software simulator or test fixtures.",
                   {"kind": DEVICE_KIND, "availability": DEVICE_AVAILABILITY}, DEVICE_FIELDS),
        tool("get_mock_device_status", "Read status for one software simulator or test device.", {"device_id": ID},
             {**DEVICE_FIELDS, "label": STRING, "telemetry_source": nullable(STRING),
              "telemetry_observed_at": nullable(TIMESTAMP), "position_fixture_id": nullable(ID),
              "mission_id": nullable(ID), "execution_id": nullable(ID), "pilot_handoff_active": BOOLEAN}),
        paged_tool("list_mock_rehearsals", "List stored software rehearsal records.",
                   {"plan_id": ID, "mission_id": ID, "state": RUN_STATE}, REHEARSAL_FIELDS),
        tool("get_mock_rehearsal", "Read the result and members of a software rehearsal.", {"rehearsal_id": ID},
             {**REHEARSAL_FIELDS, "input_content_revision": COUNTER, "members": array(object_schema({
                 "device_id": ID, "state": RUN_STATE, "progress_pct": PERCENT,
             })), "reason_code": nullable(STRING)}),
        paged_tool("list_mock_replay_records", "List completed Mock records that contain replay samples.",
                   {"execution_id": ID, "rehearsal_id": ID, "record_id": ID},
                   {"record_id": ID, "execution_id": nullable(ID), "rehearsal_id": nullable(ID),
                    "duration_ms": COUNTER, "replay": nullable(RECORD_REPLAY)}),
        tool("get_mock_workspace_health", "Check the isolated Mock Backend and connected client roles.", {},
             {"workspace_id": ID, "version": COUNTER, "state": choices("ready", "offline"),
              "backend_reachable": BOOLEAN, "last_backend_observed_at": nullable(TIMESTAMP),
              "simulation_time": nullable(TIMESTAMP), "clients": array(object_schema({
                  "role": choices("command", "map", "video"),
                  "connection_state": choices("connected", "disconnected"),
              })), "reset_request": {"type": "null"}}),
        tool("get_mock_workspace_context", "Read selection within the assigned Mock session.", {"session_id": ID},
             {**CONTEXT_FIELDS, "device_ids": array(ID)}),
        tool("get_mock_workspace_preferences", "Read the isolated session's interface language.", {"session_id": ID},
             PREFERENCE_FIELDS),
        paged_tool("list_mock_video_sources", "List synthetic video fixture metadata without media addresses.",
                   {"device_id": ID, "availability": VIDEO_SOURCE["availability"]}, {"source_id": ID, **VIDEO_SOURCE}),
        tool("get_mock_video_view", "Read the video target in the assigned Mock session.", {"session_id": ID},
             VIDEO_FIELDS),
        paged_tool("list_mock_operation_events", "List redacted Mock events from Backend.",
                   {"category": CATEGORY, "object_id": ID, "from": TIMESTAMP, "to": TIMESTAMP}, EVENT_FIELDS),
    )
}

TERMINAL = {"completed", "failed", "aborted", "cancelled"}
EVENT_CATEGORIES = {"PLAN": "plan", "MISSION": "plan", "ROUTE": "plan", "EXECUTION": "execution",
                    "REHEARSAL": "rehearsal", "SIMULATION": "rehearsal", "FLEET": "fleet", "VIDEO": "video",
                    "HANDOFF": "handoff", "SYSTEM": "system"}


def stamp(epoch):
    return utc_timestamp(epoch) if isinstance(epoch, (int, float)) and epoch > 0 else None


def id_or_none(value):
    return value if isinstance(value, str) and re.fullmatch(ID_PATTERN, value) else None


def lookup(rows, ident):
    if ident not in rows:
        raise ToolFailure("NOT_FOUND", "Choose an object in the assigned Mock workspace")
    return rows[ident]


def percentage(row):
    return max(0, min(100, row.get("progress", 0) * 100))


def reason(row):
    value = row.get("failure_reason") or row.get("completion_reason") or ""
    # Adapter exception text can contain host-specific values. Expose only a code.
    return value if re.fullmatch(r"[A-Z][A-Z0-9_]{0,63}", value) else None


def elapsed(row):
    return max(0, round(1000 * row.get("elapsed_seconds", 0)))


def plan_summary(plan):
    return {"plan_id": plan["id"], "name": plan["name"], "state": plan["status"].lower(),
            "plan_version": plan["version"], "content_revision": plan["content_revision"]}


def execution_summary(run):
    kind = "single_takeover" if run.get("source_execution_id") else (
        "fleet_move" if run.get("control_kind") == "GROUP_MOVE" else "mission")
    return {"execution_id": run["execution_id"], "kind": kind,
            "plan_id": id_or_none(run.get("plan_id")), "mission_id": id_or_none(run.get("mission_id")),
            "state": run["state"].lower(), "version": int(run.get("control_version", 0)),
            "scheduled_for": stamp(run.get("scheduled_start_epoch"))}


def belongs_in_workspace(run):
    return run.get("simulation") is True and run.get("adapter") == "Mock"


def with_update(row, field="updated_at"):
    return row.get(field) or row.get("created_at") or 0


class WorkspaceReader:
    def __init__(self, backend_port, bridge_token, workspace_id, session_id, video_sources):
        self.url = f"http://127.0.0.1:{backend_port}/api/mock-mcp/workspace"
        self.token = bridge_token
        self.workspace_id = workspace_id
        self.session_id = session_id
        self.video_sources = video_sources
        self.handlers = {
            "list_mock_security_plans": self.list_plans, "get_mock_security_plan": self.get_plan,
            "get_mock_mission_route": self.get_route,
            "list_mock_plan_deployments": self.list_deployments, "list_mock_executions": self.list_executions,
            "get_mock_execution": self.get_execution, "list_mock_devices": self.list_devices,
            "get_mock_device_status": self.get_device, "list_mock_rehearsals": self.list_rehearsals,
            "get_mock_rehearsal": self.get_rehearsal, "list_mock_replay_records": self.list_records,
            "get_mock_workspace_health": self.get_health, "get_mock_workspace_context": self.get_context,
            "get_mock_workspace_preferences": self.get_preferences, "get_mock_video_view": self.get_video,
            "list_mock_video_sources": self.list_video_sources,
            "list_mock_operation_events": self.list_events,
        }

    async def snapshot(self):
        try:
            async with httpx.AsyncClient(timeout=5, trust_env=False) as http:
                response = await http.get(self.url, headers={"Authorization": f"Bearer {self.token}"})
                response.raise_for_status()
                payload = response.json()
            if not isinstance(payload, dict) or payload.get("simulation") is not True:
                raise ValueError("Invalid Mock Backend snapshot")
        except (httpx.HTTPError, ValueError) as error:
            raise ToolFailure("MOCK_UNAVAILABLE", "The authorized Mock Backend did not return a snapshot") from error
        return payload

    async def call(self, identity, name, arguments, record_replays=None):
        if arguments.get("session_id", self.session_id) != self.session_id:
            raise ToolFailure("NOT_FOUND", "Choose the session assigned to this service")
        data = await self.snapshot()
        # The private transport is not authorization to expose arbitrary registry rows.
        data["executions"] = {key: row for key, row in data["executions"].items() if belongs_in_workspace(row)}
        data["plans"] = {key: row for key, row in data["plans"].items() if not row.get("real_assignment")}
        data["configuration_owner"] = "mock-mcp-" + identity.name
        result = self.handlers[name](data, arguments)
        if name == "list_mock_replay_records" and record_replays:
            for _, key, record in result:
                record["replay"] = record_replays.get(key)
        if isinstance(result, list):
            result = self.page(identity, name, arguments, result)
        if name == "list_mock_plan_deployments":
            result["plan_id"] = arguments["plan_id"]
        return result, utc_timestamp(data["observed_at"])

    async def records(self):
        data = await self.snapshot()
        data["executions"] = {key: row for key, row in data["executions"].items() if belongs_in_workspace(row)}
        return {key: row for _, key, row in self.list_records(data, {})}

    def page(self, identity, name, arguments, rows):
        filters = {key: value for key, value in arguments.items() if key not in PAGE_INPUT}
        scope = json.dumps([self.workspace_id, identity.name, name, filters], sort_keys=True, separators=(",", ":"))
        key = hmac.digest(self.token.encode("ascii"), scope.encode(), "sha256")
        cursor = arguments.get("cursor")
        after = None
        if cursor:
            try:
                encoded, tag = cursor.split(".")
                digest = hmac.digest(key, encoded.encode("ascii"), "sha256").hex()
                if not hmac.compare_digest(digest, tag):
                    raise ValueError("Scope does not match")
                after = tuple(json.loads(base64.urlsafe_b64decode(encoded)))
            except (ValueError, TypeError, UnicodeError) as error:
                raise ToolFailure("CURSOR_INVALID", "Refresh the first page for these filters") from error
        ranked = sorted(((-updated, ident), result) for updated, ident, result in rows)
        if after is not None:
            ranked = [(sort_key, result) for sort_key, result in ranked if sort_key > after]
        limit = arguments.get("limit", 20)
        cursor = None
        if len(ranked) > limit:
            encoded = base64.urlsafe_b64encode(json.dumps(ranked[limit - 1][0], separators=(",", ":")).encode())
            cursor = encoded.decode() + "." + hmac.digest(key, encoded, "sha256").hex()
        return {"items": [result for _, result in ranked[:limit]], "next_cursor": cursor}

    @staticmethod
    def matching(arguments, row, filters):
        return all(field not in arguments or row[field] == arguments[field] for field in filters)

    def list_plans(self, data, arguments):
        rows = []
        for plan in data["plans"].values():
            row = {**plan_summary(plan), "mission_count": len(plan["mission_ids"])}
            if self.matching(arguments, row, ("state",)):
                rows.append((with_update(plan), row["plan_id"], row))
        return rows

    def get_plan(self, data, arguments):
        plan = lookup(data["plans"], arguments["plan_id"])
        missions = []
        for key in plan["mission_ids"]:
            mission = lookup(data["missions"], key)
            path = data["paths"].get(mission.get("route_id"), {})
            assignments = mission.get("assigned_uav_ids")
            if assignments is None:
                assignments = [mission["assigned_uav_id"]] if mission.get("assigned_uav_id") else []
            missions.append({"mission_id": key, "name": mission["name"], "device_ids": assignments,
                             "route_version": path.get("revision", 1) if path else 0})
        lease = data["configuration_session"]
        active = lease and lease["lease_expires_at"] > data["observed_at"]
        owned = active and lease["owner_instance_id"] == data["configuration_owner"] and lease["plan_id"] == plan["id"]
        return {**plan_summary(plan), "description": plan.get("description", ""), "missions": missions,
                "configuration": {"state": "owned" if owned else "busy" if active else "idle",
                                  "configuration_id": "configuration-" + str(lease["acquired_version"]) if owned else None,
                                  "lease_expires_at": stamp(lease["lease_expires_at"]) if owned else None}}

    def list_deployments(self, data, arguments):
        lookup(data["plans"], arguments["plan_id"])
        return [(row["deployed_at"], key, {"deployment_id": key, "content_revision": row["content_revision"],
                 "created_at": stamp(row["deployed_at"]), "mission_count": len(row["snapshot"]["missions"])})
                for key, row in data["deployments"].items() if row["plan_id"] == arguments["plan_id"] and
                not row["snapshot"]["plan"].get("real_assignment")]

    def get_route(self, data, arguments):
        plan_id = arguments["plan_id"]
        mission_id = arguments["mission_id"]
        plan = lookup(data["plans"], plan_id)
        if mission_id not in plan["mission_ids"]:
            raise ToolFailure("NOT_FOUND", "Choose a mission in the selected Mock plan")
        mission = lookup(data["missions"], mission_id)
        route_id = mission.get("route_id")
        result = {"plan_id": plan_id, "mission_id": mission_id, "route_version": 0, "route": None}
        if not route_id:
            return result
        saved = lookup(data["paths"], route_id)
        if not saved.get("map_fixture_id") or "waypoint_fixture_ids" not in saved:
            raise ToolFailure("FIXTURE_NOT_FOUND", "The saved route has no approved synthetic fixture references")
        result["route_version"] = saved.get("revision", 1)
        result["route"] = {"route_id": route_id, "map_fixture_id": saved["map_fixture_id"],
                           "waypoint_fixture_ids": saved["waypoint_fixture_ids"],
                           "closed_loop": bool(saved.get("bClosedLoop")), "state": "saved"}
        return result

    def list_executions(self, data, arguments):
        rows = []
        for run in data["executions"].values():
            if run.get("rehearsal"):
                continue
            row = execution_summary(run)
            if self.matching(arguments, row, ("plan_id", "mission_id", "state")):
                rows.append((with_update(run), row["execution_id"], row))
        return rows

    def get_execution(self, data, arguments):
        row = lookup(data["executions"], arguments["execution_id"])
        if row.get("rehearsal"):
            raise ToolFailure("NOT_FOUND", "Use the rehearsal tool for a rehearsal record")
        group = row.get("group_id")
        members = [run for run in data["executions"].values() if group and run.get("group_id") == group] or [row]
        return {**execution_summary(row), "deployment_id": id_or_none(row.get("deployment_id")),
                "progress_pct": percentage(row), "elapsed_ms": elapsed(row), "reason_code": reason(row),
                "members": [{"device_id": run["uav_id"], "state": run["state"].lower()}
                            for run in sorted(members, key=lambda item: item["uav_id"])]}

    def devices(self, data):
        devices = {}
        reserved = {}
        for row in data["executions"].values():
            if row["state"].lower() not in TERMINAL:
                reserved[row["uav_id"]] = row
        for kind, source in (("test_fixture", data["fixture_devices"]), ("simulated", data["simulation_uavs"])):
            for key, row in source.items():
                run = reserved.get(key) or {}
                devices[key] = (max(with_update(row), with_update(run)), {"device_id": key, "name": row.get("name", key),
                    "kind": kind, "label": row.get("label", ""), "version": row.get("version", 1),
                    "availability": "busy" if run else "available", "telemetry_source": "synthetic",
                    "telemetry_observed_at": stamp(with_update(run)), "position_fixture_id": None,
                    "mission_id": id_or_none(run.get("mission_id")), "execution_id": run.get("execution_id"),
                    "pilot_handoff_active": row.get("telemetry_source") == "mock_pilot"})
        return devices

    def list_devices(self, data, arguments):
        return [(updated, key, {field: row[field] for field in DEVICE_FIELDS})
                for key, (updated, row) in self.devices(data).items()
                if self.matching(arguments, row, ("kind", "availability"))]

    def get_device(self, data, arguments):
        return lookup(self.devices(data), arguments["device_id"])[1]

    @staticmethod
    def rehearsal_groups(data):
        groups = {}
        for row in data["executions"].values():
            if row.get("rehearsal") and row.get("plan_id") and row.get("mission_id"):
                groups.setdefault(row.get("group_id") or row["execution_id"], []).append(row)
        return groups

    @staticmethod
    def record_for(members):
        if not all(row["state"].lower() in TERMINAL and len(row.get("replay_samples", [])) >= 2 for row in members):
            return None
        return "record-" + min(row["execution_id"] for row in members)

    def rehearsal(self, key, members):
        first = members[0]
        states = {run["state"].lower() for run in members}
        # Pick an outstanding state while a group has outstanding members.
        priority = ["returning", "executing", "paused", "waiting_ack", "start_requested", "starting",
                    "preflight", "created", "scheduled", "failed", "aborted", "cancelled", "completed"]
        return {"rehearsal_id": key, "plan_id": first["plan_id"], "mission_id": first["mission_id"],
                "state": next(state for state in priority if state in states),
                "version": sum(int(run.get("control_version", 0)) for run in members),
                "review_state": "approved" if all(run.get("rehearsal_review") for run in members) else "pending",
                "record_id": self.record_for(members)}

    def list_rehearsals(self, data, arguments):
        rows = []
        for key, members in self.rehearsal_groups(data).items():
            row = self.rehearsal(key, members)
            if self.matching(arguments, row, ("plan_id", "mission_id", "state")):
                rows.append((max(map(with_update, members)), key, row))
        return rows

    def get_rehearsal(self, data, arguments):
        key = arguments["rehearsal_id"]
        members = lookup(self.rehearsal_groups(data), key)
        return {**self.rehearsal(key, members), "input_content_revision": members[0].get("input_content_revision", 0),
                "members": [{"device_id": run["uav_id"], "state": run["state"].lower(), "progress_pct": percentage(run)}
                            for run in sorted(members, key=lambda item: item["uav_id"])],
                "reason_code": next((reason(run) for run in members if reason(run)), None)}

    def list_records(self, data, arguments):
        rows = []
        rehearsals = self.rehearsal_groups(data)
        runs = [(None, key, [row]) for key, row in data["executions"].items() if not row.get("rehearsal")]
        runs.extend((key, None, members) for key, members in rehearsals.items())
        for rehearsal, execution, members in runs:
            key = self.record_for(members)
            if key is None:
                continue
            duration = max(round(1000 * row["replay_samples"][-1]["elapsed_seconds"]) for row in members)
            result = {"record_id": key, "execution_id": execution, "rehearsal_id": rehearsal,
                      "duration_ms": duration, "replay": None}
            if self.matching(arguments, result, ("execution_id", "rehearsal_id", "record_id")):
                rows.append((max(map(with_update, members)), key, result))
        return rows

    def get_health(self, data, _arguments):
        statuses = {}
        for client in data["clients"]:
            role = client["role"].lower()
            statuses[role] = statuses.get(role, False) or client["state"] == "ONLINE"
        observed = utc_timestamp(data["observed_at"])
        return {"workspace_id": self.workspace_id, "version": data["version"], "state": "ready",
                "backend_reachable": True, "last_backend_observed_at": observed, "simulation_time": observed,
                "clients": [{"role": role, "connection_state": "connected" if online else "disconnected"}
                            for role, online in sorted(statuses.items())], "reset_request": None}

    def get_context(self, data, _arguments):
        context = data["context"]
        devices = self.devices(data)
        selected = [key for key in context.get("selected_uav_ids", []) if key in devices]
        primary = context.get("active_uav_id")
        plan = context.get("active_security_plan_id")
        mission = context.get("active_mission_id")
        target = data["video"].get("video_target_uav_id")
        return {"session_id": self.session_id, "version": context["context_version"],
                "plan_id": plan if plan in data["plans"] else None,
                "mission_id": mission if plan in data["plans"] and mission in data["plans"][plan]["mission_ids"] else None,
                "device_ids": selected, "primary_device_id": primary if primary in selected else None,
                "video_target_device_id": target if target in devices else None}

    def get_preferences(self, data, _arguments):
        return {"session_id": self.session_id, "version": data["preferences"]["ui_preferences_version"],
                "language": data["preferences"]["language"]}

    def get_video(self, data, _arguments):
        target = data["video"].get("video_target_uav_id")
        target = target if target in self.devices(data) else None
        source_id = data["video"].get("mock_source_id")
        source = self.video_sources.get(source_id, {})
        source_id = source_id if target and source.get("device_id") == target else None
        return {"session_id": self.session_id, "version": data["video"]["video_view_version"],
                "target_device_id": target, "source_id": source_id,
                "playback_state": "no_source" if target and not source_id else "unreported",
                "playback_observed_at": None}

    def list_video_sources(self, data, arguments):
        devices = self.devices(data)
        if "device_id" in arguments:
            lookup(devices, arguments["device_id"])
        return [(0, key, {"source_id": key, **row}) for key, row in self.video_sources.items()
                if row["device_id"] in devices and self.matching(arguments, row, ("device_id", "availability"))]

    def list_events(self, data, arguments):
        def parse_time(field, default):
            value = arguments.get(field)
            if value is None:
                return default
            try:
                if not re.fullmatch(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{1,6})?Z", value):
                    raise ValueError("UTC required")
                return datetime.fromisoformat(value).timestamp()
            except (OverflowError, OSError, ValueError) as error:
                raise ToolFailure("INVALID_ARGUMENT", "Use an RFC 3339 UTC time") from error

        lower = parse_time("from", float("-inf"))
        upper = parse_time("to", float("inf"))
        if lower >= upper:
            raise ToolFailure("INVALID_ARGUMENT", "The event time range must be increasing")
        rows = []
        for event in data["events"]:
            when = event.get("timestamp", 0)
            if not when or not lower <= when < upper:
                continue
            category = EVENT_CATEGORIES.get(event.get("category"), "system")
            target = id_or_none(event.get("target_id"))
            if category == "execution" and target not in data["executions"]:
                continue
            code = event.get("event_type", "")
            if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,63}", code):
                continue
            key = "event-" + str(event["sequence"])
            row = {"event_id": key, "category": category, "object_id": target, "code": code,
                   "occurred_at": stamp(when), "message": code.replace("_", " ").capitalize()}
            if self.matching(arguments, row, ("category", "object_id")):
                rows.append((when, key, row))
        return rows
