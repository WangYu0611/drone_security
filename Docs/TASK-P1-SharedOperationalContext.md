# TASK-P1 — Shared Operational Context & Cross-Client Sync

## Baseline and scope

Branch: `feat/triple-screen-p1-shared-context`, created from clean M1 acceptance HEAD `6a9dec83feaf3fb4faa9ce35c403670c0a2dfba0`, following implementation `f06a17dee72969707e5900247fa83282ab74544a`. No merge to M1, integration or main.

Backend is business authority. Command is the main operator console; Map and Video may change Active UAV. Three independent UE processes retain their own GameInstance, Registry and shell. All cross-process communication goes through Backend HTTP/WebSocket; no client-to-client transport or static shared state.

## Context model and conflict policy

`Backend/storage/operational_context.h` owns one mutex-protected document:

| Field | Representation |
|---|---|
| context_version | Monotonic integer; starts at 0; increments once per effective atomic patch |
| active_uav_id | null or canonical `UAV-01`, `UAV-03`, etc.; maps to existing integer Registry IDs |
| active_alert_id | null or event's stable Backend Alert ID |
| active_mission_id | null or Mission ID |
| active_security_plan_id | null or Plan ID |
| active_area_id | null or Area ID |
| operation_mode | MONITOR / PLAN_EDIT / MISSION_EXECUTION / ALERT_RESPONSE |
| updated_at | Unix UTC seconds |
| updated_by | Registered source client ID |

Requests are validated before mutation. Invalid fields/modes/IDs reject the entire patch. No-op requests do not increment the version or emit an event. Concurrent requests are serialized; the last request accepted by Backend wins. Alert selection updates alert, UAV and ALERT_RESPONSE in one version. Mission/Plan/Area APIs carry IDs only; no route/UAV assignment is invented.

The context is persisted next to `config.storage_path` as `operational_context.json` before publishing, using a flushed temporary file and atomic replacement. Restart retains the version and selections. Corrupt/unreadable persistence fails visibly; operators must not delete/reset it while running clients retain higher versions.

## Backend API

| Method/path | Contract |
|---|---|
| GET /api/context | Complete context snapshot |
| POST /api/context/clients | Register client_id, client_role, instance_id, hostname, app_version |
| GET /api/context/clients | Client identities and subscription ONLINE/OFFLINE state |
| PATCH /api/context | `{ "instance_id": "...", "patch": { "active_uav_id": "UAV-03" } }` |
| POST /api/context/debug/alert | Debug-enabled Backend only; `{ "drone_id": 3 }` publishes a labelled QA link-loss alert |

Roles: Command/Map/Video. Registration alone is OFFLINE. An accepted WebSocket subscription marks ONLINE; socket teardown marks OFFLINE. A live instance cannot be rebound to a different role or a second socket. Map and Video can patch only `active_uav_id`; other context fields require Command. These are Phase-1 role checks on the existing trusted Backend network, not a new authentication system.

## WebSocket messages

Client sends `{"type":"subscribe_context","instance_id":"..."}` after registration and HTTP snapshot. Backend sends `context_subscribed` with the latest full `payload` and `clients` array. This second snapshot closes the HTTP-snapshot-to-subscription race.

Effective updates emit:

```json
{
  "type": "OperationalContextChanged",
  "event_type": "OperationalContextChanged",
  "event_id": "context-1024",
  "version": 1024,
  "timestamp": 0,
  "source_client": "CLIENT-COMMAND-...",
  "changes": ["active_uav_changed"],
  "payload": { "context_version": 1024, "active_uav_id": "UAV-03" }
}
```

The example payload is abbreviated; real messages always contain every context field. `changes` supports active_uav_changed, active_alert_changed, active_mission_changed, active_security_plan_changed, active_area_changed and operation_mode_changed. One aggregate event keeps multi-field Alert transitions atomic. `client_connected` / `client_disconnected` carry the affected identity independently of context versions. Client-state snapshot on every subscription repairs missed presence events. Each identity carries a separate monotonic `client_version`; UE merges snapshot/events by that version and clears the previous server connection's presence cache on disconnect.

