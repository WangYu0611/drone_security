# TASK-P3 evidence

Read with `Docs/TASK-P3-SecurityPlanMissionDeployment.md`. All paths below are relative to this folder. Backend fixture ports are HTTP 19180 / WS 19181 and loopback UDP only. Synthetic UAV descriptors/telemetry are visibly labeled. No real flight or camera acceptance is claimed.

## Native A–J

A–J was performed with actual UE 5.8 Command, Map and Video game processes on source `4028dab`. These are direct captures of each native window. They were not generated, composited, relabeled or redrawn. Capture dimensions reflect desktop DPI and window capture, not a claim that the historical 1082-vs-1080 viewport issue is fixed.

| Prefix | Evidence |
|---|---|
| A | Command 3/3, baseline Map, Video UAV-01 Follow OFF; process arguments and presence |
| B | Native Perimeter Patrol A creation, Map/Command DRAFT, Backend snapshot |
| C | Two missions in Map |
| D | UAV-01/02 assignment, Command mission selection, native duplicate-assignment rejection and unchanged state |
| E | Three waypoints per first two missions; exact state equality across native Map restart |
| F | Third mission missing route, NOT READY; separate HTTP negative deployment probe against the same native plan |
| G | Third route saved and all three missions validated READY |
| H | Native Command confirmation, DEPLOYED Event Log/Map, authoritative plan/all-missions deployment snapshot; 3D occlusion capture |
| I | Native Command/Map process restart; exact domain/context recovery |
| J | Same Video process retained Local Source with Follow OFF through B–I; Follow ON then tracked Command UAV-02 |
| K | Later-build supplemental field restoration, route clear/delete, visible scaled handles, attempted drag diagnostics and final process/state observations; not a replacement for A–J |

Accepted native domain: plan-11; Mission-01/02/03 are mission-12/13/16; their routes are route-15/14/17. The separate P3 automated QA plans were generated during this task. `K-final-domain.json` can contain later QA edits; `H-backend-deployed.json` is the immutable A–J deployment snapshot.

## Automated and protocol layers

- `backend-tests.log/xml`: full Backend suite, 53 executed / 51 passed / two retained planner failures.
- `backend-domain-final.log`: six P3 domain tests, all passed.
- `p3-protocol-final.log`, `protocol-results.json`, `protocol-state.json`, `protocol-context.json`: thirteen real HTTP/WS tests using simulated protocol clients. They are not native UE A–J.
- `p1-protocol.log`: eleven P1 protocol regressions passed.
- `before-backend-restart.json`, `after-backend-restart-precise.json`, `backend-restart.log`: exact persisted-domain restart recovery, including deployed state and double timestamps.
- `after-backend-restart.json`: earlier imprecise parser result, retained to explain the precision fix. `p3-protocol.log` similarly records an earlier strict float-comparison trial; use the explicitly named final result.
- `automation-summary.json`: selected UE test executions by run, with pass/fail/not-run counts. `automation-<run>.json` contains the actual report; logs retain discovery and Test Completed records. Repeated runs are counted as executions, not unique test names.
- UE P3 replica tests exercise snapshot version gating and event projection in one process. Network and physical consistency are separately evidenced above.
- Standalone regression preserves its 1082-vs-1080 failure. M1 requires its existing P1DisableSync fixture; no test was weakened to obtain a pass. `gizmo-hit-final` also retains one A2 failure from a Cesium tile ConnectionError; `gizmo-network-retry` passed with the same binary.

## Build, process and GPU provenance

- `backend-build-precision.log` is the passing Backend build used for native acceptance.
- `ue-build-ui-final.log`: source 4028dab used for full A–J.
- `ue-build-reconnect.log`: source 7d39d92, late-hydration and obsolete-feedback fixes; `reconnect-final` automation and K restored-input screenshot.
- `ue-build-gizmo.log` / `ue-build-gizmo-hit.log`: handle scale and 2D picking builds. `ue-build-delivery.log` is the final build with the direct waypoint-axis/save regression. `automation-delivery-route.json` verifies movement data. `K-drag-tool-diagnostic.log` records the native tool limitation: mouse-down was observed at the drag destination, so native dragging is not claimed as passed. Temporary diagnostic logging was removed before the final build.
- Other build logs include earlier corrected compiler errors. Preserve chronology; do not interpret the mere existence of a build log as success.
- `binary-sha256.json`: final retained binary digests (not a digest claim about earlier screenshots).
- `A-processes.json`, `K-final-processes.json`: actual PID, creation time and command arguments. `A-presence.json` / `K-final-presence.json` also retain offline historical instances; filter ONLINE to count current clients.
- `physical-*.log`: native runtime logs for the named stages. A–J Video is `physical-video-A-J.log` and remained the same process through B–I.
- `video-startup-incomplete.log`, `video-startup-crash.xml`: one initial UE startup assertion before Video connected. An unchanged build succeeded on retry. Root cause is unproven; this is disclosed as a startup-stability qualification.
- `gpu-three-clients.csv` and `gpu-final-three-clients.csv`: live nvidia-smi observations on a 6144 MiB RTX 3060 Laptop GPU. They do not establish a safe VRAM budget under all content loads.

## Capture and log integrity

`screenshots-sha256.json` hashes the raw image bytes. Shared text logs have only Cesium URL access tokens replaced with `[REDACTED]`; the affected filenames are listed in `sanitized-log-files.json`. Original runtime logs remain locally in ignored `Saved/P3QA`. Screenshots are untouched. This redaction does not remove test results, error stacks, timestamps or process arguments.

`launch-game.ps1` and `run-automation.ps1` document the isolated launch/automation invocation. They refer to this worktree and its local runtime fixture; they are evidence helpers, not a production launcher. Source changes and test implementations are committed separately from this evidence.


Final source: `82a90e3`. `K-delivery-ready-feedback-cleared.jpg` verifies remote validation clears obsolete Command feedback; `K-delivery-command-deployed.jpg`, `K-delivery-map-deployed.jpg` and `K-delivery-video-independent.jpg` show restored plan-11 / mission-16 and independent Video. `K-deployed-plan-preserved.log` compares all accepted deployed objects with H. Map still shows tile gaps and an exposure warning. `physical-*-delivery.log` and refreshed K snapshots identify these final processes. `verify-git.ps1 -RequireClean` verifies the delivery branch, base ancestry, protected refs, no merges and clean workspace.
