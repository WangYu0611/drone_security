"""Schemas and transport for Backend-owned Mock plan transactions."""

import httpx

from protocol import BOOLEAN, COUNTER, ID, STRING, TIMESTAMP, UUID, ToolFailure
from protocol import array, choices, nullable, object_schema, tool, utc_timestamp
from workspace import PLAN_STATE

VALIDATE = "validate_mock_plan"
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
}


class PlanWriter:
    def __init__(self, backend_port, bridge_token):
        self.endpoint = f"http://127.0.0.1:{backend_port}/api/mock-mcp/plan-write"
        self.token = bridge_token

    async def call(self, identity, name, arguments):
        normalized = dict(arguments)
        try:
            if "name" in normalized:
                value = normalized["name"].strip(" \t\r\n\f\v")
                if not 1 <= len(value.encode("utf-8")) <= 160:
                    raise ToolFailure("INVALID_ARGUMENT", "Use a name between 1 and 160 UTF-8 bytes")
                normalized["name"] = value
            if len(normalized.get("description", "").encode("utf-8")) > 1000:
                raise ToolFailure("INVALID_ARGUMENT", "Use a description of at most 1,000 UTF-8 bytes")
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
                return payload, confirmed
            code = BACKEND_ERRORS.get(payload.get("code"))
            if code:
                raise ToolFailure(code, f"Backend refused the Mock plan operation ({code})")
        except (httpx.HTTPError, ValueError, TypeError, KeyError, AttributeError):
            pass
        raise ToolFailure("MOCK_UNAVAILABLE", "Mock Backend unavailable; retry with the same request_id")