`context_ping` / `context_pong` provide liveness. Standard Backend alert messages now include `alert_id`; the shared socket populates CommandAlertStore using this ID. Existing non-P1/Standalone alert projection remains available.

## UE integration and lifecycle

`Shared/OperationalContextSubsystem.h/.cpp`: one UGameInstanceSubsystem per process. It resolves the existing role, registers an instance identity, obtains/applies the HTTP snapshot, opens WS, subscribes, then marks CONNECTED. Transport logic remains outside the shells. It owns the local JSON context model, named field delegates, full-context delegate, client presence replicas, version rejection, reconnect and HTTP updates.

The Registry's existing selection entry points route local intent to the subsystem before changing primary state. REMOTE_CONTEXT_APPLY uses a scoped guard to update existing Registry selection without echoing to Backend. Command Fleet, Map hit-test/selector and Video source/slot buttons therefore retain their existing UI paths. Offline requests leave the authoritative selection unchanged. Late descriptors are projected after arrival. Video's automatic first-source fallback is disabled during global sync so it cannot overwrite a restored selection.

Command Alert action submits the stable Alert ID and associated UAV atomically. Map retains its existing selection highlight and deferred focus. Video retains its V2 layout, slots, retry, empty state and source projection. Process-local multi-selection sets are committed only after the matching Backend primary acknowledgement; a request serial prevents old replies from undoing newer local actions. Mission/Plan/Area/Mode have public context update/notification APIs; full editors and mission-to-route/source mappings remain deferred.

Minimal state feedback uses existing Command overview, Map status and Video header. It displays Backend ONLINE/OFFLINE, sync state, Active UAV and version. No P5 header/launcher redesign is included.

Configuration:

```ini
[OperationalContext]
; Otherwise inherits /Script/UE5DroneControl.DroneNetworkManager URLs.
BackendBaseUrl=http://127.0.0.1:8080
WebSocketUrl=ws://127.0.0.1:8081/ws
FollowSelection=true
FollowGlobalSelection=true
```

FollowSelection controls Map camera focus; highlight remains selection-driven. FollowGlobalSelection applies to Video and defaults true; false permits local source browsing. Standalone keeps legacy local behavior. `-P1Http=...` / `-P1Ws=...` override only shared-context endpoints for isolated QA. `-P1DisableSync` is an explicit legacy regression switch, not the production default.

Disconnected clients retain the last context and UI. Heartbeat timeout is 12 seconds, ping interval 3 seconds, retry interval 2 seconds, HTTP timeout 5 seconds. Every reconnect repeats registration and HTTP snapshot before opening a fresh socket. Generation guards ignore callbacks from prior attempts or teardown. Client applies only strictly newer, structurally valid context versions. No credentials or endpoint queries are added to P1 logs.

## Validation status

P1 core business acceptance: **PASS**. Overall disposition: **CONDITIONAL PASS**, retaining the existing viewport and Backend planner known failures below.

- UE5.8 Development Editor build succeeded. A first discovery run found zero P1 tests because the build makefile omitted the new source; it is explicitly not a pass. `-NoUBTMakefiles` rebuilt with OperationalContextTest.cpp included.
- Backend Release build succeeded.
- Backend unit suite: 47 executed, 45 passed, 2 pre-existing failures. All 3 P1BackendContext tests passed: atomic/no-op/validation, concurrent monotonic publishing and persistence after restart.
- Existing failures: SegmentDistanceTest.Crossing and AssemblyPlannerTest.ConflictHeightSeparation. The same failures exist in the prior Saved/CommandQA/backend-tests.log; P1 does not modify those tests or planner implementation.
- Real Backend HTTP/WS protocol test: Command/Map/Video selection, atomic Alert, validation/role policy, Mission/Plan/Area/Mode, late join/restart, client state and no-event-loop checks passed. Most recent local measured protocol selection propagation: Command 5 ms, Map 4 ms, Video 4 ms. Same-instance disconnect/reconnect with a fresh snapshot and newer presence version also passed. These are protocol-client measurements, not UE UI latency claims.

