# TASK-P5.2 — Security Mission Execution Closure

## Final Result

**CONDITIONAL PASS — Stage 1 software / Mock execution scope.** All A–U native gates are satisfied, including visible Deploy/Start, Backend-driven movement, controls, recovery, immutable deployment and video/language isolation. The final repaired native execution E22 completed four waypoints in 149.83 seconds at 100%. Conditions are the two allowed historical planner failures, inherited Cesium/third-party diagnostics and short performance-observation limits. Failed development/environment trials are retained separately. No unresolved execution hard gate is being classified as conditional.

- Branch: `feat/triple-screen-p5-2-mission-execution`
- Base HEAD: `5ad04ce4070758025db047741ebec5280d67b09c` (`feat/triple-screen-p5-1-command-plan-ux`).
- Final HEAD: the local closure commit containing this report; resolve with `git rev-parse HEAD` in this worktree. Its exact SHA is recorded in the delivery response, avoiding a self-referential commit hash inside the commit.
- Worktree: `C:\Users\wy331\Documents\UE5DroneControl-p5-2`.

## Architecture

`SecurityPlanStore` remains the single Backend authority. Execution is a separate collection in its existing atomic persistent document, sharing its mutex and event log. Plan state remains DRAFT/READY/DEPLOYED; execution progress does not change plan revision or invalidate configuration edits. Each execution owns an immutable copy of the deployed route, deployment ID, route revision, mission/UAV identity, position, progress, timestamps and completion/failure reason.

Command submits explicit requests and renders acknowledged snapshots. Map renders geographic positions from those snapshots through the existing coordinate service and Registry; its UAV marker does not integrate motion locally. For Mock execution the receiver follows Backend samples with physics integration disabled. The separate legacy local-preview shadow is hidden and its independent actor tick/projection trail disabled; its logical position is synchronized to the same sample. This avoids duplicate preview movement and black curtain geometry while retaining the visible receiver and execution marker. The existing flight execution engine, PX4/MAVLink paths and video transport are not invoked. UI selection, mission assignment, video target and execution are separate. Only explicit View Video actions change video target.

Execution updates have their own monotonic version. Client snapshot merging preserves newer plan and execution versions independently. Control versions protect against stale state changes; durable request IDs make retries idempotent. Another active execution on the same UAV is rejected, including CREATED/PREFLIGHT/STARTING/PAUSED/RETURNING states.

## Execution State Machine

`CREATED → PREFLIGHT → STARTING → EXECUTING → COMPLETED`.

- EXECUTING → PAUSED freezes position and progress; Resume continues the same execution and segment.
- EXECUTING → RETURNING travels directly to the explicitly configured simulation Home, then COMPLETED with `RETURNED_HOME`; remaining route points are skipped.
- EXECUTING/PAUSED → ABORTED freezes the current position and cannot resume.
- Missing simulation UAV/invalid runtime data → FAILED with an explicit reason and shared event/alert.
- Unified shutdown durably pauses active executions, preserving the resume state, including RETURNING.

## Mock Execution Model

`Backend/mock_execution.json` defines four explicitly labelled Mock UAVs, independent Home coordinates, and default speed 8 m/s. Fresh Launcher data is seeded only when absent; existing business data is never replaced. Each segment uses its saved positive speed, falling back to the centralized default. The model interpolates latitude/longitude/altitude, honors waypoint waits, advances waypoint index and distance progress, and stops at the final point. The Backend timer targets 10 Hz; elapsed integration is capped at one second per tick to avoid large scheduling jumps. After Backend restart, execution resumes from the last persisted sample without integrating downtime.

The native four-point route contains an initial default-speed leg and saved 1 m/s legs. Home is not WP1. All results here are Stage 1 simulation, not vehicle/flight acceptance.

## Deployment UX and Start Mission UX

