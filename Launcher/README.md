# Stage 1 launcher and shared workstation profiles

For a new UE workstation follow [First run on Windows](../Docs/First-Run-Windows.md). Install UE 5.8, Cesium for Unreal, VS C++ tools and Python 3.10+ with Tk, then run `SetupDroneSecurity.cmd`. Use `DroneSecurityLauncher.cmd` to launch. A Backend-only workstation can use `SetupDroneBackend.cmd` without UE/Cesium; it still needs Python with Tk and the Backend build dependencies. See [shared-profile deployment](../Docs/LANDeployment.md) for preparation and startup order. UE is discovered through the registry/Epic manifest; optional local overrides belong in ignored `Launcher/config.local.json`. Rebuild the actual configured Backend EXE and UE DLL after source updates: a successful build in a separate review directory does not replace the default `Backend/build/Release/DroneBackend.exe` or `Binaries/Win64/UnrealEditor-UE5DroneControl.dll`.

For one computer, select **All**, leave the shared profile path empty, and click **Launch System**. To use a shared profile on one computer, select [deployment.local.example.json](deployment.local.example.json) or a local copy. The template uses four `127.0.0.1` entries, `mode=realtime`, and ports `8080/8081`. For four workstations, use [deployment.example.json](deployment.example.json), fill in each computer's LAN IPv4 address, distribute matching content, and select **Backend**, **Command**, **Map** or **Video** before clicking **Launch System** on the corresponding machine.

Role-specific dependency checks run when starting; `CheckDroneSecurity.cmd` retains the complete single-workstation checks. These checks do not verify a Cesium token, provider allowance, live map access or actual video playback. Configure your own Cesium ion connection before expecting online terrain. Earlier validation is available in the [128-test record](../Evidence/LANDeployment/20261117/validation.json) and [local integration evidence](../Evidence/LANDeployment/20261117-local/validation.json).

## Two-workstation Mock deployment

For one Windows PC running Backend and another running Command, Map, Video and Pilot, copy the DEMO template to ignored `Launcher/deployment.json`. Replace `hosts.Backend` with the Backend PC's LAN IPv4 and put the client PC's LAN IPv4 in each of the four client fields. Copy the same profile to both PCs. The Backend PC needs inbound TCP access on both shared ports from the client PC. [Two-workstation DEMO template](deployment.two-workstations.demo.example.json)

1. Prepare the current Backend build on the Backend PC and the current UE module on the client PC. Set `DRONE_PILOT_CONTROL_CREDENTIALS` to the absolute path of a private credential file on each PC before opening the launcher. Use matching Command and Pilot identities and tokens in both files.
2. On the client PC set `pilot_control_command_identity` and `pilot_control_pilot_identity` in ignored `Launcher/config.local.json` to the corresponding credential IDs. For an on-screen Mock input test, optionally set `pilot_control_mock_input` to `true`.
3. Open the launcher on the Backend PC, select the shared profile and **Auto**, and click **Launch System**. Once Backend is ready, open the launcher on the client PC, select the same profile and **Auto**, and click **Launch System**. The client PC connects to the shared Backend and starts its four UE windows.
4. For the Pilot control walkthrough, set the Mock aircraft's `video_url` to a playback page reachable from the client PC. Pilot checks for fresh video frames before enabling control. See the [two-workstation deployment steps](../Docs/LANDeployment.md#两台电脑运行驾驶员-mock) and [Mock credential format](../Backend/README.md#104-驾驶员控制权-mock).

**Auto** starts every role in the shared profile whose address belongs to the current PC. Repeat an address for roles on the same PC and use the actual LAN IPv4 for each PC in a multi-workstation deployment. A PC with no matching role reports an address mismatch. You can also pick **Backend**, **Clients** or an individual role explicitly. **Clients** manages Command, Map and Video, plus Pilot when a `demo` profile includes `hosts.Pilot`.

## Local configuration

`Launcher/config.local.json` merges over `Launcher/config.json`. It supplies local executable paths and operational settings. A shared profile supplies role IPs and two TCP ports. Use `mode=realtime` for standard deployments and the two-workstation `mode=demo` template for the Mock Pilot workflow. Same-computer roles may repeat an IP or use loopback. Use actual LAN addresses for remote computers. The selected role and profile path are saved in `Saved/Launcher/deployment-selection.json`. Keep credentials out of these files and public commits.

If Python 3.10+ with Tk is installed but `py`/`python` is not discoverable, create ignored `Launcher/python.path` containing one line with the interpreter's absolute path, for example `C:\Python313\python.exe`. Do not add quotes or JSON. This local startup override does not change system PATH; omit it when the normal `py` or `python` entry works.

