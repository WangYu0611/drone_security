# Stage 1 single-screen launcher

For a new workstation follow [First run on Windows](../Docs/First-Run-Windows.md). Install UE 5.8, Cesium for Unreal, VS C++ tools and Python 3.10+ with Tk, then run `SetupDroneSecurity.cmd`. Use `CheckDroneSecurity.cmd` for actionable diagnostics and `DroneSecurityLauncher.cmd` to launch. UE is discovered through the registry/Epic manifest; optional local overrides belong in ignored `Launcher/config.local.json`.

Reviewed against the implementation on 2026-10-28. `CheckDroneSecurity.cmd` checks installed dependencies, resolved paths, LFS assets and build outputs; it does not verify a Cesium token, provider allowance, live map access or actual video playback. Configure your own Cesium ion connection in the UE editor before expecting online terrain. The default online source is Google Photorealistic 3D Tiles through Cesium ion; this path does not require a separate Google Maps API key. Mock operation needs no physical aircraft connection or video URL.

## Local configuration

`Launcher/config.local.json` merges over `Launcher/config.json`. Only set values that differ on the workstation; keep credentials out of this file and public commits.

| Setting | Default / purpose |
| --- | --- |
| `ue_executable` | Empty means discover UE 5.8; set an absolute `UnrealEditor.exe` path only when discovery fails |
| `backend_executable` | `Backend/build/Release/DroneBackend.exe` |
| `map` | `/Game/Level/CesiumWorld` |
| `http_endpoint`, `ws_endpoint` | `http://127.0.0.1:19880`, `ws://127.0.0.1:19881/ws`; newly owned Stage 1 Backends must be local |
| `startup_timeout` | `180` seconds; increase if first-run shader compilation delays client readiness |
| `max_fps` | `20`, applied to each UE client |
| `auto_start` | `true`; false waits for Start System |
| `mock_reset_on_start` | `true`; controls the new owned Mock Backend reset described below |

The launcher does not append `-httpproxy` or support a custom `extra_args` / `proxy` JSON field. If the workstation needs an HTTP proxy, close its UE editor and clients, then merge the following into the project's ignored `Saved/Config/WindowsEditor/Engine.ini` and restart the clients. Replace the example address with an actual running HTTP proxy; preserve other INI sections.

```ini
[HTTP]
HttpProxyAddress=127.0.0.1:7897
HttpNoProxy=127.0.0.1,localhost,::1
```

This path applies to the current `UnrealEditor.exe -game` launcher. A direct UE command line can instead specify `-httpproxy=127.0.0.1:7897`. Keep the bypass list so local Backend HTTP requests remain direct. Verify the selected proxy in the Map client log and confirm real terrain appears; merely having a proxy process running is insufficient. Set `HttpProxyAddress=` to request direct access, or remove the key to restore the engine's OS-proxy fallback. Do not put a workstation proxy into shared project defaults. See the [Chinese map configuration instructions](../Docs/First-Run-Windows.md#4-设置地图服务) for the full sequence.

Map startup, UAV formation and Cesium request caching already have shared defaults. The map starts in 3D over The Meadow; `[MapStartup]` in `Config/DefaultGame.ini` controls only presentation, while `startup_formation` in `Backend/mock_execution.json` controls the Mock reset. Changing one does not relocate the other or saved plans. Cesium retains up to `131072` cache entries after pruning and checks pruning every `10000` requests. The new policy is read by new UE processes, observes response cache headers and expiry, and is not an offline region package or a byte limit. Normal online use has `UseLocalTileServer=false` and needs no separate tile server. See [startup and cache details](../Docs/MarinaBayStartup.md).

## Starting and stopping

The launcher starts Backend first and validates the existing context, preferences, video and plan APIs before starting three independent instances of the same UE project. READY requires the expected instance for every role, a live owned process, a subscribed session and a fresh hydrated heartbeat.

Command occupies the left half of the working area. Map and Video share the right half. These are normal resizable windows on one screen. Use **Focus** for detailed work and **Arrange Windows** to restore the overview. No monitor enumeration, display IDs, DPI mapping or borderless mode is required.

