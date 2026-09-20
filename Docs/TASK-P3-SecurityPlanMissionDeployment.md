# TASK-P3 — Security Plan / Mission / Route / Deployment Foundation

## 1. Final Result

**CONDITIONAL PASS**. Backend-authoritative creation, mission assignment, route persistence, validation and atomic business deployment are implemented. Native Command / Map / Video acceptance A–J completed on the P3 branch. The five retained P2 limitations remain unchanged; an isolated UE startup assertion is separately recorded below. This is not real flight or media acceptance.

## 2. Git / Recon

Base: `feat/triple-screen-p2-command-center-v2`, `88b95a952348fcd9bcda328cb3415a56523de5c3`.
Worktree: `C:/Users/wy331/Documents/UE5DroneControl-p3`; branch `feat/triple-screen-p3-security-plan`; initial status clean.

1. No persisted SecurityPlan domain exists. DroneTaskState and UDroneMissionService describe legacy execution/pause, not a security mission.
2. Existing routes: FDronePathSaveData/FDroneWaypointSaveData, FDroneWaypoint, ADronePathActor. Reuse these actors and serialized pathId/bClosedLoop/waypoints/location/segmentSpeed/waitTime fields; add geographic metadata for portable persistence.
3. Existing Main Map saves via UDronePathSaveLibrary into Saved/DronePaths JSON. This remains the legacy import/export format; P3 saves mission-linked paths to Backend.
4. Backend POST /api/arrays consumes paths and starts assembly/execution. It is not a route CRUD endpoint and must never implement P3 deploy. No persisted route CRUD found.
5. OperationalContext already owns active_security_plan_id and active_mission_id. Keep these exact fields (active_plan_id is the product concept, not a duplicate protocol field). Existing generic Map PATCH is restricted; use an additive validated plan-selection endpoint without weakening the P1 generic role contract.
6. UOperationalEventStore already has PLAN/MISSION categories and a 500-entry session projection. Extend it to project persisted Backend domain events; Clear Display remains a UI watermark.
7. UAV assignment must validate HttpServer's existing drone_records_; UE dropdown uses UDroneRegistrySubsystem descriptors. No new UAV database.
8. CommandMapInteractionService owns camera/map mode and delegates planning to PlayerController.
9. Main Map selection currently routes through P1 Backend and Registry. Mission route focus must not change Global Active UAV or Video source.
10. CommandCenterPanel is embedded in CommandShellWidget; Main Map has its own MapShellWidget. Add small business panels to these existing shells.
11. Backend persists JSON; OperationalContext uses temp-file + atomic replace and a mutex. Use the same mechanism for one plan/mission/path domain document, with copy/validate/persist/publish transactions.
12. Existing fixtures: P2.QA synthetic GPS, P1 HTTP/WS integration, UE OperationalContext/CommandCenterV2/MapExtraction/CommandMapMode/Video lifecycle tests.
13. ADronePathActor already renders polylines/waypoint handles in 2D/3D; PlayerController supports waypoint editing/dragging. Preserve business coordinates and historical 3D occlusion.
The P2 Git Gate was already complete at start. Local author: `WangYu0611 <331508364@qq.com>`. No global Git configuration change. All implementation is confined to the independent P3 worktree; main, P1 and P2 references are protected. No merge, reset, stash, or removal of existing user data.

## 3. Architecture

`SecurityPlanStore` owns one domain document. A mutation takes a mutex, copies state, validates, atomically persists, then publishes. Clients render acknowledged snapshots; there is no optimistic READY or DEPLOYED. `UOperationalContextSubsystem` reuses the P1 HTTP endpoint and context WebSocket, with `SecurityPlansChanged` and a GET after subscription. Monotonic snapshot versions reject stale deliveries. No second WebSocket or execution engine was added.

## 4. Security Plan Model

Fields: `id`, `name`, `description`, `status`, `version`, `created_at`, `updated_at`, `mission_ids`, `validation`. States: DRAFT, READY, DEPLOYED. Edits invalidate readiness and require validation again. DEPLOYED plans are read-only. IDs and timestamps originate in Backend.

## 5. Mission Model

Fields: `id`, `plan_id`, `name`, `assigned_uav_id`, `route_id`, `status`. A mission belongs to exactly one plan, with zero or one UAV and route while drafting. Main Map supports add, rename, delete, assignment and unassignment. Mission deletion removes its associated persisted path; it does not delete a UAV record.

## 6. UAV Assignment

