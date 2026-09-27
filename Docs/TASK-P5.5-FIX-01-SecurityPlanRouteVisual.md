# P5.5 FIX-01 — Security Plan Route V2 integration

## Scope and repository

Development is confined to `D:\CodexPublish\drone_security`, `main`, origin `https://github.com/WangYu0611/drone_security.git`. Preflight HEAD and origin/main were both `0fcbb6b64e51bc850a1184e543acfe4daeb91867`, with a clean worktree. No feature branch or new worktree was created. Historical worktrees were not used for development. `AGENTS.md` records this repository boundary.

## Defect and correction

The baseline had Route V2 in local/editor actors and execution snapshots, but `MapPlanMoveWidget::NativePaint` still drew orange route polylines/arrows and read-only Mission hydration only displayed the selected Mission. That was a real integration defect.

| Entry / state | Display owner after fix |
| --- | --- |
| Local path | Existing `ADronePathActor` |
| Mission Begin/Edit/Save | Existing editable Mission actor; other Missions remain read-only |
| Saved browse / READY / DEPLOYED | One presentation actor per saved Mission; selected Mission emphasized |
| Plan Move | Presentation-only actors, original world geometry plus preview transform |
| EXECUTING / COMPLETED | Acknowledged immutable execution route snapshots; corresponding saved Mission visual removed |

`FPlanRouteVisualSet` shares the existing PlanUI ellipsoid resolution and CoordinateService decoder with editor hydration and execution presentation. All geometry/material/state encoding remains in `ADronePathActor`, `DronePathVisual`, and `M_CommandPathV2`. Incoming segment speed and final-point closing speed remain unchanged. Original resolved altitude colors are preserved during preview transforms, including camera-radius/material refresh; shader world endpoints move with the preview.

Move Slate drawing now contains only bounds, center cross, handle, and caption. Dragging changes presentation transforms only. Confirm continues through the existing Backend move API; Cancel destroys preview actors and restores saved presentation. Expired leases also release previews. No Backend schema, HTTP/WS protocol, or Camera Lock core changes.

Saved presenters exclude the edited Mission and any Mission owned by a visible execution snapshot. Move suppresses saved/editor/execution ownership for its plan. Independent active execution groups remain supported. Cached read-only editor geometry is hidden and noninteractive, with at most one cached actor; it is replaced on selection and cleared on move. Retain/reset operations destroy obsolete presentation actors on plan/state changes and widget teardown. Existing conflict detection is reused against displayed saved/edit or move routes, without adding presentation copies to global conflict scans.

## Verification

Final UE 5.8 Development Editor build: **PASS**, `Evidence/TASK-P5.5-FIX-01/build-8.log` (18.65 s). Final automation: **9/9 discovered and completed, 4 Success + 5 SuccessWithWarnings, 0 failed, 0 not run**. Each runtime test ran serially in a fresh Editor. The original eight tests and assertions were preserved; the ninth test exercises real Security Plan widgets and Backend ACKs. Machine-readable results: `Evidence/TASK-P5.5-FIX-01/accepted-verification.json`; complete runner output: `final-run-2.log` and `final-verification.json`.

| Test | Final result |
| --- | --- |
| `DroneOps.P55.RouteVisualComponents` | PASS |
| `DroneOps.P55.RouteVisualEncoding` | PASS |
| `DroneOps.P54.RouteEditCameraLock.2D` | PASS |
| `DroneOps.P54.RouteEditCameraLock.3D` | PASS |
| `DroneOps.P55.SecurityPlanRouteVisual` | PASS (warnings) |
| `DroneOps.P4.Localization.FTextAndDraftIsolation` | PASS (warnings) |
| `DroneOps.P4.P52.MapExecutionMonitor` | PASS (warnings) |
| `DroneOps.P4.P54.CameraDampingAnd3DRoute` | PASS (warnings) |
| `DroneOps.P4.Workflow.MapLiveDraftGuard` | PASS (warnings) |

Runtime warning events remain in each `index.json` and Editor log, including isolated Backend configuration and the offline fixture's absent RealTimeDroneReceiver. They are not silently discarded. Earlier build failures, fixture content-revision error, and missing canonical world-coordinate Move fixture failure remain in the build/first-run artifacts; final fixture hydration uses real Map editors and unchanged Backend validation. The native preview visibility failure and fix are documented below.

Implementation commits: `acdeae3` (shared lifecycle presentation) and `4375739` (native-discovered preview width and selected execution emphasis). Final build and tests include both repairs.

The native environment uses the existing `/Game/Tests/P55OfflineMap` (production coordinate service and real Command/Map widgets, offline ground fixtures) and an isolated Mock Backend. This does not verify authenticated Cesium satellite/terrain delivery or real aircraft execution. No standalone `P55.QA` route generation is used for the Security Plan journey.

## Retained native finding and repair

The first native Plan Move attempt failed visibility despite passing actor/material ownership checks. Evidence: `native/retained-move-invisible.png` and first Map log. `P55.QA inspect` was used read-only (no fixture generation): Route V2 MIDs existed but core scale was 0.007, the default 0.35 cm radius.

Cause: the existing PlayerController early return during Move prevented `TickView`, which maintains camera-dependent route width. The Move branch now calls the existing view refresh before returning; competing pointer polling stays suppressed. Camera Lock implementation and its guards are unchanged. The new lifecycle test invokes the actual virtual PlayerController tick during Move and asserts a visible camera-scaled mesh width, covering this native-discovered gap. The repaired native journey and final regression results supersede the earlier conditional run; the failure is not erased.

## Native result: CONDITIONAL PASS

Real Command/Map UI journey: open persisted three-Mission plan → select Mission A → Begin Edit → change P02 Ellipsoid altitude 70→90 m and P03 incoming speed 12→14 m/s → Save Draft (lock retained) → Finish (unlock and V2 retained) → select/save B and C → reopen persisted plan → Move Cancel → Move Confirm → pre-deployment review/validate/deploy → explicitly start and complete Mock C → reopen and explicitly start/complete closed A.

Evidence is in `Evidence/TASK-P5.5-FIX-01/native/`, with numbered screenshots and `result.json`:
- Screenshots 01–05: three saved routes, real parameter edits, save/finish, and Mission selection.
- Screenshots 06–10: repaired Move preview, drag, cancel restoration, and confirmed saved routes.
- `during-preview.json` and `after-cancel.json`: Backend route JSON is exactly unchanged.
- `after-confirm.json`: all 15 points share translation `[-6010.197957, 5469.950388, approximately 0]` cm; maximum disagreement is `1.79e-7` cm. Closed flags, speeds, and wait times unchanged.
- Screenshots 11–15 and `deployed.json` / `executing.json`: review, deployment with zero auto-started executions, then real acknowledged Mock execution and completion.
- Screenshots 16–20 and `completed.json`: closed route A, closing leg, `closure_completed=true`, final completed geometry, corrected active-Mission legend, and 3D viewing.

The Deploy UI performs validate → review → deploy sequentially; READY is a transient Backend stage, not a separately captured stable screen. Native execution history contains completed `execution-22` (open C) and `execution-24` (closed A). No standalone route generation was used. Read-only `P55.QA inspect` diagnosed the first invisible preview.

The execution-selected emphasis/legend now follows active Mission context, with UAV selection only as fallback when no Mission is selected. This prevents a background Mission's range from being shown while monitoring the selected execution.

Limits: offline ground/georeference fixture only, existing visible Lumen exposure warning retained, Mock rather than real aircraft. Sustained native right/middle camera drags were not revalidated; preserved camera automation covers their mathematical/input ownership regressions. No online Cesium terrain delivery claim.