Use **Shutdown System** for normal session exit; closing the launcher uses the same shutdown flow and may require confirmation for active executions. **Stop / Restart** operates on an individual component and does not perform the whole-session pause step. Handle active executions in the application before using component maintenance controls, especially Backend Restart, which applies the default Mock reset below. Existing compatible Backend processes may be reused; the launcher never terminates or restarts a Backend it did not launch. Logs and PID/role/start-time/executable records are in `Saved/Stage1`. A Windows job contains each owned process tree. Shutdown requests WM_CLOSE, waits eight seconds, then terminates only the retained process handle if necessary. Backend currently requires that bounded fallback; business mutations are persisted when acknowledged.

The launcher never clears persisted history. A fresh installation seeds four explicitly labelled Mock UAVs from `Backend/mock_execution.json`, without camera or flight connections. Existing aircraft and Home configuration files are preserved. Configuration deployment does not start execution.

The shipped configuration enables `mock_reset_on_start`. Whenever the launcher creates a new owned Backend for a Mock session, it waits for an acknowledged startup reset before opening clients. Eligible UAVs are arranged by numeric UAV identifier, eastward at 3 m intervals from the selected grass location within The Meadow at Gardens by the Bay: latitude 1.279445, longitude 103.862188, ellipsoid altitude 15 m. The location was chosen in the native map inside the lawn; the official The Meadow pin is approximately 55 m north, near a path, and was not used directly. The 15 m height is a near-ground display value for the currently loaded Google mesh, not automatic terrain snapping or a surveyed physical elevation. The formation is read from `startup_formation` in the source fixture `Backend/mock_execution.json`; it does not replace saved custom Home configuration. Attaching to an already running Backend or restarting only Command, Map or Video does not reset positions. Restarting the owned Backend creates a new session and performs the reset again.

Each reset saves a complete pre-reset document under `Saved/Stage1/data/mock-reset-history` and records a new position epoch. Completed history and route snapshots remain available. Eligible active Mock executions are marked ABORTED, scheduled Mock executions are CANCELLED, and their active reservations are released. Subsequent execution and return-home requests use the new session positions, including when a saved custom Home file still contains a previous location. Saved routes are not relocated; review their coordinates before starting an older plan in the new area. Received real telemetry, confirmed real execution positions and active real executions are protected; an active group containing a protected aircraft is skipped as a whole. Mock positioning does not mark hardware Online or issue flight commands.

The earlier 50 m waterfront checks remain historical evidence. The [grass startup acceptance](../Evidence/GrassStartup/20261023/acceptance.json) records 12/12 configuration/Backend checks and native grass inspection. The RHI startup test retains its failed result because Google tiles did not reach 99 percent loaded within the timeout; origin and spacing checks passed. See [startup details and evidence boundaries](../Docs/MarinaBayStartup.md).

Stage 1 uses local endpoints 19880/19881 and `Backend/build/Release/DroneBackend.exe` (existing `build-p53` installations are recognized as a fallback). Unified shutdown warns about active executions and, while its owned Backend is alive and reachable, requires Mock pause acknowledgement before stopping owned processes. It does not automatically pause a reused external Backend. To preserve acknowledged execution positions across owned Backend restarts, merge `"mock_reset_on_start": false` into `Launcher/config.local.json` and reopen the launcher before the next session. Starting Backend directly also does not invoke the launcher's reset. Executions paused by unified shutdown still require Resume. The normal launcher configuration applies the startup reset described above.

3D fleet movement requires Backend capabilities `fleet_move_3d` and `map_fleet_controls`, advertised by `GET /`. Readiness checks both newly started and reused Backends before starting any client. An older executable can have valid plan/context snapshots while still rejecting Map fleet movement with `ROLE_FORBIDDEN`; it is now rejected with a rebuild instruction instead. Rebuild `Backend/build/Release/DroneBackend.exe` through `SetupDroneSecurity.cmd` after source updates, stop the old running Backend, and relaunch. `Scripts/FleetBackendCompatibility/verify.py` tests the resolved normal Launcher configuration with isolated Mock data and ports; it does not substitute a phase-specific backend build.

Checks: `python Launcher/test_runtime.py`. `record_acceptance.py` is a read-only native acceptance oracle; it does not drive UI or simulate client recovery.

P5.3: Route editing exposes Closed Route (at least three waypoints). Move Plan is available in the Command plan workspace for idle plans, including deployed plans. In Map, drag the orange center handle, then Confirm Position or Cancel. Closed routes execute once in Stage 1 Mock mode. Running/paused/returning missions block moving.
