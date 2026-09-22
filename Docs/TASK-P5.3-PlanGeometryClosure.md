# TASK-P5.3 FINAL RESULT

Status: **CONDITIONAL PASS / not an unconditional Definition-of-Done PASS**. Closed-route and whole-plan translation functionality is delivered and the main native business flow completed. Two inherited Backend failures, retained Cesium/startup errors, and the unconfirmed physical single-waypoint axis-drag scenario prevent claiming that every requested regression/native check passed.

## Git and scope

- Branch: `feat/triple-screen-p5-3-plan-geometry`
- Base HEAD: `e9ce39402f3bc6222e9d7b2f3c93ef65b5870941`
- Implementation/test HEAD: `02464fcfdd6af16599cfa9cd637573548bb39972`; the final evidence/report commit HEAD is supplied in the task response and by `git rev-parse HEAD`.
- Worktree: `C:/Users/wy331/Documents/UE5DroneControl-p5-3`
- Existing P5.2 untracked `Docs/Stage1.zip`, `Docs/Stage1/`, and `Evidence/STAGE1-AUDIT/` were preserved.
- Main and prior stage branches are protected; no push, merge, tag, reset, stash, or global Git configuration change.

## Closed Route

The existing `FDronePathSaveData.bClosedLoop` / `ADronePathActor` topology is reused end to end. No duplicate first waypoint is stored. The Map checkbox persists through `save_route`; the existing spline renderer supplies the final-to-first segment. Adding, moving and deleting points refresh the same renderer. Fewer than three points cannot enable closure, and deleting below three opens the route. Backend rejects non-boolean topology and closed routes shorter than three points.

Mock execution traverses the closure exactly once, uses the last point's segment speed, accounts for closure distance and estimated duration, and finishes at the first point. `loopMode: Once` and `closure_completed` are execution fields. Logical waypoint counts remain unchanged. Existing Pause, Resume, Return and Abort behavior remains separate.

## Move Plan

Command exposes Move Plan in the selected plan workspace and requests Map interaction through the existing Backend HTTP/WebSocket channel. Map acquires a leased MOVE session and captures all routes belonging to the plan. Its orange preview shows waypoint numbers, direction arrows, closing segments, a plan boundary and a bounding-box-center handle. Dragging applies one world-coordinate offset to every original point. It does not rotate or scale.

Confirm sends all affected routes in one transaction. Cancel discards the preview; stored JSON is untouched. Errors retain the preview for retry/cancel. Draft/Ready plans return to Draft to invalidate previous validation. Deployed plans remain Deployed and receive a new deployment record; historical deployment and execution snapshots remain immutable. Starting an execution while a move lease exists is blocked. All active execution states, including pause/return/preflight, prevent moving the same plan.

## Coordinate conversion

`ICoordinateService::GeographicToWorld` hydrates original coordinates; dragging uses `NewAnchor - OldAnchor` in UE world coordinates; `WorldToGeographic` serializes the result. In CesiumWorld these calls are backed by `UCesiumCoordinateService` and the existing `ACesiumGeoreference` transforms. Conversion services, map projection, NED conversion and geographic parsers were not replaced. Backend checks a common finite world delta across every route with 0.1 cm tolerance, unchanged point count, closure and route parameters. WGS84 ranges are validated by the existing route validator.

The unchanged georeference/frame is part of the existing route contract. A mismatched saved world frame is rejected rather than silently rewriting geometry. The conversion itself is performed by the existing Map coordinate service, not reimplemented in Backend.

## Backend/API

Existing `/api/security-plans` action endpoint is reused: Command `request_map_plan_move`; Map `begin_plan_move`, `move_plan`, `cancel_plan_move`. The existing `MapRouteEditRequested` event has additive `mode: MOVE`. Existing `edit_sessions` leases, ownership, heartbeat, version conflict handling, atomic file persistence and `SecurityPlansChanged` publication are reused. Normal route-edit actions cannot reuse a MOVE lease.

`move_plan` receives a `paths` object keyed by existing route IDs. Each route supports `waypoints` and canonical `closedRoute`, with the existing `bClosedLoop` alias retained for client compatibility. Saving normalizes both booleans; conflicting values are rejected. It must include every saved route in the selected plan and no unrelated route. Mission assignments, speed/wait parameters, context/video target and historical snapshots are not translated.

## Localization and UI

All new labels use ProductText / NSLOCTEXT / FText and generated en/zh-Hans PO, archive and locres assets. No new product strings are hardcoded in widgets. Command tactical route presentation follows acknowledged Backend coordinates; Map execution presentation includes the closing segment and suppresses obsolete completed-deployment overlays after a move.

## Validation

Automated validation (retained, unfiltered evidence):

