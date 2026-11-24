# Drone Security Mock MCP

The gateway serves the implemented tools from [the v0.2 integration contract](../../Docs/Mock-MCP-Integration.md).
The isolated Backend owns the workspace query data and stores drafts together with their receipts.
Command receives its ordinary plan snapshot and update. MCP owns the playback clocks and workspace
record replay receipts.

## Run an isolated workspace

1. Build DroneBackend with the standard Backend build. Create a fresh local workspace outside
   tracked files. Set the Backend configuration's storage path to that workspace's synthetic
   registry, with an empty JSON array for an empty fleet. Put a mock_execution.json containing
   {"simulation": true} alongside the registry. Use different free HTTP and WebSocket ports,
   disable debug, omit port_map and keep video metadata storage inside the workspace.
2. Generate a random 32-byte hex bridge secret with a trusted process. Set
   DRONE_MOCK_MCP_BRIDGE_TOKEN for the Backend and the MCP gateway. With this option enabled the
   Backend requires the explicit Mock file, refuses any real_execution.json or UDP mappings,
   and binds HTTP and WebSocket to 127.0.0.1. Use local Command with those same ports for handoff.
3. Give each MCP client a different random 32-byte hex token. Keep only its lowercase SHA-256
   digest in the gateway identities file. The file contains an identities array of objects with
   id, token_sha256 and scopes. All identities need mock:read. Add mock:replay, mock:plan:draft or
   mock:plan:validate as needed. Keep this file and all tokens outside Git. Client tokens and the bridge secret are
   distinct. The gateway binds to loopback.

Install and start the gateway in a Python 3.11+ virtual environment.

~~~powershell
python -m pip install -r tools/mock_mcp/requirements.txt
python tools/mock_mcp/server.py --identities <identities.json> --backend-port <backend-http> --http-port <mcp-http>
~~~

Point the local MCP client at http://127.0.0.1:<mcp-http>/mcp and send the client's bearer token.
The gateway checks the token on each request, filters discovery by scope, and returns 401 or 403
for invalid tokens or missing tool scopes. Cross-machine TLS and OAuth need an organization-managed
deployment. This package validates local bearer tokens on loopback.

For stdio, omit --http-port and pass DRONE_MOCK_MCP_CLIENT_TOKEN to the subprocess. A read or
replay-only stdio process can use fixed demo tools without the bridge secret. Workspace tools appear
in discovery when the process has the bridge secret. The stdio draft identity is for a trusted
local host that can hold the bridge secret. Use the HTTP gateway when the client must receive only
its own token.

Assign authorized clients the `--workspace-id` and `--session-id` used at gateway startup, or use
the defaults `mock-local` and `mock-session`. Deploy each workspace with its own Backend, identities,
bridge secret and state database. `--state-db` sets the replay database path. By default it is named
`mock-mcp-replay.sqlite3` alongside the identities file. Keep it outside Git with the workspace data.

## Behavior and acceptance

- The fixed recording is demo-a. A replay identity has its own playback clock and version for the
  lifetime of the MCP process. Read-only identities see replay as null. An MCP replay never changes
  Backend execution state. Workspace-record playback depends on the later record-opening tools.
- Submit the draft request ID once per logical creation. Configuration conflicts retain the reserved
  request parameters. Repeat with the same ID and parameters after the operator releases configuration.
  Draft persistence and release are one Backend transaction, so a successful receipt has no MCP
  lease left to release. The same receipt is returned following a timeout or Backend restart.
- `begin_mock_plan_edit` returns a configuration ID. Pass it with the current plan version when
  editing that plan, and call `end_mock_plan_edit` to free the lease. The Backend grants the lease
  for up to 120 seconds of inactivity and renews it after a successful operation on that lease.
  The plan and mutation receipt are committed together. A completed retry returns its first receipt.
- The automated acceptance uses temporary test credentials, an empty registry and isolated ports.
  It exercises the MCP protocol and the Command HTTP/WebSocket handoff. Native Command interaction
  is a separate acceptance check.

~~~powershell
python -m pip install "websockets>=15,<17"
python tools/mock_mcp/verify.py --backend Backend/build/Release/DroneBackend.exe
~~~

For an unattended Command UI check, build the Development Editor target and run the following with a new evidence directory. The verifier creates the draft through MCP, clicks the rendered Command buttons through Slate hit testing, and independently observes the lease acquisition and release on the isolated Backend.

~~~powershell
python tools/mock_mcp/verify_ui.py --backend Backend/build/Release/DroneBackend.exe --editor 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' --evidence Saved/MockMcpDraftUi
~~~

The two screenshots and `result.json` record the rendered UI and Backend outcome. Testing the desktop Computer Use driver and Windows input delivery is a separate check.