| Setting | Default / purpose |
| --- | --- |
| `ue_executable` | Empty means discover UE 5.8; set an absolute `UnrealEditor.exe` path only when discovery fails |
| `backend_executable` | `Backend/build/Release/DroneBackend.exe` |
| `map` | `/Game/Level/CesiumWorld` |
| `startup_timeout` | `180` seconds; increase if first-run shader compilation delays client readiness |
| `max_fps` | `20`, applied to each UE client |
| `auto_start` | `false`, wait for **Launch System**. `true` uses the saved shared profile or the default local All setup |
| `realtime_http_endpoint`, `realtime_ws_endpoint` | Real-time: `http://127.0.0.1:8080`, `ws://127.0.0.1:8081/ws`; match the selected Backend configuration |
| `realtime_backend_config` | `Backend/config.yaml`; existing registry and `port_map` remain authoritative; local override supported |
| `realtime_runtime_directory` | `Saved/Realtime`; launcher ownership records and logs, not a new business database |
| `realtime_map_location_source` | `computer` uses the Map workstation's Windows location when coordinates are blank. Set `uav` to use UAV GPS priority |
| `realtime_gps_timeout` | `60` seconds to obtain a valid GPS fix in `uav` mode when a real UAV is connected |
| `realtime_gps_max_age` | `5.0` seconds maximum age of Backend's packet receipt time for `uav` mode |
| `realtime_device_wait` | `8` seconds in `uav` mode to discover a connected real UAV before considering computer location |
| `realtime_computer_location_timeout` | `15` seconds to obtain Windows system location |

All clients in a shared deployment use `hosts.Backend` for HTTP/WS. The launcher checks role IPs with temporary local UDP binds. **Clients** checks the client role addresses. **All** checks all selected addresses and starts Backend before the clients. On separate machines, start the workstation that owns Backend first. Runtime records use `Saved/Deployment/{mode}/{selection}`. The shared ports set the Backend process's TCP listeners. The realtime Backend configuration supplies its storage, UDP mappings, and execution settings. Configure `video_url` and media transport separately for the computer running Video or Pilot. Restart affected sessions after changing the profile. See the [deployment guide](../Docs/LANDeployment.md).

The launcher does not append `-httpproxy` or support a custom `extra_args` / `proxy` JSON field. If the workstation needs an HTTP proxy, close its UE editor and clients, then merge the following into the project's ignored `Saved/Config/WindowsEditor/Engine.ini` and restart the clients. Replace the example address with an actual running HTTP proxy; preserve other INI sections.

```ini
[HTTP]
HttpProxyAddress=127.0.0.1:7897
HttpNoProxy=127.0.0.1,localhost,::1
```

This path applies to the current `UnrealEditor.exe -game` launcher. A direct UE command line can instead specify `-httpproxy=127.0.0.1:7897`. Keep the bypass list so local Backend HTTP requests remain direct, and add the actual Backend LAN IP when using separate computers. Verify the selected proxy in the Map client log and confirm real terrain appears; merely having a proxy process running is insufficient. Set `HttpProxyAddress=` to request direct access, or remove the key to restore the engine's OS-proxy fallback. Do not put a workstation proxy into shared project defaults. See the [Chinese map configuration instructions](../Docs/First-Run-Windows.md#4-设置地图服务) for the full sequence.

Cesium retains up to `131072` request cache entries after pruning and checks pruning every `10000` requests. New UE processes read this policy and observe response cache headers and expiry. Normal online use has `UseLocalTileServer=false`. See [startup and cache details](../Docs/MarinaBayStartup.md).

## Map locations

Leave latitude and longitude blank to open Map at the Map workstation's Windows location by default. Enter decimal coordinates to choose a different starting location. Latitude must be within `±85.05112878°` and longitude within `±180°`. To save the coordinates, enter an optional name and click **Add favorite**. A blank name displays the coordinates. Selecting a favorite fills its coordinates. **Remove selected** deletes the entry from `Saved/Launcher/map-locations.json`.

To change the position during a session, apply coordinates with **Switch location**. The launcher restarts only its Map process. Startup coordinates set the Map view. Aircraft positions, Home, and saved waypoints use their own operational coordinates.