Map Save retains the edit session. Finish closes editing and sends Command to Pre-deployment check. A full-width Deploy Plan action precedes review details. Deployment ACK exposes Start Mission; it never starts movement automatically. Start confirmation displays plan, task, UAV, waypoint count, estimated distance/time, Mock availability, immutable deployment, conflict and Backend/Map readiness. Start, Return and Abort have explicit confirmations. Pending actions show Backend acknowledgement status.

The execution workspace displays simulation identity, state, current waypoint, progress, duration and valid controls. Overview links to the active mission, Fleet shows execution state, and deployed plans expose recent execution records. Map shows the immutable route, completed/current/pending waypoint markers and current UAV position in an automatically framed view. The route editor remains read-only for deployed plans.

## Backend Tests

`Evidence/TASK-P5.2/backend/results.xml`: 77 discovered, 75 passed, two historical planner failures (`SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`). All 13 P5.2 tests pass: transitions, pause/resume, completion, abort, independent Home, duplicate/conflicting start, immutable snapshot, missing Mock failure, stale controls, separate versions, shutdown resume state, durable restart and atomic publisher failure.

## Protocol Tests

Isolated synthetic Backend at 19680/19681, never counted as native UI acceptance. P4 21/21, P5 8/8, P5.1 11/11, P5.2 14/14; paused execution and deployment snapshots survive Backend restart exactly. Results and traffic are under `Evidence/TASK-P5.2/protocol`. `Scripts/P5.2/run_backend_regression.py` creates fresh runtime data and owns its processes; it does not operate the native system.

## Launcher Tests

Seven tests cover readiness, process ownership/safe shutdown, role arguments, single instance, layout, active execution warning and persistence acknowledgement failure. The P5.2 launcher uses 19780/19781, separate from the pre-existing P5.1 launcher. It warns on active unified shutdown, allows cancellation, and requires pause persistence acknowledgement before stopping owned processes. Individual Backend restart resumes the persisted simulation; unified relaunch retains PAUSED until explicit Resume.

## UE Tests

Command 3/3, Map 3/3, Video 1/1, with fresh exported reports and explicit `Test Completed. Result={Success}` counts. All seven tests are `SucceededWithWarnings`, zero failed in the final successful exports. Map's three tests were repeated after the final simulation presentation change, each in its own Editor process (`ue/Map-isolated`) to avoid the Cesium repeated-PIE-world failure documented below. The final monitor test also verifies a hidden, non-ticking local preview, a visible receiver, and matching logical positions. `ue/final-counts.json` names the exact accepted reports; failed combined/network attempts remain separate evidence. The Command copy test forces garbage collection before revealing the copied task, covering the native-discovered ComboBox lifetime defect. New tests use actual execution controls and the Map monitor. Automation remains synthetic, distinct from the native ledger.

## Native Acceptance

The initial launcher was opened by double-clicking `DroneSecurityLauncher.cmd` in Explorer. All user actions use real Command/Map/Video/Launcher windows. HTTP reads are evidence oracles only; no script/API performs native business mutations. Original bounded window captures are saved without compositing or redrawing in `Evidence/TASK-P5.2/native`.

The native run found and repaired: stale not-started copy, off-window map projection caused by double-applying desktop origin, unwanted read-only editor restoration, generated ComboBox text lifetime across GC, execution workspace width, and duplicate legacy shadow-preview curtains. The last defect was traced to `AMultiDroneCharacter::Tick` following the receiver while owning another ground-projection component; Mock execution now suppresses that preview rather than running two visual motion systems. The pre-fix Command crash log and projection screenshot are retained; no pre-fix failing trial is represented as final PASS.

