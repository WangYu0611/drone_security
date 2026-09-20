# TASK-P5 — Stage 1 Final Integration

## Final Result

**CONDITIONAL PASS — STAGE 1 COMPLETE** for **Single-Screen Multi-Window Layout**. The shared Backend and three independent Command / Map / Video programs run on one computer, launch from one entry, expose shared health, recover without clearing state and shut down through owned process handles. The final native demonstration completed.

Conditions are retained non-blocking planner/Cesium/legacy visual issues listed below. There is no pending physical-three-monitor acceptance requirement: the user explicitly removed monitor detection, physical left/center/right mapping, borderless fullscreen, Display ID/DPI mapping and physical triple-screen screenshots. [Scope amendment](../Evidence/TASK-P5/scope-amendment.md) overrides the preserved original request.

## Git and baseline

- Workspace: `C:\Users\wy331\Documents\UE5DroneControl-p5`.
- Branch: `feat/triple-screen-p5-stage1-integration`.
- Exact P4 base: `5237bb86cc992ef8f9c586492cb43c0cb888c4df`.
- Runtime recovery fix tested: `d73b027`; test/evidence tools: `aae1694`.
- Final HEAD: the closure commit containing this report, resolved by `git rev-parse feat/triple-screen-p5-stage1-integration`; exact immutable SHA is included in the final delivery response. A commit cannot embed its own hash.
- Baseline worktree was clean and all baseline builds/regressions preceded business edits. Protected main/P1/P2/P3/P4 plus other recorded branches remained unchanged. Original P4 worktree remains clean.
- Staged commits separate baseline, launcher, layout, presence, shell, recovery fix, test tools, evidence and documentation. No push, merge, reset or stash.
- Final clean status and diff checks are verified after the closure commit; verification record is under `Evidence/TASK-P5/closure`.

## Architecture and launcher

One UE project continues to use `-ClientRole=Command/Map/Video`. No duplicated project or second authoritative store was added. Existing Backend context, preferences, video view and security-plan endpoints remain authoritative. The legacy network manager now respects the same endpoint command-line overrides as the shared context subsystem.

Double-click `DroneSecurityLauncher.cmd`. The Tk launcher reads paths/endpoints/window mode from `Launcher/config.json`, launches Backend, validates actual snapshot schemas and then starts three independent processes. READY requires the expected instance UUID, live owned process, subscribed ONLINE session, completed domain hydration and fresh heartbeat. Process existence alone never means ONLINE.

The launcher keeps PID, role, creation time and executable records, retains process handles and contains each process tree in a Windows job. Shutdown sends WM_CLOSE and waits eight seconds before a bounded fallback against the owned process handle only. Backend uses this fallback and persists mutations when acknowledged. A reused existing compatible Backend is not owned and cannot be restarted or terminated by the launcher. A named mutex prevents duplicate launchers; live registered roles block duplicate role startup.

Stop/Restart is available per component; Shutdown System handles the whole owned session. Health polling leaves controls usable and queues clicks during a poll. Failure messages persist until the next user action. The initial ignored-click defect was fixed and revalidated by real Backend outage/recovery.

## Single-screen placement

Three normal resizable windows are arranged within the working area: Command left half, Map upper right, Video lower right, with an eight-pixel gap. This is a window layout, not a physical monitor mapping. Focus enlarges a role for detailed work; Arrange Windows restores the overview. No desktop/display settings are changed.

The observed work area was 2048×1104; recorded rectangles were Command `[0,0,1020,1104]`, Map `[1028,0,1020,548]`, Video `[1028,556,1020,548]`. Native process identities, window screenshots and launcher placement logs establish three concurrent programs. No composite was presented as a physical desktop screenshot.

## Presence, health and recovery

Existing registration, subscription and context heartbeat carry `last_seen` and `hydrated`. A session starts unhydrated, reports ready after context/plans/language/video hydration, and goes offline on disconnect or twelve-second heartbeat expiry. Existing event broadcasts update all clients. The common header distinguishes READY, DEGRADED and Backend offline/reconnecting.

Native checks:

| Scenario | Observed result |
|---|---|
| Video stop/restart | OFFLINE → DEGRADED → hydrated ONLINE/READY; video target and language unchanged |
| Map stop/restart | Command shows Map offline; edit request returns `Map client unavailable`; no false edit; restored READY |
| Command stop/restart | Map/Video stay running; plan, mission, deployment, UAV and language restore |
| Backend stop/restart | All three UE processes stay running; offline/reconnecting; automatically hydrate and return READY |
| Whole system shutdown/relaunch | All four domains equal pre-shutdown snapshots; no Saved data clearing or repair |
| Unified shutdown | All owned roots exit; separate baseline Backend remained reachable, proving it was not killed |

`Launcher/record_acceptance.py` only reads APIs and ownership records; it does not simulate native clients or drive recovery. Exact domain comparisons and PID evidence are in `Evidence/TASK-P5/recovery`. The separate task-owned regression Backend was later stopped only after verifying its executable and creation timestamp.

## Visual shell and localization

Command, Map and Video share the same 72-unit header component, product/role identity, health row and locale/time row, using the existing theme and FText/ProductText catalog. Existing role-specific workflows remain below the header. New launcher strings use the same en/zh-Hans catalog and follow acknowledged Backend language.