Dropdown values come from the existing Registry descriptors; Backend checks its existing drone records. Legacy `d1` descriptors map through the existing numeric slot to canonical `UAV-01`. Duplicate UAV assignment within one plan returns `UAV_ALREADY_ASSIGNED` without changing state. The same UAV may appear in a different plan; P3 does not schedule or reserve aircraft across plans.

## 7. Route Integration

Reuse `FDronePathSaveData`, `FDroneWaypointSaveData`, `FDroneWaypoint`, `ADronePathActor` and `ADroneWaypointActor`. Persist existing `pathId`, `bClosedLoop`, `waypoints.location`, `segmentSpeed`, `waitTime`; additive `route_id`, `latitude`, `longitude`, `altitude`, `sequence` preserve geographic meaning. `ICoordinateService` converts only at the UE boundary. Legacy Saved/DronePaths import/export remains separate from authoritative mission saves. No P3Route database, conflict solver or playback path was introduced.

## 8. Route Editor

PLAN EDITOR opens a narrow panel in Main Map. ADD / MOVE WP uses the existing map-click, waypoint selection, axis dragging and deletion machinery. CLEAR ROUTE changes the local draft; SAVE ROUTE persists it. An empty route is valid draft data but is not READY. Saving exits edit mode; changing mission reloads that mission's saved route. Remote deployment terminates editing and reloads the authoritative read-only path. P3 permits editing the first waypoint while retaining legacy formation-origin locking outside mission mode. Native A–J covers addition/save/restart; supplemental K evidence covers restored input fields and clear/delete operations. Mission-only handle visuals and hit targets now scale with the map; 2D axis hit testing uses projected screen segments so the basemap cannot intercept them. Legacy and 3D picking stay unchanged. The final UE route test moves the first waypoint through the existing axis API and checks saved coordinates, untouched neighboring point and retained wait time. Native tool-driven axis dragging is NOT EVALUABLE: diagnostic input logs show that the tool can deliver mouse-down only after its cursor has reached the destination. Trial drags were discarded by reloading the saved QA route; they are not claimed as successful movement.

## 9. Backend API

GET `/api/security-plans`: full authoritative snapshot. POST on the same endpoint: registered ONLINE `instance_id`, `action`, `expected_version`, and action fields. Actions: `create_plan`, `update_plan`, `add_mission`, `rename_mission`, `delete_mission`, `assign`, `save_route`, `validate`, `select`, `deploy`. Map edits; Map/Command select and validate; only Command deploys; Video mutations return 403. Original P1 generic PATCH role rules remain unchanged. Stale versions return 409 with the current state. Invalid input and duplicate assignments fail without partial writes.

## 10. Persistence

Backend JSON contains version, next ID, plans, missions, paths and events. Temp-file replacement follows existing OperationalContext storage practice. Tests use isolated `Saved/P3QA/data` and loopback ports 19180/19181, with no real drone endpoints. Exact pre/post Backend restart comparison passes after enabling Boost JSON precise-number parsing; the original imprecise trial is retained as diagnostic evidence. Native Map restart and combined Command/Map restart preserve domain data and selection exactly.

## 11. Active Plan / Active Mission

Reuse existing `active_security_plan_id` and `active_mission_id` in OperationalContext. `active_plan_id` is the product concept, not a duplicate incompatible protocol field. Selection is Backend validated and broadcast. Selecting a plan clears the previous mission; selecting a mission verifies ownership. These actions do not change `active_uav_id`. Late domain hydration now waits for referenced objects before filling editor text fields.

## 12. Command Center

The existing Command center panel shows selected plan, mission, status, mission count, UAV assignment count, ready route count and blocking reasons. Plan/Mission selection synchronizes with Map. DEPLOY PLAN opens explicit confirmation with CONFIRM DEPLOY and CANCEL. Domain updates clear obsolete feedback and invalidate an old confirmation. Existing tactical map and Event Log remain in the shell; the details button was moved to avoid selector overlap.

## 13. Main Map

Starts in OPERATIONS with the editor collapsed. Main Map owns business editing. Selected Mission UAV uses a distinct highlight from Global Active UAV; focusing a route targets its first waypoint rather than changing aircraft selection. The saved polyline is restored after restart and displayed in 2D/3D. Low-altitude 3D occlusion remains a documented limitation; route coordinates were not raised to mask it.

## 14. Video Regression

Video source files remain unchanged from P2. Protocol tests verify plan operations preserve Global Active UAV; UE tests verify plan snapshots do not enable Follow or change Local Source. In native A–J the same Video process retained UAV-01 with Follow OFF through B–I. With Follow ON, selecting UAV-02 in Command changed Video to UAV-02. The synthetic fixture has no camera feed: NO SOURCE is expected and does not demonstrate WebRTC/media playback.

