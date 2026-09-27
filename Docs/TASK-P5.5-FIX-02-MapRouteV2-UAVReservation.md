# P5.5 FIX-02 — MAP Route V2 and active UAV reservation

## Baseline and evidence boundary

Repository `https://github.com/WangYu0611/drone_security.git`, `D:/CodexPublish/drone_security`, branch `main`. Clean preflight after fetch: HEAD = local main = origin/main = `efa16debd5ea9e1fe0ee919663973a9f5f0be89c`. No branch, worktree, reset, or historical code replacement.

Native observations use real Command and Map clients against a task-owned Mock Backend (20580/20581), production widgets and CoordinateService, and existing offline ground/georeference map `/Game/Tests/P55OfflineMap`. Persisted three-Mission route fixture was created through Backend APIs; no QA-generated route actors. This does not validate satellite/terrain delivery or real flight. API fixture setup and supplemental requests are identified separately from mouse-driven operations.

Baseline native Browse already displayed three colored Route V2 paths (`native/00-baseline-browse.png`). The reported pure legacy orange MAP route was **not reproduced on this baseline/fixture**. Do not claim an identified root cause for that report. Independent Command overview tactical map has Slate orange execution and cyan saved polylines; that widget is not instantiated by the independent Map shell.

## Ownership audit

| State/source | Creates and owns | Geometry | Material | Removal |
|---|---|---|---|---|
| Saved browse / READY / DEPLOYED | MapShellWidget -> SavedRouteVisuals (FPlanRouteVisualSet) | accepted persisted Mission route via shared CoordinateService decoder | ADronePathActor -> DronePathVisual -> M_CommandPathV2 MIDs on core/halo/joints | Retain/reset on selection, state changes, widget destruction |
| Editing | DroneOpsPlayerController editable Mission ADronePathActor | waypoint editing and existing save contract | same V2 components | cached read-only actor hidden; saved presenter excluded while editor owns Mission |
| Plan Move | MapPlanMoveWidget -> PreviewVisuals | original resolved geometry + presentation transform | same V2 components, camera-scaled width retained | preview destroyed on cancel/confirm; saved presenters restored |
| EXECUTING / COMPLETED | MapExecutionWidget -> FPlanRouteVisualSet | immutable acknowledged execution route_snapshot | same V2 components | retain latest execution group per Mission; remove superseded history and saved duplicates |
| Slate in MapPlanMoveWidget | move bounds, center cross, handle and caption only | screen overlay, no route body | UI brush | move lifecycle |
| Slate in MapExecutionWidget | waypoint markers/captions and UAV diamond only | snapshot projections, no route body | UI brush | execution lifecycle |
| CommandTacticalMap | Command overview only | separate schematic overview polylines | Slate | outside independent MAP surface |

Searched NativePaint, DrawLine(s), Polyline, Spline, RouteActor, PathActor, MissionRoute, PlanRoute, ExecutionRoute, DrawDebug, Slate, MapRoute and MapMission. No extra Security Plan shader was introduced. Disabled spline debug drawing and removed silent fallback to M_CommandPath: missing V2 material now emits an explicit diagnostic and hides the affected mesh rather than showing a legacy/default material.

Execution ownership chooses the latest group per Mission, preserves completed snapshots alongside other running Missions, excludes stale deployment history, and treats empty legacy group IDs individually. Multi-UAV executions retain one presentation set per Mission with each acknowledged per-UAV trajectory; they are not collapsed into a fabricated central route. Existing separate-Mission concurrent execution support is preserved.

## Reservation contract

Derived under the existing SecurityPlanStore mutex from durable execution records. Locking states: CREATED (accepted Start), PREFLIGHT, STARTING, START_REQUESTED, WAITING_ACK, EXECUTING, PAUSED and RETURNING. READY and DEPLOYED are plan states and do not reserve. SCHEDULED is not an assignment reservation; existing execution scheduler exclusivity remains unchanged. COMPLETED/CANCELLED/ABORTED/FAILED release immediately.

Cross-plan checks run at assign, save_route, validate, review, deploy and execution_start. Existing same-plan mission validation is unchanged. Existing drafts/copies may retain old selections; these cannot be saved/validated/deployed/started while conflicting. The HTTP response uses existing 409 envelope: code DRONE_ACTIVE_PLAN_CONFLICT, params.drone_id, params.active_plan_id, params.active_plan_name, params.active_execution_id, and current state for refresh. No telemetry or real-aircraft protocol change.

UI reads the accepted execution replica on every refresh, keeps unavailable UAVs visible and disabled with plan name, and refreshes release without restarting clients. Clearing an unavailable draft selection is explicit and requires Save to change Backend assignment. Bilingual conflict text uses FText/localization resources.

## Verification (updated at closure)

Backend reservation tests: 9/9; complete suite 129/131. Historical failures retained: SegmentDistanceTest.Crossing and AssemblyPlannerTest.ConflictHeightSeparation, documented in TASK-A and Stage1 audit. Their algorithms/tests were not modified.

Retained failed attempts: first Backend link while task-owned host still used the executable; JSON/std::string test comparison compile error; old integration fixtures attempted saving UAV-01 while a separate paused execution reserved it; UAV-04 fixture initially flew through static UAV-01 position and was rejected by unchanged separation checks. Fixture execution is now spatially aligned with UAV-04 home. No safety threshold or reservation check was weakened.

The independent-group replica test previously cloned the same Mission ID; it now supplies a distinct Mission ID to express its actual independent-Mission intent. A new ownership test separately asserts that same-Mission historical owners are suppressed.