- Backend: 91 discovered; 89 passed, 2 historical failures (`SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`). All 14 P53Geometry tests passed. See `Evidence/TASK-P5.3/build/backend-alias-final.xml`.
- HTTP/WebSocket: 64/64 checks (P4 21, P5 8, P5.1 11, P5.2 14, P5.3 10), plus persisted-state restart check. Final suite: `Evidence/TASK-P5.3/run-1789918525892920100/protocol/`.
- Launcher: 7/7 passed (`build/launcher-tests.log`).
- UE: all eight selected integration tests have passing isolated-process evidence. Seven passed in `run-1789918525892920100/ue/`; geometry passed in fresh retry `geometry-retry-1789921760286816900/` (one discovered, one succeeded with warnings, zero failed, explicit Automation queue completion). Geometry checks actual spline segments for close/add/delete/move-first-point, save/restore, exact cancel, translation, all edges and Cesium WGS84 roundtrip within 0.1 cm.
- Retained failures: initial geometry run contained Cesium tile `ConnectionError` events; earlier map reload hit a Cesium GaussianSplat/Niagara access violation. Fresh Editor processes starting from Entry provided final evidence. Initial build and TArray-alias errors were repaired and their logs retained.
- Localization: en and zh-Hans catalogs/locres regenerated; GatherText commandlet reports result 0. The process reports 1 due to inherited GameFeatureData startup errors (`build/localization-delivery.log`), so this is not presented as a clean process-level pass.

Native interaction uses the three independently running native UE clients and the existing Launcher runtime, with real mouse/keyboard input. Read-only HTTP snapshots corroborate persisted state.

| Scenario | Physical UI evidence |
|---|---|
| A Open route | Created P5.3 Native Patrol, assigned UAV-01, placed four map waypoints, Save Draft retained editing, Finish exited (`native/A-open-four-points.png`). |
| B Closure | Toggled Closed Route, real final-to-first segment appeared; saved four logical points and restarted clients with closure restored (`B-*`). |
| C Closed editing | Added fifth waypoint and deleted it, with closure reconnecting (`C-*`). Axis-drag trials did not establish reliable physical single-point movement; those attempts were undone. The actual actor move-first-point/segment rebuild is covered by the passing UE test, not claimed as a physical-mouse pass. |
| D Open translation | Dragged all four points, confirmed and rehydrated; common world delta approximately (-865.945, 2164.856, 0) cm, variation below 0.000001 cm (`D-*`). |
| E Cancel | Long closed-route drag then Cancel restored exact route JSON and deployment snapshots (`E-cancel-verified.json`). |
| F Closed deployed translation | Chinese preview, long drag, Confirm. New common delta approximately (-1150.049, 2597.749, 0) cm; remains DEPLOYED, new snapshot created, old snapshots byte-equivalent as JSON objects (`F-confirm-verified.json`). |
| G Execution guard | Started Mock execution from moved deployment; move disabled with visible reason in Chinese and English. Paused state also blocks; Resume works (`G-*`). COMPLETED at the first waypoint after 142 seconds; `closure_completed=true`, `loopMode=Once`, 100%, four logical points (`G-completed-once.json`, `G-completed-once-command.png`). |
| H Independence/localization | Command/Map/Video online; language synchronization verified, preview/confirm/cancel/guard localized. Video target remains empty despite mission execution (`H-video-target-unchanged.png`). No video source/hardware connected. |

Native input investigation retained earlier failed drag attempts. Final center control uses a real UMG hit target and captures mouse events with their screen coordinates; plain clicking produces zero offset and long focused drags translate the preview. The test driver occasionally consumes its first interaction after window activation; only observed completed actions are marked passed. No HTTP mutation substitutes for the native flows above.

## Known issues and evidence boundaries

- The requested strict “Stage 1 all regressions passed” criterion is not met: the two named historical Backend tests still fail. They are not rewritten or hidden by P5.3.
- Physical single-waypoint axis dragging did not produce a verified displacement with the Windows driver. The attempts are retained (`native/C-single-waypoint-drag-unconfirmed.png`); added accidental points were undone. A copied draft was finished without changing its saved route. Actual UE actor movement and closing-segment rebuild passed automation. Manual axis-drag UX acceptance remains required.
- Native windows sometimes consume the first driver action after activation. Final plan-handle verification used a focused real control, then long captured drags; single clicking was verified to produce zero offset.
- Cesium/network and inherited GameFeatureData errors are retained separately from product assertions. Native basemap rendered during final tests; this is not a guarantee of external imagery availability.
- Route lengths in Command's existing geographic approximation and Map's UE-world measurement can differ slightly; both now include the closing segment. The rigid-translation acceptance uses world points/edges, not equality between those legacy distance approximations.
- No real aircraft, PX4/Jetson, physical three-screen setup, AI detection, or real video source was accepted. Stage 1 execution is explicitly Mock/Simulation.

## Changed Files

The complete implementation/configuration/test list (35 files) is in [TASK-P5.3-ChangedFiles.txt](TASK-P5.3-ChangedFiles.txt). This report and `Evidence/TASK-P5.3/` are the delivery additions. Changes reuse existing Backend storage, route actors, coordinate adapters, HTTP/WS and localization; no new transport or coordinate projection was introduced.

## Working tree and process handoff

Final status is checked after the report/evidence commit: `nothing to commit, working tree clean`. All native QA processes were stopped through their task-owned Launcher runtime; no unrelated PID was terminated. Native synthetic plan data remains under `Saved/P5.3-native`; the completed deployed plan and copied draft are retained. The normal launcher uses its own configured runtime directory.

## Deferred

Infinite/repeated patrol execution, rotation/scale, full Undo/Redo, runtime mission relocation, Stage 2/PX4/MAVLink and real-aircraft/video acceptance are outside this task. Cancel is supported inside Move mode.

## Local handoff

`DroneSecurityLauncher.cmd` starts this worktree with `Backend/build-p53/Release/DroneBackend.exe`, native Command/Map/Video clients and local endpoints 19880/19881. Deployment activates configuration; Start Mission launches explicitly labelled Stage 1 Mock execution.
