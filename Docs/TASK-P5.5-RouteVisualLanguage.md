# TASK-P5.5 — Route Visual Language

## Baseline and scope

Repository: WangYu0611/drone_security. Work is on the existing main checkout at D:/CodexPublish/drone_security; no branch or worktree was created.

Fetched public base: `70caac8b0c4414854613ac2d76f03d1f457ead76`. The P5.4 source HEAD `a4700991d7c31ef844310501b3185e497d5de656` belongs to a different repository/history. After explicit user authorization, its delta since P5.3 was synchronized as a new sanitized commit, preserving the public history and credential-free assets. No reset, forced merge or history rewrite occurred.

P5.5 implementation base: `414094076014dc076205195f33e656393cd1ee03`. Baseline provenance is recorded in `Docs/P54-Public-Sync.json`. Changes to Backend and execution protocol in that synchronization are pre-existing P5.4 changes, not P5.5 implementation changes.

## Design

- Real flight geometry stays waypoint-to-waypoint linear. Visual cylinder tangents are linear, with endpoint overlap limited to min(radius × 0.6, segment length × 0.2). Independent, non-colliding sphere joints keep continuity when waypoint markers are hidden.
- Core encodes altitude, interpolating StartColor to EndColor. Heights use the existing coordinate service's resolved ellipsoid height. When the coordinate service is unavailable in an empty component test, world height is used and explicitly labelled. Datum semantics and persisted altitude offsets are unchanged.
- Route-local range uses a 30 m minimum span. Palette: deep blue, cyan, yellow, orange, red-orange. Ordinary high altitude is distinct from pure conflict red.
- Incoming segment speed remains Waypoints[i].SegmentSpeed; closure remains last waypoint speed. Zero uses GetDefaultSegmentSpeedMps only for visualization and is never written back.
- Flow rate maps 1–15 m/s to 0.15–2.4 cycles/s. Shader phase is frac(u × density − Time × rate), where u is the projection from world-space Start to End. Animation therefore moves in the flight direction regardless of cylinder UV orientation. Speed never controls hue.
- Halo is 1.65 times core width, with amber/cyan/green/gray state colors. Selection increases intensity/opacity; other routes remain visible. Conflict overrides both layers with red/white pulse. Dark outer edges improve bright-surface contrast.
- New `M_CommandPathV2` is unlit, exposure-compensated and depth-tested. The original material is unchanged. Shader Time drives animation; no animation Actor Tick or per-frame MID allocation.
- Geometry count unchanged: reuse components and MIDs. Drag previews update only presentation from deferred waypoint handles, preserving canonical waypoints and flight spline until the existing commit on release. Cancel restores the canonical visual geometry.
- Execution monitor world routes consume acknowledged immutable snapshots. The old always-visible Slate route line is removed; existing telemetry and progress labels remain. No fabricated execution progress.
- Compact bilingual route legend, selected/conflict waypoint rings, and route state styling retain the existing Map input and Camera Lock paths.

## Result: CONDITIONAL PASS

Evidence lives under `Evidence/TASK-P5.5`. Online and offline fixture results are kept separate. The public copy has no Cesium credential, and online tiles return HTTP 401. The retained online regression run completed all four tests but each aggregate failed from Cesium errors; no non-Cesium test error was recorded.

`P55OfflineMap` is an explicit synthetic test map copied from the production level, preserving its georeference and UI while replacing online tiles with bright, white and dark static surfaces. It does not modify the production map, supply credentials, or prove satellite-imagery readability. Original regression assertions remain unchanged; an explicit test flag selects this fixture.

Final validated implementation HEAD: `9379c2d5235da8c3a50cdc8f8e7343563d02bfa0`. The following delivery commit adds tests, fixtures, raw evidence and this report. Its exact Final HEAD is the commit containing this report (`git log -1 --format=%H -- Docs/TASK-P5.5-RouteVisualLanguage.md`). The full delivery SHA, clean-status result, remote SHA and actual push exit code are recorded after commit/push in the local `Saved/P55-delivery-receipt.json` and final handoff; this avoids an impossible self-referential commit hash in the committed report.

### Build and automation

UE 5.8 Editor Win64 Development: **PASS**, `build-13.log`, 16.66 seconds, `Result: Succeeded`. Backend Release build also succeeded; Backend was not changed by P5.5.

