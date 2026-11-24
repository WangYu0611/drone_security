"""Shared schemas and error vocabulary for the Mock MCP contract."""

from datetime import UTC, datetime

from mcp import types

ID_PATTERN = r"^[A-Za-z0-9_-]{1,64}(?![\s\S])"
UUID_PATTERN = r"^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}(?![\s\S])"
STRING = {"type": "string"}
ID = {"type": "string", "pattern": ID_PATTERN}
UUID = {"type": "string", "pattern": UUID_PATTERN}
COUNTER = {"type": "integer", "minimum": 0, "maximum": (1 << 53) - 1}
TIMESTAMP = {"type": "string", "format": "date-time"}
BOOLEAN = {"type": "boolean"}
ENVELOPE = {"contract_version": {"const": "0.2"}, "simulation": {"const": True}, "observed_at": TIMESTAMP}


def object_schema(properties, optional=()):
    return {
        "type": "object", "properties": properties,
        "required": [name for name in properties if name not in optional],
        "additionalProperties": False,
    }


def nullable(schema):
    return {"anyOf": [schema, {"type": "null"}]}


def array(items, **constraints):
    return {"type": "array", "items": items, **constraints}


def choices(*values):
    return {"enum": list(values)}


ERROR = object_schema({
    **ENVELOPE, "error": object_schema(
        {"code": STRING, "message": STRING, "field": STRING, "current_version": COUNTER}, ("field", "current_version")),
})


def tool(name, description, inputs, fields, optional=(), *, read_only=True, result_alternatives=()):
    return types.Tool(
        name=name,
        description=description,
        input_schema=object_schema(inputs, optional),
        output_schema={"type": "object", "oneOf": [
            *(object_schema({**ENVELOPE, **result}) for result in (fields, *result_alternatives)), ERROR,
        ]},
        annotations=types.ToolAnnotations(read_only_hint=read_only, open_world_hint=False),
    )


def utc_timestamp(epoch=None):
    now = datetime.now(UTC) if epoch is None else datetime.fromtimestamp(epoch, UTC)
    return now.isoformat(timespec="milliseconds").replace("+00:00", "Z")


class ToolFailure(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code
