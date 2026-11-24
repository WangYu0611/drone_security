"""Schemas and transport for Backend-owned Mock plan transactions."""

import json
import math
import re

import httpx
from jsonschema import Draft202012Validator

from protocol import BOOLEAN, COUNTER, ID, STRING, TIMESTAMP, UUID, ToolFailure
from protocol import array, choices, nullable, object_schema, tool, utc_timestamp
from workspace import PLAN_STATE, RUN_STATE

VALIDATE = "validate_mock_plan"
BEGIN_ROUTE = "begin_mock_route_edit"
SAVE_ROUTE = "save_mock_route_draft"
END_ROUTE = "end_mock_route_edit"
DELETE_PLAN = "delete_mock_security_plan_draft"
DELETE_MISSION = "delete_mock_plan_mission"
REVIEW = "record_mock_plan_review"
DEPLOY = "publish_mock_plan_snapshot"
CREATE_DEVICES = "create_mock_simulated_devices"
UPDATE_DEVICE = "update_mock_device_metadata"
REMOVE_DEVICE = "remove_mock_test_device"
START_REHEARSAL = "start_mock_rehearsal"
REVIEW_REHEARSAL = "record_mock_rehearsal_review"
FLEET_MANAGE = "mock:fleet:manage"
NAME = {"type": "string", "minLength": 1}
DESCRIPTION = {"type": "string", "maxLength": 1000}
VERSIONED_PLAN = {"plan_id": ID, "expected_plan_version": COUNTER, "request_id": UUID}
CONFIGURATION = {**VERSIONED_PLAN, "configuration_id": ID}
RESULT = {"plan_id": ID, "plan_version": COUNTER, "content_revision": COUNTER, "request_id": UUID}
DRAFT = {**RESULT, "state": {"const": "draft"}}
TASK = {**RESULT, "mission_id": ID}


def write(name, description, params, result, optional=()):
    return tool(name, description, params, result, optional, read_only=False)


PLAN_WRITE_TOOLS = {item.name: item for item in (
    write("begin_mock_plan_edit", "Acquire a configuration lease on a Mock plan.", VERSIONED_PLAN,
          {**RESULT, "configuration_id": ID, "lease_expires_at": TIMESTAMP}),
    write("end_mock_plan_edit", "Release your current Mock plan configuration.", {"configuration_id": ID, "request_id": UUID},
          {"plan_id": ID, "configuration_id": ID, "request_id": UUID, "released": {"const": True}}),
    write("update_mock_security_plan_draft", "Update the name or description of an editable Mock draft.",
          {**CONFIGURATION, "name": NAME, "description": DESCRIPTION},
          {**DRAFT, "name": STRING, "description": STRING}, ("name", "description")),
    write("copy_mock_security_plan_draft", "Copy a Mock plan into an independent draft.",
          {"source_plan_id": ID, "expected_source_plan_version": COUNTER, "name": NAME, "request_id": UUID},
          {**DRAFT, "source_plan_id": ID, "name": STRING, "mission_count": COUNTER}, ("name",)),
    write("reopen_mock_security_plan", "Reopen an inactive Mock deployment as a draft.", VERSIONED_PLAN,
          {**DRAFT, "previous_deployment_id": nullable(ID)}),
    write("add_mock_plan_mission", "Add an empty task to your Mock draft.", {**CONFIGURATION, "name": NAME},
          {**TASK, "name": STRING, "device_ids": array(ID), "route_version": COUNTER}),
    write("rename_mock_plan_mission", "Rename an existing Mock task.",
          {**CONFIGURATION, "mission_id": ID, "name": NAME}, {**TASK, "name": STRING}),
    write("set_mock_mission_assignment", "Assign simulated devices to your Mock task, or clear the assignment.",
          {**CONFIGURATION, "mission_id": ID, "device_ids": array(ID, uniqueItems=True, maxItems=20)},
          {**TASK, "device_ids": array(ID)}),
    write(VALIDATE, "Run Backend configuration checks and return blocking issues.", CONFIGURATION,
          {**RESULT, "validation_id": ID, "state": PLAN_STATE, "passed": BOOLEAN, "checks": array(object_schema({
              "code": STRING, "message": STRING, "severity": choices("blocking", "warning"),
          }))}),
)}