Native screenshots and API evidence: `Evidence/TASK-P5.5-FIX-02/native`. Build, suite results and retained failures: `Evidence/TASK-P5.5-FIX-02`.

## Final automated results

- Backend Release build: PASS (`backend-build-2.log`); reservation 9/9 PASS; full suite 129 passed / 2 historical failures / 131 discovered and completed.
- UE5.8 Development Editor build: PASS (`ue-build-layout-final-2.log`). Localization GatherText completed; compiled Chinese and English resources checked in.
- Final combined run (`final-run-2.log`, `final-verification.json`): 12 discovered and completed, 12 passed, 0 failed. Five unit/camera assertions passed without warnings; seven real-widget offline tests passed with retained engine/config/environment warnings. Reports: `offline-1790489866518812800`, `offline-1790489888476147900`, `ue-1790489906879491900`.
- Route tests inspect actual visible actors and core/halo material instance parents across persisted browse, edit, READY, DEPLOYED, execution, completion and move lifecycle. Ownership tests cover old empty group IDs, later execution replacement, completed plus other active Missions, and stale deployment snapshots. Speed-flow/altitude encoding and closed topology assertions remain enabled.
- Reservation tests use real public Store transactions, including two concurrent threads with exactly one accepted cross-plan start, immutable rejection, reload from disk, paused/pending states, terminal release and draft validation paths. Native HTTP evidence additionally establishes 409 status and semantic owner IDs.
- P5.4 lock, draft isolation and damping automation passes. Native edit/save lock behavior was observed. Sustained native right/middle-button 3D camera gestures are **NOT NATIVE VERIFIED**; no camera/coordinate/telemetry logic was edited.

## Native journey and evidence index

All images below are actual native window captures under `native/`, not generated mockups. Starting data came from Backend fixture APIs. Mouse operations used the production widgets. API-only steps are explicitly separated.

| Evidence | Observed action/result |
|---|---|
| 00-baseline-browse | Baseline already shows three Route V2 colored routes; original plain orange MAP defect not reproduced |
| 01-edit-altitude-speed, 02-save-camera-locked, 03-finish-closed-loop | Native waypoint edit (altitude 70 to 90, speed 6 to 12), Save and Finish; retained V2 colors/closed topology and edit lock |
| 04-move-preview, 05-move-cancel, 06-move-confirm | Native nonzero Plan Move drag, cancel restoration, then confirm persisted translation |
| 07-existing-draft-before-start | Plan B retained UAV-01 selection before Plan A native Start |
| 08-deployed-command, 09-deployed-map | Native deployment and V2 Map presentation |
| 10-executing-command, 11-executing-map | Native Start, running Mock mission and immutable V2 route |
| 12-completed-closed-map | Completed closed loop stays V2; completion state includes closure_completed |
| 13-paused-disabled-en, 14-draft-rejected-en | Paused Plan A blocks B; native stale-selection Save rejected with English UAV and plan name |
| 15-executing-disabled-conflict-zh | Chinese disabled owner and conflict text; supplemental API Resume kept Plan B visible; execution state recorded alongside image |
| 16-completion-release-save-zh | After completion, native Save of B's UAV-01 succeeds without client restart |
| 17-clear-and-reassign-uav02 | During another paused execution, native explicit Clear unavailable, select UAV-02 and Save succeeds |
| 18-reconnect-reservation | Task-owned Backend restarted; Command hydrates same paused reservation; API conflict retains execution-24 |
| 19-aborted-command | Native Abort and confirmation end execution-24; identical API assignment changes from 409 before abort to 200 after abort |

`api-*.json` files are supplemental API responses, not native-action evidence. API fixture start/pause was used to keep a stable UI observation window for clear/reassign and restart. The first attempt without an online Map was correctly rejected (`MAP_CLIENT_UNAVAILABLE`) and retained. Native execution-20 and execution-22 started through Command; their completed closed-loop states are recorded in `completed-state.json`.

Native inspection found a dynamic-label wrapping problem after execution began. The checkbox caption previously reused the narrow width of its short UAV ID. Assignment captions now use fill alignment and disable automatic wrapping; this is a local presentation change. Earlier successful state tests did not establish layout correctness, so native layout was separately rechecked after rebuilding.

## Acceptance boundary

**CONDITIONAL PASS** for this offline Mock validation scope. Native route and reservation workflows were exercised in addition to automation. The reported baseline orange-route defect was not reproduced, so these changes do not prove its cause. Satellite/Cesium network tile delivery, real aircraft execution and sustained native right/middle 3D gestures remain outside the verified result. Historical Backend failures and all failed test/build attempts remain in Evidence. No test threshold or production separation check was relaxed.

Final UI-only follow-up: `ui-layout-final-2.log` / `ue-1790490588684060600` records 1 discovered, completed and passed (with environment warnings), 0 failed after the final caption change. `native/20-final-dynamic-reservation-layout.png` confirms a live transition from ordinary UAV ID to a single-line disabled owner caption while Plan B stays open. Its execution was started by the explicitly supplemental `api-final-layout-start-2.json` request. The preceding fill-only attempt did not resolve wrapping and was superseded by the no-auto-wrap caption correction; earlier evidence is retained.

Task-owned test clients and Backend were stopped after capture. User-owned Launcher and unrelated processes were preserved. Commit/push details are reported with the final Git state outside this document to avoid a self-referential commit hash.
