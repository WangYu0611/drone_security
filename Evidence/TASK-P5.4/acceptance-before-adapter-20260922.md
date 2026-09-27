# TASK-P5.4 — delivery and acceptance record

**P5.4 FINAL RESULT: FAIL for full requested acceptance.** The implemented Mock functionality and passing checks below are usable for local review, but mandatory unified Mock/Real adapter migration and the complete native A-L acceptance are unfinished. This is not a claim that every implemented feature failed.

This branch extends the existing Backend-authoritative SecurityPlanStore and its persisted execution state. Deployment remains configuration activation; starting or scheduling is an explicit execution action. Current end-to-end execution evidence is **Simulation / Mock**, not flight-controller acceptance.

## Reproducible baseline

- Worktree: `C:\Users\wy331\Documents\UE5DroneControl-p5-4`
- Branch: `feat/triple-screen-p5-4-3d-multi-uav`
- Accepted P5.3 base: `9f5dac4d398d5544529aa8cb34948c313d0f0c27`
- Protected main: `f6ed7f97fabc21c202333528b0cb030dd4b1b2fe`
- No push, merge, tag, reset, stash, or modification of the P5.3 checkout.
- Runtime fixtures, ports and owned processes are isolated from accepted P5.3 business data.

## Implemented behavior

### Map Camera

2D browsing supports thresholded left drag and wheel zoom toward the pointer's focus plane. Camera presentation is damped; primary release settles to the pointer target without amplifying coalesced events into extra displacement. Route-edit clicks add on release; dragging a waypoint and map camera gestures have separate paths. 3D supports right orbit, middle pan, bounded pitch and wheel zoom. Position, distance and rotation approach targets exponentially; mode changes preserve the map center and scale. Configuration is in `[CommandMapCamera]` in `Config/DefaultGame.ini`.

### 3D Security Plan

Existing `altitude`, `waitTime` and `segmentSpeed` remain canonical; no parallel route schema was created. Additive `altitude_reference` is `Ellipsoid`, `AGL` or `MSL`. Missing reference retains legacy ellipsoid meaning. AGL requires `terrain_ellipsoid_m`; MSL requires `geoid_undulation_m`. The Backend rejects an unresolved datum. Runtime snapshots resolve once to ellipsoid height; editable central paths retain their original semantics.

Map waypoint controls edit height and hover time, and expose AGL versus ellipsoid. New map-click points default to 80 m above the hit surface. This is configurable under `[MissionRoute]`. Existing Cesium coordinate conversion builds world positions. Moving AGL geometry re-samples loaded collision terrain before save/confirm; missing terrain or below-ground points block saving. Vertical route geometry and labels retain the height and hover fields. MSL import is supported with an explicit geoid offset; there is no automatic geoid service or native MSL selector.

### Multi UAV Mission

`assigned_uav_ids` extends each existing mission; legacy `assigned_uav_id` remains its primary member. Multiple missions compose an existing Security Plan. The editor has assignment checkboxes and spacing. `Line` derives distinct fixed lateral slots from the initial route heading. Column, V, Grid and Custom are reserved identifiers that reject clearly until implemented. The central plan route is never mutated to make per-aircraft trajectories.

Safety defaults to 1.5 m through one Backend accessor, with optional `safety_separation_m`, per-UAV `radius_m` and `safety_margin_m` in the existing Mock configuration. Required distance is the larger of separation and both radii plus margin. Piecewise continuous relative-motion minima check approaches, travel, hover, closure and final occupancy, including other aircraft. The runtime also checks swept motion between ticks and freezes unsafe motion. Conflict responses include the UAV pair, predicted minimum and required separation. This is basic local deconfliction, not obstacle avoidance or ORCA/RVO.

### Group Control

`selected_uav_ids` travels through Shared Operational Context and is persisted with a valid primary member. Fleet Ctrl/Shift selection and the existing Map Shift rectangle feed the same mechanism. Selection does not alter Video Target. Group Move uses the existing execution state machine: the first selected member anchors a WGS84 ellipsoid target, while other members retain relative offsets. A user-supplied target radius can trigger a compact Line re-layout; insufficient space or unsafe approaches reject atomically. It does not infer free space from buildings. Pause/resume/abort operate on the entire execution group.

### Scheduled Mission