APPROVAL_SCOPES = {DELETE_PLAN: "mock:plan:delete", DELETE_MISSION: "mock:plan:delete",
                   REVIEW: "mock:plan:review", DEPLOY: "mock:plan:deploy", REMOVE_DEVICE: FLEET_MANAGE,
                   REVIEW_REHEARSAL: "mock:rehearsal:review"}
APPROVAL_TOOLS = {item.name: item for item in (
    write(DELETE_PLAN, "Delete a draft after operator approval for this version.",
          {**VERSIONED_PLAN, "approval_id": ID},
          {"request_id": UUID, "plan_id": ID, "deleted": {"const": True}}, ("approval_id",)),
    write(DELETE_MISSION, "Delete an editable task after operator approval for this version.",
          {**CONFIGURATION, "mission_id": ID, "approval_id": ID},
          {**TASK, "deleted": {"const": True}}, ("approval_id",)),
    write(REVIEW, "Record an operator-confirmed review of a passed validation.",
          {**CONFIGURATION, "validation_id": ID, "approval_id": ID},
          {**RESULT, "review_id": ID, "validation_id": ID, "state": {"const": "ready"}}, ("approval_id",)),
    write(DEPLOY, "Freeze a reviewed plan into a Mock deployment snapshot after operator approval.",
          {**CONFIGURATION, "review_id": ID, "approval_id": ID},
          {**RESULT, "deployment_id": ID, "state": {"const": "deployed"}, "configuration_released": BOOLEAN}, ("approval_id",)),
    write(REMOVE_DEVICE, "Remove an unused software simulator after operator approval.",
          {"device_id": ID, "expected_version": COUNTER, "request_id": UUID, "approval_id": ID},
          {"device_id": ID, "request_id": UUID, "removed": {"const": True}}, ("approval_id",)),
    write(REVIEW_REHEARSAL, "Record operator approval for a completed software rehearsal.",
          {"rehearsal_id": ID, "expected_version": COUNTER, "request_id": UUID, "approval_id": ID},
          {"rehearsal_id": ID, "request_id": UUID, "version": COUNTER, "state": {"const": "completed"},
           "review_state": {"const": "approved"}, "reviewed_at": TIMESTAMP}, ("approval_id",)),
)}

SIMULATION_SCOPES = {CREATE_DEVICES: FLEET_MANAGE, UPDATE_DEVICE: FLEET_MANAGE, START_REHEARSAL: "mock:rehearsal:run"}
SIMULATION_TOOLS = {item.name: item for item in (
    write(CREATE_DEVICES, "Add software simulators from an approved device fixture.",
          {"device_fixture_id": ID, "count": {"type": "integer", "minimum": 1, "maximum": 20}, "request_id": UUID},
          {"request_id": UUID, "created_count": COUNTER, "devices": array(object_schema({
              "device_id": ID, "name": STRING, "kind": {"const": "simulated"}, "version": COUNTER,
          }))}),
    write(UPDATE_DEVICE, "Set the display name or label of a software simulator.",
          {"device_id": ID, "expected_version": COUNTER, "name": NAME,
           "label": {"type": "string", "maxLength": 160}, "request_id": UUID},
          {"device_id": ID, "version": COUNTER, "name": STRING, "label": STRING, "request_id": UUID}, ("name", "label")),
    write(START_REHEARSAL, "Start a rehearsal that uses only assigned software simulators.",
          {"plan_id": ID, "mission_id": ID, "expected_content_revision": COUNTER, "request_id": UUID},
          {"rehearsal_id": ID, "plan_id": ID, "mission_id": ID, "request_id": UUID, "version": COUNTER,
           "state": RUN_STATE, "input_content_revision": COUNTER, "review_state": {"const": "pending"}}),
)}