Native Command → Chinese and Video → English changes converged across all three clients. Six final demonstration captures are `demo-08-command-zh`, `demo-09-map-zh`, `demo-10-video-zh`, `demo-11-video-en`, `demo-12-map-en`, `demo-13-command-en-logs`. Localization resources were regenerated through the existing workflow. The P4 browser-isolation automation passed; language changes do not change video target or business content.

At tiled sizes Map/Video text is dense and some Command fields truncate. Focus/maximize makes detailed operations usable; this is recorded rather than claimed as a perfectly responsive small-window design. Legacy UAV alias/offline captions and date formatting remain retained visual conditions.

## Builds and automated validation

| Validation | Result |
|---|---|
| Backend Release configure/build | PASS |
| Backend suite | 60 discovered, 58 PASS, 2 retained historical failures |
| UE5.8 Development Editor build | PASS |
| Launcher readiness/role args/ownership/shutdown/single-instance/layout | 5/5 PASS (ownership and shutdown combined) |
| P4 real HTTP/WS workflow | 21/21 PASS with synthetic protocol clients |
| P5 presence/expiry/localization/protocol replica recovery | 8/8 PASS |
| UE P4 Command workflow/review | 2/2 Success |
| UE Map draft guard and FText isolation | 2/2 Success |
| UE explicit Video Target/browser isolation | 1/1 Success |

UE total: discovered 5, Test Completed Success 5, failures 0, not run 0. All five carry retained engine warnings; the report's `succeededWithWarnings` is counted explicitly, not hidden. Initial runner miscount is preserved. These are real UE PIE automation results against a real isolated Backend, separate from native UI acceptance.

Retained Backend failures: `SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`. No new Backend failures. No expectations were edited to make tests pass. Full reports, command logs and failed trials are indexed in [Evidence README](../Evidence/TASK-P5/README.md).

## Native A–P and continuous demonstration

The complete amended A–P matrix is [native/acceptance-ledger.md](../Evidence/TASK-P5/native/acceptance-ledger.md). Physical Native B is removed; normal single-screen placement is accepted instead. All remaining native gates passed.

Initial native workflow created plan-1, mission-2 and deployment-5 with two route points. The final continuous sequence started at System OFF, launched all roles, additionally checked final-version Video recovery, opened the persisted copied draft plan-6/mission-7, requested Map editing, added a third waypoint and saved revision 2. Command checked configuration, reviewed and deployed `deployment-10`. It selected operational UAV-02, then explicitly opened task UAV-01 video. It switched Chinese from Command and English from Video, inspected operation logs/alerts and shut down the whole system.

Final snapshot: active UAV-02, video target UAV-01, language en, plan-6 DEPLOYED, mission-7 assigned UAV-01, route-8 with three waypoints. Both deployment snapshots remain persisted. Deploy means configuration only; the UI states execution has not started. No array/execution/flight endpoint was used for this workflow.

The demo used native UI throughout: no UE console, separate manual UE launch, window dragging, Backend repair or state-file edit. Original captures `demo-00` through `demo-15`, authoritative snapshots, operation logs and owned shutdown records support the result. An early unacknowledged selection click is explicitly distinguished from the later acknowledged UAV-02 change in the ledger.

## Performance observation

During three-client operation, working sets were approximately Command 2.63 GiB, Map 2.93 GiB, Video 3.55 GiB, Backend 8 MiB. The two-second CPU sample totaled roughly 20% of 16 logical processors; this is a brief observation, not a benchmark. GPU: RTX 3060 Laptop, 5672/6144 MiB used, 44% utilization. FPS was capped at 20 through the launcher; achieved FPS was not instrumented. Native interaction remained usable; no OOM, GPU crash or repeated UE crash observed.

## Known, retained and deferred issues

- Retained: the two historical planner test failures; Cesium imagery/network dependence; SQLite cache concurrency risk; 1082 vs 1080; low-altitude route occlusion; GameFeatureData/plugin diagnostics; 6 GB VRAM capacity risk.
- Observed visual conditions: Command's inherited tactical imagery stayed blank while Map imagery rendered; legacy synthetic registry aliases and occasional bilingual offline captions; dense tile-size controls. Map editing and the continuous configuration workflow completed using the native Map window and Focus/maximize.
- Diagnostic startup plugin prompts and offline polling responsiveness were fixed for the final launcher, not left as conditions.
- Synthetic UAVs and NO SOURCE Video provide no evidence of real telemetry, camera, flight or AI behavior.
- Stage 2 only: second-PC Video migration, LAN multi-host, production RTSP/WebRTC, real camera/Jetson/AI, real UAV/PX4/MAVLink and execution safety.

These conditions do not block the completed single-computer three-program Stage 1 scope. They prevent claiming production flight/video readiness or a flawless legacy visual layer.

## Evidence and completion

Evidence is divided into Selected Final Result, Diagnostic and Historical Failing Trial. Logs are copied with credential redaction; originals remain in Saved and copy/source hashes are indexed. Screenshots are unmodified native captures. See [Evidence README](../Evidence/TASK-P5/README.md), [diagnostics](../Evidence/TASK-P5/diagnostics.md), [Launcher instructions](../Launcher/README.md) and [protected refs](../Evidence/TASK-P5/closure/protected-refs.txt).

**STAGE 1 COMPLETE: one computer, Backend + independent Command / Map / Video, unified launch, single-screen multi-window layout, visible health, acknowledged workflow/localization, recovery and safe unified shutdown.**