Existing execution documents gain `SCHEDULED`, UTC ISO8601 deadline and durable epoch. UI entry uses local civil time; Windows converts using the selected date's timezone rules. The same group and state machine service immediate starts, scheduled starts and group movement. Start Now, Reschedule and Cancel are available. Due groups revalidate aircraft positions and trajectories before starting. Restart restores schedules; this is one-time scheduling, without recurring jobs.

### Command Situation Map

The existing `UCommandTacticalMap` is revived inside Overview with its existing XYZ basemap, friendly UAV selection, heading arrows, selected markers, central plan routes, active execution routes, alert-associated UAV locations and registered enemy targets. Pan disables Follow; Follow is explicit. Overview retains recent operational events and the separate full log page. No arbitrary alert coordinates or unimplemented geofences are fabricated.

## Interfaces

All mutations use existing `/api/security-plans` with request IDs, expected configuration versions, execution control versions and Backend acknowledgement. Additive actions are `execution_group_move`, `execution_start_now`, `execution_reschedule`, `execution_cancel`. Existing actions and immutable deployment/route snapshots remain compatible. `SecurityPlansChanged` distributes execution updates; configuration revision and execution version stay separate.

Coordinate/data compatibility is distinct from real aircraft acceptance. The preserved real path is DroneManager / UDP telemetry and NED commands / existing ExecutionEngine. Their hashes are recorded in `Evidence/TASK-P5.4/real-interface-hashes.json`. This task did not activate hardware control or validate a real aircraft/video feed. The legacy real execution engine has not yet been migrated into the new persisted SecurityPlan execution controller; therefore the requested fully unified Mock/Real adapter architecture is **not complete**. Preserving old files does not prove that architecture or hardware acceptance.

## Evidence and scope

- Backend: `build/backend-08.xml`, `build/tests-08.log`: 107 discovered, 105 passed, 2 inherited failures (`SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`). No zero-test result is counted as PASS.
- Launcher: `build/launcher-tests.log`: 15 tests passed.
- P5.4 HTTP/WS: `protocol/result.json`: 8 checks; `protocol/resume-result.json`: 2 restart/deadline checks. Synthetic clients against a real task-owned Backend; not native UI evidence.
- Four UAVs followed heights 40/80/120/60 m with 5/10 second waits. 72 sampled group snapshots have minimum measured separation **2.556129809 m**; analytic continuous checks are separately covered by Backend tests. Sampling alone is not a collision proof.
- Actual UTC +2 minute scheduling survived a Backend process restart and started at 11:44:11.388Z for a deadline of 11:44:11Z (0.388 s after deadline).
- P4/P5/P5.1/P5.2 inherited protocol suites: 21/8/11/14 checks passed in `regression-1790077543334810200`.
- Original P5.3 protocol: 8 checks passed, then new safety correctly rejected its ingress within 0.562875853 m of a parked UAV, below 1.5 m. That failure remains in the original log.
- Unchanged P5.3 script with an explicitly separate fixture moving only idle aircraft clear of the ingress: 10 checks passed in `p53-safe-1790078114691792800`. This supplements and does not overwrite the original failure.
- First final UE regression batch: eight separate Editor processes, 8 tests discovered/performed, 8 succeeded with warnings, 0 failures, exit 0 and explicit queue completion. Evidence: `ue-regression-1790078139498719400`.
- Initial new UE camera/height test: 1 discovered/performed, 1 succeeded with warnings, 0 failed, exit 0 in `ue-camera`.
- After the waypoint property refresh and UAV status-label refinements, `ue-regression-1790080041802756800` ran MapLiveDraftGuard, CameraDampingAnd3DRoute and FTextAndDraftIsolation: 3 discovered/performed, 3 succeeded with warnings, zero failures, exit 0.
- The intervening batch `ue-regression-1790079547920521300` is retained as a failed batch: two tests passed, then camera/locale processes returned 777003/3 with no exported test results under memory pressure. Neither zero-test process is counted as PASS. The runner now returns failure for incomplete or failed batches.

Retained build failures include memory retries, a task-owned Map process holding the DLL, an incorrect localization config filename and a C++ test compilation error. Old binaries run after that failed test build are not claimed as verification of the new tests. Reports preserve actual logs and distinguish successful later reruns.

## Re-run

