# Stage 1 single-screen launcher

Double-click `DroneSecurityLauncher.cmd` in the repository root. Python 3.13 with Tk, the UE5.8 Development Editor build and the Backend Release build must be installed/built. Configure their paths and the HTTP/WS endpoints in `config.json` before use on another workstation.

The launcher starts Backend first and validates the existing context, preferences, video and plan APIs before starting three independent instances of the same UE project. READY requires the expected instance for every role, a live owned process, a subscribed session and a fresh hydrated heartbeat.

Command occupies the left half of the working area. Map and Video share the right half. These are normal resizable windows on one screen. Use **Focus** for detailed work and **Arrange Windows** to restore the overview. No monitor enumeration, display IDs, DPI mapping or borderless mode is required.

Use **Stop / Restart** for individual components and **Shutdown System** for the whole session. Closing the launcher also shuts down its owned session. Existing compatible Backend processes may be reused; the launcher never terminates or restarts a Backend it did not launch. Logs and PID/role/start-time/executable records are in `Saved/Stage1`. A Windows job contains each owned process tree. Shutdown requests WM_CLOSE, waits eight seconds, then terminates only the retained process handle if necessary. Backend currently requires that bounded fallback; business mutations are persisted when acknowledged.

The launcher never clears persisted data. A fresh P5.2 installation seeds four explicitly labelled Mock UAVs and independent Home positions from `Backend/mock_execution.json`, without camera/flight connections. Existing data is never overwritten. Configuration deployment does not start execution.

P5.2 uses local endpoints 19780/19781. Unified shutdown warns about active executions and requires durable pause acknowledgement before stopping owned processes. Individual Backend restart resumes persisted simulation state; unified shutdown restores PAUSED and requires Resume.

Checks: `python Launcher/test_runtime.py`. `record_acceptance.py` is a read-only native acceptance oracle; it does not drive UI or simulate client recovery.