Mock fixtures, including the Mock Pilot, remain available for internal QA. [Mock control setup](../Backend/README.md#104-驾驶员控制权-mock)

## Mission rehearsals

With Command and Map connected to the realtime Backend, open the Command fleet list and choose **Add simulated aircraft**. Set a batch size and start location, or choose a Launcher favorite. A rehearsal can run with an empty physical fleet.

Assign simulated aircraft to a task in the Security Plan workspace, save its route in Map, then choose **Run rehearsal** on the task. Map shows live progress and lets you play or scrub completed rehearsal records. Changes to the saved route or formation require a new rehearsal before the task can be reviewed and assigned.

After reviewing a completed single-aircraft rehearsal, select a registered physical aircraft and save the pending assignment. Command opens a new draft linked to the rehearsal. The draft must pass the existing real-aircraft checks, review and deployment before a separate start confirmation.

[Native UI and automated validation record](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

## Starting and stopping

**Launch System** uses local ports `8080/8081` and `Backend/config.yaml` by default. The launcher starts or attaches to Backend, validates its APIs, and starts the selected UE processes. Readiness depends on the local role set. Whenever a realtime session includes Map, the launcher resolves the location before launching Map. Shut down the current session before changing deployment settings.

### Real-time position initialization

Map startup opens the **2D basemap** at the selected or automatically resolved location. Use the toolbar to switch to **3D** where data is available.

The real-time Backend runs with the repository root as its working directory. Its selected configuration supplies the registry, UDP `port_map`, storage and existing execution policy. The shipped configuration uses `Backend/data/drones.json`; adjacent Backend state stays with that configured storage path. `Saved/Realtime` contains launcher ownership records, launcher logs, captured Backend stdout/stderr and UE absolute logs. Backend's own file log follows the selected configuration's `log.file` (default `logs/backend.log`). This branch does not create a Mock fixture, synthetic UAV registrations or a startup reset, and the launch button does not create or enable `real_execution.json` or start a flight task.

With blank coordinates, the default `computer` source queries Windows system location directly, including when a real UAV is connected. The lookup allows up to `realtime_computer_location_timeout` (15 seconds by default). If Windows omits altitude, Map uses an estimated `0 m` camera anchor and identifies the elevation as estimated.

For UAV GPS priority, set `realtime_map_location_source` to `uav` in the Map workstation's ignored `Launcher/config.local.json` and configure the actual UAV connections and registry entries. A valid UAV GPS fix can initialize immediately. A qualifying record from `/api/drones` has `status=online`, `gps_fix=true`, `simulation=false` and `telemetry_source=received`, valid finite geographic coordinates other than `(0,0)` and a finite Backend `telemetry_received_at` no more than five seconds old by default. Backend records receipt time when UDP telemetry arrives. Among qualifying aircraft, the active UAV is preferred, otherwise the lowest numeric slot is used. GPS altitude is converted from AMSL to WGS84 ellipsoid height using `GeoidSeparationMeters` from `Config/DefaultGame.ini`, overridden by `Saved/Config/WindowsEditor/Game.ini`, matching the UE client.

With `uav` selected, a real `online` or `connecting` connection with invalid GPS keeps the launcher waiting for a fix up to `realtime_gps_timeout` (60 seconds by default). A real connection requires Backend-received non-simulation telemetry. When that connection is absent after `realtime_device_wait` (eight seconds by default), the launcher queries Windows system location.

Disabled, denied, unavailable or timed-out Windows location produces a clear error. The launcher does not automatically change location permissions, guess a city from the IP address or fall back to Singapore. A failed position lookup closes only launcher-owned processes and opens no UE role. Rebuild an older Backend through `SetupDroneSecurity.cmd` if it lacks the `telemetry_receive_time` capability. Once initialized, new GPS or a change of location source does not automatically move the camera; restarting Map resolves the location again with the same priority. Synthetic tests of these rules do not establish native computer-location, GPS accuracy, physical-flight or map-alignment acceptance.

Command occupies the left half of the working area. Map and Video share the right half. These are normal resizable windows on one screen. Use **Focus** for detailed work and **Arrange Windows** to restore the overview. No monitor enumeration, display IDs, DPI mapping or borderless mode is required.

Use **Shutdown System** for normal session exit. Handle active executions through the existing operational controls before stopping a session. **Stop / Restart** manages individual owned components. Reused external Backend processes remain under their original owner's control. The launcher tracks each owned process tree through a Windows job. Shutdown requests WM_CLOSE, waits eight seconds, then terminates the retained process handle when necessary. Business mutations are persisted when acknowledged.

The [historical DEMO runbook](../Docs/Stage1/Stage1-Demo-Runbook.md) and [grass startup acceptance](../Evidence/GrassStartup/20261023/acceptance.json) document the earlier Mock reset behavior and the former launcher UI.

3D fleet movement requires Backend capabilities `fleet_move_3d` and `map_fleet_controls`, advertised by `GET /`. Readiness checks both newly started and reused Backends before starting any client. An older executable can have valid plan/context snapshots while still rejecting Map fleet movement with `ROLE_FORBIDDEN`; it is now rejected with a rebuild instruction instead. Rebuild `Backend/build/Release/DroneBackend.exe` through `SetupDroneSecurity.cmd` after source updates, stop the old running Backend, and relaunch. `Scripts/FleetBackendCompatibility/verify.py` tests the resolved normal Launcher configuration with isolated Mock data and ports; it does not substitute a phase-specific backend build.

Checks: `python -B -m unittest discover -s Launcher -p "test_*.py" -v`. `record_acceptance.py` is a read-only native acceptance oracle; it does not drive UI or simulate client recovery.

The [2026-10-30 acceptance record](../Evidence/LauncherModes/2026-10-30/preview.md) separates 63 Launcher checks, isolated Backend checks and actual button-driven Map inspection. DEMO and the no-aircraft Windows-location branch were checked in native windows with isolated Backend data. Real UAV GPS, physical flight and nonzero-geoid marker/focus height still require dedicated validation; the record preserves earlier module identities and failed attempts. Precise computer-location screenshots and raw native coordinate logs remain local.

P5.3: Route editing exposes Closed Route (at least three waypoints). Move Plan is available in the Command plan workspace for idle plans, including deployed plans. In Map, drag the orange center handle, then Confirm Position or Cancel. Closed routes execute once in Stage 1 Mock mode. Running/paused/returning missions block moving.