- Build Backend in `Backend/build-p54` using the configured Visual Studio/vcpkg toolchain; run `Release/DroneBackend_tests.exe` with XML output.
- `python Scripts/P54/protocol_regression.py` creates fresh isolated protocol stores and records original outcomes.
- `python Scripts/P54/p53_safe_regression.py` runs only the explicitly safe P5.3 fixture.
- `python Scripts/P54/ue_regression.py` creates synthetic Backend fixtures and launches each of eight UE tests in a fresh process. Optional arguments select Map tests by suffix.
- `python Scripts/P54/qa_host.py` owns the native QA Backend and role processes. Control file: `Saved/P54-QA/command.json`. It only stops its owned processes. Send one command and wait for its completion before the next.
- `Backend/tests/p54_mission_integration.mjs` exercises the four-UAV scenario; its `--resume` phase must follow an actual owned Backend restart using the same durable store.

## Boundaries still requiring acceptance

- Full physical A–L interaction acceptance, including right/middle drag and modifier rectangle/list gestures, must not be inferred from direct component automation or protocol fixtures.
- Actual Cesium terrain availability is environmental; AGL is based on loaded collision surfaces and explicit sampled offsets, not an independently validated elevation model.
- Nonpolar local missions only: formation/group geometry is bounded to 10 km; the safety comparison uses a local tangent approximation. This is not certified geodesic/flight safety software.
- Line slot orientation remains fixed from the first segment, rather than rotating formation through arbitrary turns; analytic trajectory checks may conservatively reject a route.
- Other formation patterns, sensed free-space packing, automatic geoid data and full Real adapter unification are unfinished.
- Native visual spacing of real aircraft meshes and simultaneous independent execution groups need separate visual acceptance; numeric center separation alone is not mesh collision acceptance.
- Overview panel collapse and exhaustive key-target/geofence UX are not complete.


## Final-build notes

`build/ue-08.log` records a successful UE 5.8 Development Editor build after reducing compilation concurrency under memory pressure (667.12 seconds). `build/ue-09.log` records the later property-refresh/status-label build (54.61 seconds), and `build/ue-10.log` the first camera input refinement (16.41 seconds). Earlier unsuccessful build logs remain separate.

`build/localization-02.log` records GatherText completion with result 0 and generated manifest/archive/locres assets. Its Unreal startup still logs the inherited missing GameFeatureData asset-manager rule and Cesium source prepass warnings. This is not a warning-free engine launch.

The native fixture in `native/fixture.json` was created through HTTP/WS by `Scripts/P54/native_fixture.mjs`. It contains four ellipsoid heights 40/80/120/60 and hover 0/5/10/0 seconds. Screenshots of this fixture demonstrate display/interactions only; they do not demonstrate native creation of the fixture.

A local ignored `Launcher/config.local.json` points to the P5.4 binaries and a fresh `Saved/Stage1P54` runtime on ports 19980/19981. `DroneSecurityLauncher.cmd` remains the manual entry point. The acceptance store `Saved/P54-QA` is separate.

Additional UX limitations: Command map labels can overlap at distant zoom levels. AGL is relative to loaded collision surfaces, including the simplified 2D surface; it is not verified physical terrain clearance.


## Native A-L acceptance matrix

A protocol fixture or direct component automation is not a substitute for physical interaction acceptance. The computer-use driver provides left drag and wheel actions but no supported held-modifier/right-button/middle-button drag. Those gestures remain unverified rather than being simulated through an unrelated input driver.