ROUTE_WRITE_TOOLS = {item.name: item for item in (
    write(BEGIN_ROUTE, "Acquire the route editor session under your Mock configuration lease.",
          {"plan_id": ID, "mission_id": ID, "configuration_id": ID, "expected_route_version": COUNTER, "request_id": UUID},
          {"plan_id": ID, "mission_id": ID, "route_edit_id": ID, "route_version": COUNTER,
           "lease_expires_at": TIMESTAMP, "request_id": UUID}),
    write(SAVE_ROUTE, "Save a route assembled from authorized synthetic fixture points.",
          {"route_edit_id": ID, "expected_route_version": COUNTER, "map_fixture_id": ID,
           "waypoint_fixture_ids": array(ID, maxItems=100), "closed_loop": BOOLEAN, "request_id": UUID},
          {"mission_id": ID, "route_edit_id": ID, "route_version": COUNTER, "content_revision": COUNTER,
           "state": {"const": "saved"}, "waypoint_count": COUNTER, "edit_open": {"const": True}, "request_id": UUID},
          ("closed_loop",)),
    write(END_ROUTE, "Finish or discard unsaved work in your Mock route edit session.",
          {"route_edit_id": ID, "disposition": choices("finish", "discard_unsaved"),
           "expected_route_version": COUNTER, "request_id": UUID},
          {"mission_id": ID, "route_edit_id": ID, "route_version": COUNTER, "edit_open": {"const": False},
           "disposition": choices("finish", "discard_unsaved"), "request_id": UUID}, ("expected_route_version",)),
)}

BACKEND_ERRORS = {
    "PLAN_NOT_FOUND": "NOT_FOUND", "MISSION_NOT_FOUND": "NOT_FOUND", "MISSION_PLAN_MISMATCH": "NOT_FOUND",
    "INVALID_UAV": "NOT_FOUND", "NOT_FOUND": "NOT_FOUND", "PLAN_NOT_DEPLOYED": "INVALID_STATE",
    "PLAN_DEPLOYED": "INVALID_STATE", "INVALID_STATE": "INVALID_STATE", "PLAN_EXECUTING": "OBJECT_IN_USE",
    "UAV_ALREADY_ASSIGNED": "OBJECT_IN_USE", "DRONE_ACTIVE_PLAN_CONFLICT": "OBJECT_IN_USE",
    "INVALID_ARGUMENT": "INVALID_ARGUMENT", "INVALID_NAME": "INVALID_ARGUMENT",
    "VERSION_CONFLICT": "VERSION_CONFLICT", "REQUEST_ID_CONFLICT": "REQUEST_ID_CONFLICT",
    "CONTENT_REVISION_CONFLICT": "VERSION_CONFLICT", "CONFIGURATION_EXPIRED": "CONFIGURATION_EXPIRED",
    "CONFIGURATION_IN_PROGRESS": "CONFIGURATION_IN_PROGRESS", "PLAN_EDIT_IN_PROGRESS": "EDIT_IN_PROGRESS",
    "EDIT_SESSION_CONFLICT": "EDIT_IN_PROGRESS", "MIXED_MISSION_ASSIGNMENT": "INVALID_ARGUMENT",
    "FIXTURE_NOT_FOUND": "FIXTURE_NOT_FOUND", "ROUTE_TOO_SHORT": "CHECK_FAILED", "INVALID_WAYPOINT": "CHECK_FAILED",
    "APPROVAL_INVALID": "APPROVAL_INVALID", "APPROVAL_REQUIRED": "APPROVAL_REQUIRED", "CHECK_FAILED": "CHECK_FAILED",
    "PLAN_NOT_READY": "CHECK_FAILED", "VALIDATION_STALE": "CHECK_FAILED", "REVIEW_REQUIRED": "REVIEW_REQUIRED",
    "REVIEW_STALE": "REVIEW_REQUIRED",
    "OBJECT_IN_USE": "OBJECT_IN_USE", "SIMULATION_FLEET_FULL": "LIMIT_EXCEEDED", "SIMULATION_ID_CONFLICT": "OBJECT_IN_USE",
    "SIMULATION_ORIGIN_OCCUPIED": "OBJECT_IN_USE", "SIMULATION_INVALID_COUNT": "INVALID_ARGUMENT",
    "SIMULATION_INVALID_ORIGIN": "INVALID_ARGUMENT", "SIMULATION_UAV_REQUIRED": "CHECK_FAILED", "ROUTE_MISSING": "CHECK_FAILED",
    "EXECUTION_CONFLICT": "OBJECT_IN_USE", "REHEARSAL_INCOMPLETE": "INVALID_STATE", "REHEARSAL_STALE": "REHEARSAL_STALE",
}