| Final suite | Discovered / completed / succeeded / failed | Evidence directory |
|---|---|---|
| P55.RouteVisualEncoding + RouteVisualComponents | 2 / 2 / 2 / 0 | `offline-1790482265896976400` |
| P54.RouteEditCameraLock.2D + .3D | 2 / 2 / 2 / 0 | `offline-1790482290023902700` |
| Workflow.MapLiveDraftGuard | 1 / 1 / 1 with warnings / 0 | `ue-1790482308454099400` |
| P54.CameraDampingAnd3DRoute | 1 / 1 / 1 with warnings / 0 | same |
| P52.MapExecutionMonitor | 1 / 1 / 1 with warnings / 0 | same |
| Localization.FTextAndDraftIsolation | 1 / 1 / 1 with warnings / 0 | same |

**8/8 final scoped UE tests passed; mandatory P5.4 regressions 4/4.** All exited 0 and exported completed reports. The four runtime tests use the explicit offline fixture and retain all original assertions. Their warnings include existing receiver/configuration/georeference messages. The retained localization assertion also checks the new legend after a runtime culture switch. Backend fixture integration scripts completed successfully before the runtime tests. `final-verification.json` records the exact serial commands and exits.

**Online aggregate remains FAIL: 0/4**, retained in `ue-1790479400154118800`. All four completed but recorded Cesium HTTP 401 tile errors (11/9/7/7 Cesium events respectively). No offline result overwrites or reclassifies that online run. Real satellite imagery is not accepted by this delivery.

Material/localization/fixture commandlets successfully saved their requested artifacts, but returned 1 because of the inherited GameFeatureData asset-manager configuration error. Their entire process runs are **not** labelled PASS. The material loaded successfully in the final component tests and native window. No unrelated engine configuration was changed to suppress these diagnostics.

### Native acceptance matrix

| Item | Result and boundary |
|---|---|
| 2D continuity, zoom width, altitude, closure | PASS on synthetic bright/white/dark surfaces; final 01/02/08 and dark-surface evidence |
| Speed and direction | PASS using 1 vs 15 m/s native interval screenshots plus actual parameter/model evidence; 16× phase rate, Start→End including closure; no video claim |
| 3D height and vertical segments | PASS on synthetic georeferenced fixture; 05/06/vertical-3d |
| Joints without waypoint cover | PASS; 07 shows independent caps with all fixture waypoint actors disabled |
| Depth | PASS; opaque test block occludes both layers in occlusion-3d; depth test stays enabled |
| Native waypoint selection and drag | PASS for actual QA plan in 2D and 3D; final rendered line follows the moved handle |
| Height/speed before save | PASS; 60→120 m color/geometry and 2→1 m/s legend/MID update while dirty |
| Camera Lock, Save, Finish, Discard | PASS for native tested wheel/left-drag/UI journeys; Save retains lock, Finish unlocks, Discard restores saved visual |
| Right/middle sustained 3D gestures | NOT NATIVE VERIFIED; tool cannot sustain these buttons; preserved automation passes |
| Conflict | PASS for explicit component conflict and native red/pulse/clear rendering; detector logic unchanged |
| Planning / Confirmed | Native edit/read-only presentation verified |
| Active / Completed | Component automation covers data/state. Active screenshot is explicitly a presentation fixture. Backend state transitions: NOT NATIVE VERIFIED in this visual review |
| Three routes / independent MIDs | PASS native coexistence plus automation isolation; no cross-route color/speed mutation |
| 100 segments | Native spawn/animation/zoom smoke plus component drag/reuse checks; dense labels overlap |
| Localization | PASS native English/Chinese and retained-FText automation |
| Satellite / real buildings / real aircraft | NOT NATIVE VERIFIED |

Overall Native 2D and Native 3D are **CONDITIONAL** because the public copy cannot fetch authenticated Cesium imagery and sustained right/middle gestures are unsupported. The screenshot index and exact fixture boundaries are in `Evidence/TASK-P5.5/native/README.md`. All 12 requested filenames are present; reused views are documented, not presented as separate experiments.

### Performance and fixes found during native QA

Three 100-segment routes have 300 visual components each (100 Core, 100 Halo, 100 caps). Two hundred edits on one route took **0.167 s** in the final component test. Components and MIDs retain identity; unaffected geometry is skipped, and visual meshes never cook collision or affect navigation. Material Time supplies animation with no new Actor Tick.