| Test | Evidence obtained | Result boundary |
|---|---|---|
| A - 2D Camera | Native main Map wheel; Command small-map wheel and left drag; direct camera damping/settling test | Final native Pan → Zoom → Pan produced the expected direction and displacement after fixes; the second Zoom was interrupted by user Escape/app closure. Continuous subjective smoothness still needs manual review. |
| B - 3D Camera | Native 3D display; automated pitch bounds and damped target integration | Physical right orbit/middle pan/select sequence not completed. |
| C - 2D/3D | `native/map-3d.png`, `native/map-back-2d.png`; component center/scale assertions | Same region retained in the observed round trip. |
| D - 3D Route | Four height levels 40/80/120/60 survive WGS84/Cesium round trip; native 3D route image | Fixture was created through API; native creation of all four points and edit/save path not fully demonstrated. |
| E - Hover | `protocol/hover-observation.json`, execution snapshots and Backend completion tests | 5/10-second waits observed numerically; no complete native screen recording. |
| F - Multi UAV Plan | Three-UAV unit completion and four-UAV protocol completion with distinct trajectories | Native mesh non-overlap is not verified. |
| G - Safety | Continuous piecewise approach/travel/hover checks; swept tick freeze; observed minimum 2.556129809 m | Within the local Mock model; not physical collision/obstacle certification. |
| H - Group Selection | Four-member shared selection and atomic offset-preserving group move through HTTP/WS | Native Ctrl/Shift/rectangle gestures and full visible group journey not completed. |
| I - Conflict | Narrow target space and intersecting approaches reject atomically with structured conflict reasons | Native conflict-panel interaction not completed. |
| J - Schedule | Actual UTC +2-minute request started 0.388 s after the deadline | API entry, not native schedule-form entry. |
| K - Backend Restart | Actual owned Backend process restart retained the schedule and executed it | PASS for the recorded isolated Mock scenario. |
| L - Command Situation Map | Native pan/zoom, route/UAV display, selection event and log; independent Map hydrated UAV04 | Video Target remained null/version 0; label crowding and exhaustive alert/target UX remain. |

`native/command-pan-zoom.png`, `native/command-select.png`, `native/selection-before.json`, `native/selection-after.json` and `native/map-hydrated-uav04.png` support the Command selection evidence. The primary UAV changed from UAV01 to UAV04 while the four-member selection remained intact. Video Target did not change.

## Regression coverage and gaps

The successful inherited protocol/UE suites cover plan create, route edit/save, closed-loop geometry, Whole Plan Move, review/deploy, explicit execution controls, context replication, localization, coordinate conversion and explicit Video Target isolation. Backend tests exercise HTTP-side state/domain validation; live protocol scripts exercise HTTP and WebSocket transport. This is not a complete new physical walkthrough of every Command/Info/Video/Alert page. Alert locations remain tied to registered UAVs where the protocol provides no independent position. No real video stream, radio, flight controller or UAV was activated.

Two inherited Backend failures remain. Original P5.3 protocol start is newly rejected by the added 1.5 m safety rule; only its separate safe fixture completes all ten checks. This behavior change is disclosed and prevents an unconditional claim that every old fixture passes unchanged.

## Evidence handling

Review-copy logs are scanned for JWT credentials before commit. `redaction-manifest.json` records original/review SHA-256 values and replacement counts; untouched originals are preserved locally under ignored `Saved/P54-raw-evidence`. Failed builds/tests are retained alongside successful reruns. Screenshot files are unedited native captures. QA uses only owned process IDs and leaves protected baseline processes untouched.


## P5.4 FINAL RESULT

**FAIL — 完整需求验收未通过；已完成的 Mock 功能和局部验证保留供本地评审。**