## 15. Event Log

Persisted events include PLAN_CREATED, MISSION_CREATED, UAV_ASSIGNED, ROUTE_CREATED/updated/cleared, PLAN_READY and PLAN_DEPLOYED. Existing `UOperationalEventStore` projects them with source, target, category and original Backend timestamp, deduplicated by domain sequence. Existing session capacity and Clear Display behavior remain intact. Native H shows the deployment event; reconnect automation verifies duplicate suppression and stale snapshot rejection.

## 16. Plan Validation

Backend validates plan ownership, assignment existence/uniqueness, route linkage, waypoint count, finite numbers, geographic ranges, contiguous sequence and nonnegative speed/wait. A ready route needs at least two waypoints. All missions must be ready and the plan explicitly validated before deployment. Missing-route rejection is demonstrated natively in F; malformed coordinates, stale writes and atomic failure paths are automated. A saved empty route returns Mission DRAFT with ROUTE_TOO_SHORT rather than retaining readiness.

## 17. Deployment

Command confirmation submits a business-state transaction. Backend revalidates, then atomically marks the plan and every mission DEPLOYED and appends PLAN_DEPLOYED. Failure leaves the complete state unchanged. No `/api/arrays`, assembly controller, PX4, MAVLink, takeoff, movement command or IN FLIGHT state is invoked by P3 deployment. In native F, the UI blocks NOT READY locally; the separate recorded HTTP negative probe verifies Backend 409 against that same native-created plan. It is not represented as a native network request.

## 18. Build

Backend CMake / MSVC Release build passed (`backend-build-precision.log`). UE 5.8 Development Editor build passed (`ue-build-delivery.log`, including the final handle/picking change and movement regression test). Binary hashes are in `binary-sha256.json`. New-code compile failures and subsequent successful logs are retained. Build success alone is not used as evidence of runtime acceptance.

## 19. Automation

Selected UE runs, including targeted follow-ups: **34 executions, 32 passed, 2 failed, 0 not run**. Failures are the retained Standalone height assertion and one transient Cesium tile ConnectionError in A2; the same A2 binary passed on retry. These totals include repeated test executions, not 34 unique tests. `automation-summary.json` records each run; individual JSON reports and filtered logs contain discovery and Test Completed records. Passed-with-warnings counts as executed pass, not warning-free. `P3.CrossClient.PlanSelection` is a per-process replica/version/event test; actual network consistency is established separately by protocol and native evidence.

## 20. Backend Tests

Full suite: **53 executed, 51 passed, 2 legacy planner failures** (`SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`). P3 Domain **6/6**: create, assignment/duplicate rejection, validation/atomic deployment, invalid coordinates/stale version, readiness invalidation/ownership, exact persistence restart. P3 real HTTP/WS **13/13**, using simulated protocol clients. P1 protocol **11/11**. No test standard was weakened.

## 21. Regression

UE coverage includes P1 version/role sync; P2 Command layout, Event Log, Command map selection, Video Follow, independent selection and Set Active Target; Command A2 map mode, AlertStore, StartupRole, M1 regression; Command/Map/Video lifecycle. Standalone CesiumWorld still fails only on the established 1082 vs 1080 height check. An initial M1 trial used the wrong sync-enabled fixture and hit its empty-alert assertion; rerunning the established P1DisableSync fixture passed without changing the test. Initial protocol float comparison and disk precision findings are preserved with corrected final results.

## 22. Physical Acceptance A–J

Three real UnrealEditor `-game` processes, one each Command / Map / Video, connected to the isolated real Backend. Screenshots are raw native captures, not composited or regenerated. Full A–J ran on `4028dab`; subsequent fixes have their own build, targeted automation and K native evidence, rather than claiming the full sequence was rerun on every commit.