| Gate | Result | Native evidence / observation |
|---|---|---|
| A Startup | PASS | Explorer double-click; four independent READY roles; `startup-clients.json` |
| B New plan | PASS | `P5.2 Mission Test`, automatic Task 01; persisted plan-1 / mission-2 |
| C Assignment | PASS | UAV-01 selected in Command; Map edit opened |
| D Save / Finish | PASS | Four geographical waypoints; Save retained editor (`01-map-route-saved.jpg`); Finish closed it |
| E Visible Deploy | PASS | Finish automatically opened pre-deployment check; `01-predeploy-check.jpg`, `02-deploy-button.jpg` |
| F Deploy / Start CTA | PASS | Deployment acknowledged without starting; `03-deployed-start-button.jpg` |
| G Explicit Start | PASS | Confirmation then Backend execution; `04-start-confirm.jpg`, `05-executing-command.jpg` |
| H Motion | PASS | Backend position and visible Map UAV marker moved along all four points; `06-executing-map.jpg` |
| I Progress | PASS | Command and Map share waypoint/progress; `07-waypoint-progress.jpg`, raw observations |
| J Pause / Resume | PASS | E14 paused for 57 observed samples, position/progress unchanged; `08-paused.jpg`, `09-resumed.jpg` |
| K Command restart | PASS | E6 continued while Command restarted; same execution recovered; `13-command-recovery.jpg` |
| L Map restart | PASS | E7 continued while Map restarted; monitor restored; `14-map-recovery.jpg` |
| M Language | PASS | Command Chinese then Video English during E7; progress continued; `15-chinese.jpg`, `16-english.jpg` |
| N Return | PASS | E7 RETURNING then COMPLETED / RETURNED_HOME; independent fixture Home, not WP1; `10-returning.jpg`, `returned-home.json` |
| O Abort | PASS | E14 ABORTED at 86%; subsequent samples stationary; `11-aborted.jpg` |
| P Uninterrupted complete | PASS | E18 and final-render E22: created → started → WP1 → WP2 → WP3 → WP4 → completed, no control/restart intervention; 100%; `12-completed.jpg`, `uninterrupted-execution-18.json`, `uninterrupted-execution-22.json` |
| Q Same-UAV conflict | PASS | Another deployed plan showed conflict and disabled Start; no second active execution; `active-execution-conflict.jpg`, `conflict-state.json`; Backend rejection separately covered by protocol tests |
| R New draft isolation | PASS | Copied plan-15 / mission-16 / route-17 while E14 continued deployment-13 route unchanged; `new-draft-isolation.json` |
| S Backend recovery | PASS | E7 resumed same execution from persisted sample; `backend-restart-before.json`, `backend-restart-after.json` |
| T Unified shutdown | PASS | Active E6 warning, cancel retained system, confirm paused and closed all owned processes; `17-shutdown-confirm.jpg`, Launcher log |
| U Relaunch | PASS | Confirmed plan/deployment and Completed/Aborted history retained; `relaunch-history.jpg`, final persistent snapshot |

Gates were exercised across several executions and independent restarts, not represented as one uninterrupted session. E18 and E22 are dedicated uninterrupted completion trials. Earlier recovery/control captures precede the final width/render refinements; their timestamps and raw records are retained. Final movement/progress/completion captures use the repaired presentation. The user's final subjective UX review remains required before freezing Stage 1.

## Recovery and Localization

Command and Map are independent processes. Their restart does not stop Backend simulation. All confirmed plan/deployment/execution data is persisted. Backend restart resumes from its durable record; no new execution is created. A unified shutdown pauses first and restores the same execution on relaunch.

All new execution text uses FText and the existing en/zh-Hans localization pipeline. Language changes originate from Command or Video through existing shared preferences; they do not mutate route, execution or video target. Localization generation and GatherText logs are retained. Existing third-party diagnostics are recorded separately from successful commandlet completion.

## Execution Evidence and Performance Observation

Native observation rows contain timestamps, states, geographic position, progress and current waypoint. Backend events retain execution snapshots for created/started/waypoints/completed and control transitions. `native/validation.json` records the final counts and validates immutable execution routes, one active execution per UAV, stable pause/abort samples and independent Home return. E18 has 150 distinct observed moving positions; its complete ten-event timeline is `native/uninterrupted-execution-18.json`. These read-only checks substantiate actual UI trials; they are not substitutes for native interaction. Recheck archived evidence with `python Scripts/P5.2/validate_native.py --offline`.