| Required field | Delivered state |
|---|---|
| Branch | `feat/triple-screen-p5-4-3d-multi-uav` |
| Base HEAD | `9f5dac4d398d5544529aa8cb34948c313d0f0c27` |
| Final HEAD | Exact final documentation/evidence commit is reported in the final delivery message; `git rev-parse HEAD` reproduces it. |
| Map Camera / 2D / 3D / Damping | 2D pan/zoom and 3D orbit/pan/pitch targets implemented; exponential damping and same-area mode switching; native left-input fixes and final evidence below. Right/middle physical gestures still require review. |
| 3D Security Plan / Altitude / Hover / 3D Path | Reuses existing fields; explicit ellipsoid/AGL/MSL metadata; 40/80/120/60 m and 5/10 s fixture, height round trip and distinct runtime snapshots verified. |
| Multi UAV Mission / Assigned UAV / Formation / Safety Separation | Multiple assigned UAVs; Line formation; per-aircraft route snapshots; central 1.5 m minimum plus optional radius/margin; continuous local trajectory checks. |
| Group Control / Multi Select / Group Move / Conflict Detection | Shared multi-selection, anchor/relative-offset movement, compact Line re-layout or atomic rejection; group pause/resume/abort. Protocol evidence is separate from pending native modifier/rectangle acceptance. |
| Scheduled Mission / Persistence | UTC durable deadlines; local-time UI; Start Now/Reschedule/Cancel; actual +2-minute schedule survived Backend restart and began 0.388 s after deadline. |
| Command 2D Situation Map | Existing component reused for UAV/status/heading/routes/alerts/targets; native pan/zoom/selection and event log observed; selected UAV replicated without changing Video Target. |
| Coordinate Conversion Regression | Backend conversion tests included in the 107-test suite; UE WGS84/Cesium height/geometry round trips passed; preserved real-interface hash inventory unchanged. |
| Backend Tests | 107 discovered, 105 passed, 2 inherited failures; Launcher 15 passed. |
| Protocol Tests | P5.4 8 + restart 2 passed; inherited P4/P5/P5.1/P5.2 21/8/11/14 passed; original P5.3 blocked after 8 by new safety, separate safe fixture 10 passed. |
| UE Tests | Eight-case isolated regression 8/8; later three-case refinement 3/3; input-callback revision 2/2. Final pointer-event rerun 2/2 passed, recorded below. Successes include engine warnings; incomplete/OOM failures retained. |
| Native Tests | A-L matrix above; partial physical coverage, no full native PASS claim, no real-flight claim. |
| Known Issues | Label crowding; retained Backend failures; original P5.3 fixture safety rejection; Cesium/startup warnings; environmental terrain data; limited local tangent geometry; full mesh/gesture/native journey not accepted. |
| Deferred / unfinished | Unified Real adapter migration; complete physical A-L walkthrough; automatic geoid source; sensed free-space packing; non-Line execution patterns (reserved only); panel-collapse/exhaustive geofence UX; simultaneous independent group visual acceptance. |
| git status | Final delivery must be clean; protected main and P5.3 identities verified separately. |

No hardware was activated. Before claiming complete P5.4 acceptance, finish the shared Mission Controller/Command Model adapter migration and the missing native acceptance, then evaluate the retained regression differences. Hash preservation alone is not a substitute for that work.


## Final input refinement and interruption

The native pass found three actual primary-drag issues and retained their evidence: GameAndUI polling lost primary state/press coordinates, screen Y was inverted, and a coalesced release could produce excessive inertial movement. The final implementation observes Slate press/release positions without consuming UI events, uses the correct coordinate sign, and settles to the target after release. The observer unregisters on both unbind and destruction. Middle/right pan also converts Unreal's inverted MouseY once.

`build/ue-15.log` records final successful UE 5.8 compilation (16.39 s). Intermediate builds and images `map-pan-inverted-y.png` / `map-pan-overshoot.png` remain as failed-observation evidence. Final `map-final-before.png`, `map-final-pan1.png`, `map-final-zoom1.png`, `map-final-pan2.png` show forward/down and reverse/up pan with the expected displacement around a wheel zoom. Earlier `map-pan-after.png` and `map-pan-settled.png` capture the stationary endpoint of the first direction fix.

The final second zoom was stopped by physical Escape. The user then reported that Codex had stalled, closed it, and requested continuation. `native/user-interruption.json` records that boundary. On resumption, saved screenshots/source were intact, the old QA Backend/Map and computer-use helpers were no longer alive, and the final automated rerun was started in fresh processes. This interruption is not classified as a native PASS. `native/qa-logs` preserves task-owned runtime logs; the copied last ownership snapshot is historical, not a claim those PIDs remain alive.


Final source revision: `e95d6f4` (the complete hash is in `delivery-gate.json`). Fresh rerun `ue-regression-1790082305127418100`: CameraDampingAnd3DRoute and MapLiveDraftGuard, **2 discovered/performed, 2 succeeded with warnings, 0 failed, both exit 0 with queue completion**. It includes same-frame gesture displacement, correct downward direction and no release-velocity overshoot assertions. The full eight-test batch belongs to the earlier implementation revision, so it is not misrepresented as an eight-test run at this final source revision.

Additional unfinished visualization: the Map execution widget renders the latest execution group; simultaneous independent groups are not fully visualized. An old aborted execution monitor may remain visible beside a newly selected draft route, with separate names but cluttered context. These need further UX work before full acceptance.

The final review-copy redaction manifest contains 28 files; every recorded review hash is verified. Original logs remain local and ignored. Main and the clean P5.3 checkout retain their protected hashes; all 38 preserved interface/conversion hashes were rechecked after the final source change.