def load_fixtures(path):
    if path is None:
        return {"maps": {}, "devices": {}}
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) - {"maps", "devices"}:
        raise ValueError("A Mock fixtures file supports maps and devices objects")
    maps, devices = data.get("maps", {}), data.get("devices", {})
    if not isinstance(maps, dict) or not isinstance(devices, dict):
        raise ValueError("Fixture sections must be JSON objects")
    schema = object_schema({"latitude": {"type": "number", "minimum": -90, "maximum": 90},
                            "longitude": {"type": "number", "minimum": -180, "maximum": 180},
                            "altitude": {"type": "number", "minimum": -1000, "maximum": 10000}})
    point_check = Draft202012Validator(schema)

    def check_point(point_id, point):
        if not re.fullmatch(ID["pattern"], point_id) or not point_check.is_valid(point) or not all(map(math.isfinite, point.values())):
            raise ValueError("Invalid synthetic fixture point")

    for key, points in maps.items():
        if not re.fullmatch(ID["pattern"], key) or not isinstance(points, dict):
            raise ValueError("Invalid Mock fixture map")
        for point_id, point in points.items():
            check_point(point_id, point)
    for device_id, origin in devices.items():
        check_point(device_id, origin)
    return {"maps": maps, "devices": devices}


class PlanWriter:
    def __init__(self, backend_port, bridge_token, fixtures):
        self.endpoint = f"http://127.0.0.1:{backend_port}/api/mock-mcp/plan-write"
        self.token = bridge_token
        self.fixtures = fixtures

    def resolve_route(self, arguments):
        points = self.fixtures["maps"].get(arguments["map_fixture_id"])
        keys = arguments["waypoint_fixture_ids"]
        if points is None or any(key not in points for key in keys):
            # The Backend checks receipts before validating a resolved fixture.
            # An unresolvable new request fails, while an earlier receipt survives.
            return None
        return {"map_fixture_id": arguments["map_fixture_id"], "waypoint_fixture_ids": keys,
                "bClosedLoop": arguments["closed_loop"], "waypoints": [
                    {**points[key], "sequence": i, "altitude_reference": "Ellipsoid", "segmentSpeed": 0, "waitTime": 0}
                    for i, key in enumerate(keys, start=1)
                ]}

    async def call(self, identity, name, arguments):
        normalized = {key: int(value) if key.startswith("expected_") or key == "count" else value
                      for key, value in arguments.items()}
        if name == SAVE_ROUTE:
            normalized.setdefault("closed_loop", False)
            normalized["resolved_fixture_path"] = self.resolve_route(normalized)
        elif name == END_ROUTE and (normalized["disposition"] == "finish") != ("expected_route_version" in normalized):
            raise ToolFailure("INVALID_ARGUMENT", "Supply expected_route_version only when finishing")
        elif name == CREATE_DEVICES:
            normalized["resolved_device_origin"] = self.fixtures["devices"].get(arguments["device_fixture_id"])
        try:
            if "name" in normalized:
                value = normalized["name"].strip(" \t\r\n\f\v")
                if not 1 <= len(value.encode("utf-8")) <= 160:
                    raise ToolFailure("INVALID_ARGUMENT", "Use a name between 1 and 160 UTF-8 bytes")
                normalized["name"] = value
            if len(normalized.get("description", "").encode("utf-8")) > 1000:
                raise ToolFailure("INVALID_ARGUMENT", "Use a description of at most 1,000 UTF-8 bytes")
            if len(normalized.get("label", "").encode("utf-8")) > 160:
                raise ToolFailure("INVALID_ARGUMENT", "Use a label of at most 160 UTF-8 bytes")
        except UnicodeError as error:
            raise ToolFailure("INVALID_ARGUMENT", "Use valid UTF-8 text") from error
        try:
            async with httpx.AsyncClient(timeout=5, trust_env=False) as http:
                response = await http.post(self.endpoint, headers={"Authorization": f"Bearer {self.token}"},
                                           json={"identity": identity.name, "tool": name, "arguments": normalized})
            payload = response.json()
            if response.status_code == 200:
                confirmed = utc_timestamp(payload.pop("confirmed_at"))
                if "lease_expires_at" in payload:
                    payload["lease_expires_at"] = utc_timestamp(payload["lease_expires_at"])
                if "reviewed_at" in payload:
                    payload["reviewed_at"] = utc_timestamp(payload["reviewed_at"])
                return payload, confirmed
            code = BACKEND_ERRORS.get(payload.get("code"))
            if code:
                raise ToolFailure(code, f"Backend refused the Mock plan operation ({code})")
        except (httpx.HTTPError, ValueError, TypeError, KeyError, AttributeError):
            pass
        raise ToolFailure("MOCK_UNAVAILABLE", "Mock Backend unavailable; retry with the same request_id")