`Scripts/P5.2/observe_performance.ps1` samples all four owned processes over five 2-second intervals. CPU is expressed as percentage of one logical core, not whole-machine percentage. Working/private memory are MiB. `gpu.txt` is device-wide utilization/VRAM, shared with other desktop applications; WDDM prevents reliable per-process GPU attribution. These observations are not a performance certification.

| Process | CPU, one-core % mean | Working set MiB | Private MiB |
|---|---:|---:|---:|
| Backend | 105.8 | 8 | 2 |
| Command | 26.0 | 2687 | 2865 |
| Map | 70.3 | 3335 | 4904 |
| Video | 87.5 | 3685 | 5880 |

RTX 3060 Laptop GPU device-wide sample: 47% utilization, 5616/6144 MiB VRAM. Backend CPU above 100% means more than one logical core; this is an observation, not a claim of low overhead. Clients are configured with a 20 FPS cap, not a guaranteed measured frame rate. During E18, the execution version advanced 92 times in 10.023 seconds: observed 9.18 Hz versus the 10 Hz timer target (`native/update-rate.json`).

## Known Issues

- Two historical planner failures remain unchanged; they are not P5.2 execution failures.
- Existing Cesium/third-party plugin diagnostics and imagery limitations remain bounded as inherited conditions. Two late combined Map automation attempts exited in `UCesiumGaussianSplatSubsystem::Tick` with `UObjectArray Index >= 0` when entering the next PIE world. Failed logs are retained, including `ue/map-cesium-startup-failure.log`, and are not counted as passing runs. The final Map rerun isolates each unchanged test in a fresh Editor process. No coordinate was shifted to hide terrain geometry.
- A separate `MapLiveDraftGuard` attempt was marked Failed by UE because of nine Cesium tile/raster connection errors; its export and log remain in `ue/map-network-failure`. Those errors are not converted to successful test events. Final accepted exports are identified separately by `ue/final-counts.json`.
- Existing geographic coordinate entry controls contain older fixed Chinese labels. New execution UI uses synchronized localization.
- Performance observations are short desktop samples, not production capacity or field-flight evidence.
- Local C-drive pressure caused UE Zen cache `507 Insufficient Storage` diagnostics; the tests still completed successfully. This task's generated `Intermediate` and `Binaries` directories were preserved on `D:\CodexBuild\UE5DroneControl-p5-2` with local junctions. No shared cache, user project content or persisted mission data was removed. A portable checkout still needs the documented local build prerequisites.
- An attempted overlap of an extra automation Editor with the three native clients exhausted local commit/pagefile capacity (`1455` from process creation and `E_OUTOFMEMORY` from screenshot capture); a Command startup also reported D3D12 `E_INVALIDARG`. Interrupted logs are retained. Final automation and native revalidation are run sequentially. This failed extra-process stress attempt is not normal three-client acceptance and is not counted as PASS.

## Deferred Stage 2

PX4, MAVLink, real UAV/camera, RTSP/WebRTC, Jetson, AI, LAN and multi-host execution are explicitly deferred. Deployment means business configuration; Mock preflight does not certify terrain, airspace, endurance or real flight safety. Stage 1 is not formally frozen until the user manually reviews the complete launcher experience.

## Git Protection

Baseline protected ref evidence is in `Evidence/TASK-P5.2/baseline/refs.txt`; final comparison is `closure/protected-refs.json`. Every other local branch retains its original SHA, and the P5.1 worktree is clean. The original main worktree's pre-existing modified `logs/backend.log` and untracked `Backend/data/operational_context.json` are untouched. Native owned-process shutdown is verified in `closure/native-shutdown.json`; synthetic regression processes also exited. Delivery consists of a local closure commit followed by a clean-worktree check, with its exact final SHA in the delivery response. No push, merge, tag or Stage 2 work is authorized or performed.

Run `DroneSecurityLauncher.cmd` from this worktree for manual review. Persistent native acceptance records remain in `Saved/Stage1/data`; they are not deleted or silently reset by the launcher.