Native 100-segment smoke sampled PID 61824 for 26.4 s: working set 3,279,097,856→3,283,668,992 bytes; private bytes +7,000,064; logs +4,020 bytes. No obvious runaway growth or render-component spam was observed. This is a short smoke check, not a GPU benchmark or long-duration memory proof. Native zoom responded. The inherited 100 waypoint labels overlap; long-route drag/churn is covered by component tests, not a native 100-point editing journey.

Retained failures drove these fixes:

1. High scene exposure made the initial Core black: the EyeAdaptation divisor floor was too large. Lowered to 1e-8; retained dark-frame evidence.
2. White additive flow and opaque Halo centers reduced hue readability. Changed to hue-preserving pulse modulation, dark silhouette and rim-only Halo alpha. Visual radius is 1.4× the existing screen-space radius; no Camera/global-exposure change.
3. A native moved handle left the line at its old position. Unchanged scale setters did not publish prior SetStartAndEnd changes. Added explicit UpdateMesh for changed geometry and a `bMeshDirty == false` regression assertion.
4. Explicit updates initially cooked unwanted collision, taking 61.237 s for 200 edits with repeated Chaos logs. Disabled cooked collision/navigation and skipped unchanged geometry; final time is 0.167 s with no triangle-log spam.
5. An experimental immediate Bounds assertion failed because render bounds update is deferred. Replaced that new assertion with the correct published-mesh dirty-state assertion, then verified actual rendered motion natively. No P5.4 assertion was removed or relaxed.

Earlier compile/link/material-generation failures are retained, including the commandlet/module DLL lock and a failed DeleteAllMaterialExpressions attempt (`!IsRooted`). The final updater modifies the existing Custom node only and preserves the graph.

### Changed files and assets

- `PathEditor/DronePathVisual.*`: pure altitude/speed/priority/topology model.
- `PathEditor/DronePathActor.*`: layered continuous geometry, stable materials, deferred visual preview and collision-free refresh.
- `PathEditor/DroneWaypointActor.*`: selected/conflict Ring and preview forwarding.
- `Map/MapExecutionWidget.*`: depth-tested acknowledged snapshot routes; no invented progress.
- `Map/MapShellWidget.*`, `Shared/ProductText.cpp`, localization catalog/resources: compact bilingual legend.
- `DroneOpsPlayerController.h`: read-only current-edit-route query; Camera implementation untouched.
- `Tests/RouteVisual*.cpp`, scoped P4 test-map selector and legend assertion, `Scripts/P55/*`: model/component/native fixtures and reproducible verification.
- New production asset: `Content/Command/Materials/M_CommandPathV2.uasset`; original `M_CommandPath.uasset` unchanged.
- Test-only assets: `Content/Tests/P55OfflineMap.umap`, `M_P55QABright`, `M_P55QAWhite`, `M_P55QADark`. Production map/credential assets untouched by P5.5.

`Evidence/TASK-P5.5/delivery-manifest.json` gives changed source/asset SHA-256 values, and `changed-files.txt` lists all non-evidence delivery paths. New assets are actual UE assets managed by Git LFS, not pointer text rewritten as assets. Evidence logs retain bytes via `.gitattributes`. The delivery audit scans new evidence for JWTs and preserves any raw originals under ignored Saved before redaction.

### Commits and publication

| Commit | Purpose |
|---|---|
| `414094076014dc076205195f33e656393cd1ee03` | Authorized sanitized P5.4 synchronization |
| `d8837ec269a2dc674ca828c501cadd52301071c1` | Visual encoding model |
| `291d169de3ed3c8454f13be8460c36e8d3be7311` | Layered rendering and Material V2 |
| `80d63682cd510306ecbbef249003d29e085c6393` | Legend, localization and interaction styling |
| `9379c2d5235da8c3a50cdc8f8e7343563d02bfa0` | Native-discovered drag/contrast/performance fixes |
| Commit containing this report | Final tests, fixtures, native evidence and report |

Publication gate: build PASS, scoped automation 8/8, explicitly bounded native CONDITIONAL, tracked worktree clean. Fetch origin/main again and stop if it differs from initial `70caac8b0c4414854613ac2d76f03d1f457ead76`. Only a normal `git push origin main` is authorized; no force. The post-push receipt records actual outcome and final clean status. Task-owned native processes were stopped through Launcher Runtime; no unrelated process was terminated.
