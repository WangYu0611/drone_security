# Stage 1 single-screen launcher

For a new workstation follow [First run on Windows](../Docs/First-Run-Windows.md). Install UE 5.8, Cesium for Unreal, VS C++ tools and Python 3.10+ with Tk, then run `SetupDroneSecurity.cmd`. Use `CheckDroneSecurity.cmd` for actionable diagnostics and `DroneSecurityLauncher.cmd` to launch. UE is discovered through the registry/Epic manifest; optional local overrides belong in ignored `Launcher/config.local.json`.

The launcher starts Backend first and validates the existing context, preferences, video and plan APIs before starting three independent instances of the same UE project. READY requires the expected instance for every role, a live owned process, a subscribed session and a fresh hydrated heartbeat.

Command occupies the left half of the working area. Map and Video share the right half. These are normal resizable windows on one screen. Use **Focus** for detailed work and **Arrange Windows** to restore the overview. No monitor enumeration, display IDs, DPI mapping or borderless mode is required.

Use **Stop / Restart** for individual components and **Shutdown System** for the whole session. Closing the launcher also shuts down its owned session. Existing compatible Backend processes may be reused; the launcher never terminates or restarts a Backend it did not launch. Logs and PID/role/start-time/executable records are in `Saved/Stage1`. A Windows job contains each owned process tree. Shutdown requests WM_CLOSE, waits eight seconds, then terminates only the retained process handle if necessary. Backend currently requires that bounded fallback; business mutations are persisted when acknowledged.

The launcher never clears persisted data. A fresh P5.2 installation seeds four explicitly labelled Mock UAVs and independent Home positions from `Backend/mock_execution.json`, without camera/flight connections. Existing data is never overwritten. Configuration deployment does not start execution.

P5.3 uses local endpoints 19880/19881 and `Backend/build/Release/DroneBackend.exe` (existing `build-p53` installations are recognized as a fallback). Unified shutdown warns about active executions and requires durable pause acknowledgement before stopping owned processes. Individual Backend restart resumes persisted simulation state; unified shutdown restores PAUSED and requires Resume.

Checks: `python Launcher/test_runtime.py`. `record_acceptance.py` is a read-only native acceptance oracle; it does not drive UI or simulate client recovery.

P5.3: Route editing exposes Closed Route (at least three waypoints). Move Plan is available in the Command plan workspace for idle plans, including deployed plans. In Map, drag the orange center handle, then Confirm Position or Cancel. Closed routes execute once in Stage 1 Mock mode. Running/paused/returning missions block moving.