| Step | Native result | Principal evidence |
|---|---|---|
| A | Command 3/3 ONLINE; Map operational; Video UAV-01 Follow OFF | A-command-3of3.jpg, A-processes.json, A-presence.json |
| B | Map created Perimeter Patrol A; Command synchronized DRAFT | B-map-draft.jpg, B-command-draft.jpg, B-backend.json |
| C | Added Mission-01 and Mission-02 | C-map-two-missions.jpg |
| D | Assigned UAV-01/02; duplicate UAV-01 rejected; mission selection synchronized independently of Global UAV | D-command-assigned-2of2.jpg, D-duplicate-uav-rejected.jpg, D-after-duplicate-rejection.json |
| E | Saved three waypoints for each first two missions; native Map restart restored selected route | E-mission01-three-waypoints.jpg, E-map-restarted-route-restored.jpg, E-restart-comparison.log |
| F | Mission-03 assigned UAV-03 with no route: NOT READY; Backend deploy rejected atomically | F-command-not-ready.jpg, F-backend-deploy-rejected.json |
| G | Mission-03 three-waypoint route saved; validated READY 3/3 | G-map-ready-3of3.jpg, G-command-ready.jpg, G-backend-ready.json |
| H | Native Command confirmation; plan and all three missions DEPLOYED; Event Log updated | H-command-confirmation.jpg, H-command-deployed-event.jpg, H-backend-deployed.json |
| I | Native Command and Map restart restored DEPLOYED domain and selections exactly | I-command-restarted-deployed.jpg, I-map-restarted-deployed.jpg, I-restart-comparison.log |
| J | Same Video process retained OFF/UAV-01 through B–I; ON followed Command's UAV-02 | J-video-after-B-I-follow-off.jpg, J-command-active-uav02.jpg, J-video-followed-uav02.jpg |

The accepted domain is `plan-11`, with `mission-12`, `mission-13`, `mission-16` and `route-15`, `route-14`, `route-17`. Supplemental tests use the separately created `P3 automated QA` plan; they do not rewrite the accepted deployed plan. K records final build startup/field restoration and route editing follow-ups. On final source `82a90e3`, Map validation changed the QA plan to READY and Command cleared stale NOT READY feedback. Selecting plan-11 / mission-16 synchronized both clients while Video retained UAV-01 / Follow OFF. Final native captures show Command 3/3 ONLINE. `K-deployed-plan-preserved.log` verifies the accepted deployed plan, all three missions and paths remain exactly equal to H. Map captures retain visible missing tile patches and an exposure warning; they are not evidence of complete basemap rendering.

## 23. GPU Observation

RTX 3060 Laptop GPU, 6144 MiB total. A–J snapshot: **5821 MiB**; later three-client snapshot: **5874 MiB**. A–J did not show a Video memory exhausted warning. During K, a transient red performance warning appeared partly behind the editor; it is not silently treated as a clean GPU run. No product features, business geometry or acceptance thresholds were reduced to save VRAM. Capacity/performance remains a separate deployment task.

## 24. Known Issues

Retain all five P2 items: Standalone 1082 vs 1080; two Backend planner legacy failures; low-altitude 3D route occlusion; XYZ map network dependency; 6GB VRAM capacity risk. G originally showed stale NOT READY feedback after the authoritative READY summary; the later refresh fix clears feedback on domain-version change. I originally showed blank edit text after late hydration; K verifies populated fields after restart.

One initial concurrent-start Video process failed before connecting with UE's `InheritedContext.cpp:130`, `RefCount.load(std::memory_order_relaxed)==0` assertion. An unchanged-binary isolated retry and subsequent serial starts succeeded. The crash XML/log are retained. Its root cause is not established and is not mislabeled as a proven historical issue or a proven P3 defect; startup stability remains an explicit qualification. Supplemental native axis dragging remains unverified because of the recorded tool-input limitation; its underlying move/save behavior is covered by the final UE test. The A2 tile-network failure and unchanged-binary successful retry are both retained.

## 25. Deferred

Real Jetson/PX4/MAVLink/flight and real camera/WebRTC acceptance; scheduling/optimization, geofence/weather, formation/AI/payload/recording; launcher/window placement, performance deployment, authentication/cloud. P3 deployment remains business state only. No P4 work, merge or push is included.

## 26. Evidence Index

See `Evidence/TASK-P3/README.md` for indexed screenshots, API snapshots, exact restart comparisons, test reports, build logs, process command lines, GPU observations and Git checks. SHA-256 screenshot manifest verifies the retained raw captures. Historical failing trials are kept and explicitly distinguished from selected final results.

## 27. Final HEAD

Base HEAD: `88b95a952348fcd9bcda328cb3415a56523de5c3`.
Branch: `feat/triple-screen-p3-security-plan`.
Final delivery HEAD is the documentation/evidence commit containing this report; resolve with `git rev-parse HEAD`. The final response supplies its literal hash after commit, avoiding a self-referential hash inside the commit. `git status` and protected branch checks are verified again after the final commit. Commit history is staged by Backend, UE integration, tests, runtime fixes and evidence/documentation.