| UE run | Discovered/executed | Success incl. warnings | Failed | NotRun |
|---|---:|---:|---:|---:|
| P1 Command VersionAndRoleSync | 1 | 1 | 0 | 0 |
| P1 Map VersionAndRoleSync | 1 | 1 | 0 | 0 |
| P1 Video VersionAndRoleSync | 1 | 1 | 0 | 0 |
| P1 Map MultiSelectionAcks, live Backend | 1 | 1 | 0 | 0 |
| M1 Command: StartupRole, AlertStore, CommandRegression, RoleLifecycle | 4 | 4 | 0 | 0 |
| M1 Map: StartupRole, A2MapMode, RoleLifecycle | 3 | 3 | 0 | 0 |
| V2 Video: StartupRole, RoleLifecycle | 2 | 2 | 0 | 0 |
| Standalone: RoleLifecycle, legacy CesiumWorld | 2 | 1 | 1 known | 0 |

Every listed test has an explicit `Test Completed` record. The Standalone suite's only assertion error is `Expected 'viewport height' to be 1080, but it was 1082.` Zero discovered tests and exit code alone are never counted as success. Legacy regression suites use `-P1DisableSync` to exercise their original process-local selection contract; P1 per-role tests run with sync enabled and an unavailable dedicated endpoint to validate offline behavior.

A Map regression run exposed a stale raw-this callback in the old DroneWebSocketClient during consecutive PIE worlds. The old socket's late error callback entered a destroyed delegate and attempted an invalid allocation. Socket callbacks/reconnect timers now use weak UObject bindings, and old callbacks are cleared before replacement/close. The same complete Map sequence then passed 3/3, with no crash. The Backend WsSession pointer teardown is also synchronized with broadcast writers.

Final physical binary was built after the client-presence merge fix. A subsequent focused selection-ACK fix preserves local multi-selection removal while awaiting Backend authority. The final UE build succeeded in 8.22 seconds; the live Backend MultiSelectionAcks test passed with explicit Test Completed, verifying primary removal and authoritative clear. Its first attempt exposed a test timer started before world setup; moving timer initialization to the first latent update corrected the harness. It changes no map geometry, camera, transport protocol or role ownership. Total UE acceptance: 15 executed, 14 successful, one retained viewport failure. See [ACK event evidence](Evidence/TASK-P1/multi-selection-ack.txt).

Run `DroneOps.P1.VersionAndRoleSync` separately with isolated unavailable P1 endpoints for offline validation. Run `DroneOps.P1.MultiSelectionAcks` with a dedicated live Backend and the Map role; do not combine both under one endpoint configuration.


Raw local logs/reports: `Saved/P1QA`. Reviewable automation source: `Backend/tests/test_operational_context.cpp`, `Backend/tests/p1_context_integration.mjs`, `Source/UE5DroneControl/Tests/OperationalContextTest.cpp`.

## Physical acceptance

All A–F scenarios passed with three independent UE5.8 `-game` OS processes. Command PID 19940 and Video PID 19464 remained alive across the Backend interruption. Map PID 17548 was closed for E; new Map PID 10936 recovered the current snapshot. The dedicated compiled Backend ran on local ports 18080/18081, with its own `Saved/P1QA/backend.yaml`, registry and persistent context.

The existing explicit `M1.QA` development fixture supplies labelled UAVs, local telemetry and a sample local alert. It isolates ordinary mission/telemetry sends; the P1 shared-context channel is real and remains connected to the dedicated Backend. No real UAV, camera or video frames are claimed. Cached scene/list labels retain `QA UAV-*` while Tactical Data has `M1 QA UAV *`, as already recorded in M1.

| Scenario | Observed outcome | Screenshot group |
|---|---|---|
| Initial online | All three identities ONLINE; all screens UAV-01 / v19 | 01-three-clients-online-* |
| A: Command Fleet click UAV-02 | All three UAV-02 / v20, without refresh | 02-command-select-uav-* |
| B: map model click UAV-03 | All three UAV-03 / v21; Map cyan highlight and focus | 03-map-select-uav-* |
| C: Video aircraft click UAV-01 | All three UAV-01 / v22; Video primary/source highlighted | 04-video-select-uav-* |
| D: Backend generates Critical UAV-03 alert, Command clicks | Atomic ActiveAlert=`ALERT-QA-1789389511.555210`, ActiveUAV=UAV-03, mode=ALERT_RESPONSE / v23; Map/Video selected | 05-alert-sync-* |
| E: close Map, Command selects UAV-02, launch new Map | New instance's first context apply is UAV-02 / v24; no default UAV-01 reset | 06-map-restart-restore-* |
| F: stop and restart dedicated Backend | All three stay alive and display Offline; offline Command click UAV-01 does not replace UAV-02; all automatically reconnect ONLINE with persistent UAV-02 / v24 | 07-backend-reconnect-* |

Same-host UTC client logs show selection request-to-Registry-apply latencies of **8–51 ms**, all below the 5-second requirement. This is application-log timing, not a pixel-level render benchmark. Per-client measurements and exact timestamps are in `selection-timing.json`; screenshots confirm the resulting UI projections.

One initial B click hit ground beside the label and invoked the existing isolated local-move behavior; it is not counted as a selection pass. The test then zoomed and confirmed hover on the UAV-03 model before clicking; the actual Map-originated context update and cross-screen result passed. No coordinate, viewport business logic or hit-test code was modified to mask this. The Map restart returns to its normal fixture arrangement.

24 reviewed native window captures cover the seven groups, including all three Offline and restored screens. Captures are at the host's display scaling; launch resolution remains 1920x1080. See [evidence index](Evidence/TASK-P1/README.md), [automation counts](Evidence/TASK-P1/automation-summary.json), [protocol results](Evidence/TASK-P1/protocol-results.txt) and [sanitized P1 event log](Evidence/TASK-P1/p1-events.txt). Test windows and the dedicated Backend are stopped after acceptance; the pre-physical Registry cache is restored.


## Known issues and deferred

- Expected 1080 / Actual 1082 remains a known viewport assertion failure; no coordinate or business-viewport workaround.
- Existing low-altitude 3D route occlusion remains Known Issue / Deferred.
- Existing Backend planner failures listed above remain out of scope.
- Command Center V2, full Security Plan/Area/Route editor, assignment/Deploy, actual multi-stream video/AI/payload/recording, Launcher/window placement/unified header and real PX4/Jetson/UAV/camera acceptance remain deferred.
- M1's existing saved-sequence/media/real-transport boundaries remain in force. P1 does not convert local fixture tests into real-flight acceptance.

## TASK-P1 FINAL ACCEPTANCE

| Check | Result |
|---|---|
| Build: UE and Backend | PASS |
| Command / Map / Video | PASS |
| Cross-client ActiveUAV | PASS |
| Alert Sync | PASS |
| Late Join | PASS |
| Reconnect | PASS |
| M1 Regression | FAIL: retained Standalone 1082/1080 assertion; other listed regressions PASS |
| Final | CONDITIONAL PASS |

All P1-specific automated and physical scenarios passed. Overall acceptance retains the existing viewport and two Backend planner failures, and the documented low-altitude 3D occlusion. P2–P5 and real-device/media acceptance remain deferred. Changes are committed only on the designated P1 branch; no merge or push is part of this delivery.
