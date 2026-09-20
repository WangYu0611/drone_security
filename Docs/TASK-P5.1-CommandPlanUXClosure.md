# TASK-P5.1 — Command Center UX & Security Plan Workflow

## Final result

**CONDITIONAL PASS — ready for the user's Stage 1 UX experience review.** The native create → task → map route → pre-deployment check → deployment workflow completed in the default single-screen window layout, with Backend acknowledgement governing every business transition. Deployment does not initiate flight or execution. Retained conditions and evidence limits appear below. No Stage 2 work was added.

## Git

- Branch: `feat/triple-screen-p5-1-command-plan-ux`.
- Exact base: `53f63380cfccf1f42a1e524fd2716a6298039049`.
- Worktree: `C:\Users\wy331\Documents\UE5DroneControl-p5-1`.
- Final HEAD: the closure commit containing this report, resolved with `git rev-parse feat/triple-screen-p5-1-command-plan-ux`; its exact SHA is delivered in the final response. A commit cannot contain its own hash.
- Protected main/P1–P5 and other captured refs are compared in `Evidence/TASK-P5.1/closure`. The original P5 checkout is unchanged and clean.
- Local commit only. No push, merge, tag, reset of P5, or stash. Final clean status is checked after committing.

## Delivered behavior

Command has five primary pages: Overview, Security Plans, UAV Fleet, Alerts and Logs. The shared header retains Backend/role health and language. Overview summarizes the active plan, UAV/alert counts and recent events; plans occupy a dedicated workspace instead of sharing the tactical dashboard. New Plan is a centered modal with name and description. Creation atomically creates the default Task 01 in the existing store, acknowledges active Plan/Task and enters the task workspace directly. The UAV picker saves a real user choice through Backend; refresh-driven selection does not submit a mutation.

The workspace continuously shows the plan, state and highlighted step. Basic information, task configuration, route editing, pre-deployment check and deployment are distinct views. UI workflow metadata is persisted in the existing plan document and remains separate from DRAFT/READY/DEPLOYED business state. Backend request versions, content revisions and edit leases remain authoritative. Legacy clients do not acquire workflow metadata merely by beginning a legacy route edit.

Command sends the existing Map edit request with active Plan/Task, displays route editing and saved waypoint count, and offers Switch to Map / Retry. The native Windows focus helper acts only when exactly one visible Map window has the expected role title. Offline Map feedback explicitly says the saved draft is retained. Map shows Plan/Task/UAV, draft state, dirty/saved state and last-save time. The editor includes undo, point tools, speed, route statistics, Save draft and Finish. Save retains the edit lease; Finish saves dirty data, requires a valid saved route, closes the lease and advances Command to pre-deployment check only after acknowledgement.

Dirty exit/selection requests require Continue editing, Save and exit, or Discard. A rejected save keeps the local draft. A single retry is allowed only for a stale positive global version with unchanged plan content revision; actual content conflicts are not overwritten. Failed Finish-save clears its pending finish intent. Map restart reacquires a valid lease for an unfinished saved workflow; completed editing does not reopen. Unsaved local points are not represented as durable crash-recovery data.

Confirm and deploy internally performs validate → review → deploy with each response acknowledged. Incomplete configuration disables deployment. Deployed configuration is read-only; a new version creates a distinct draft without modifying the historical deployment. The result explicitly says configuration is active, the UAV has not taken off, and task execution has not started.

Friendly workflow events feed existing Command logs. Errors remain inline and shared health indicates disconnection; there is no parallel event or business store. Video retains its existing target/view responsibilities and receives no plan-edit or deploy UI. Assignment, active UAV and explicit video target remain separate.

## Validation

| Check | Result |
|---|---|
| Backend Release build | PASS |
| Backend GoogleTest | 64 discovered, 62 PASS, two historical failures only |
| Historical failures | `SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation` |
| P4 HTTP/WS | 21/21 PASS |
| P5 protocol/presence | 8/8 PASS |
| New P5.1 HTTP/WS workflow | 11/11 PASS |
| Launcher | 5/5 PASS |
| UE5.8 Development Editor build | PASS |
| UE automation | 5/5 PASS: Command 2, Map 2, Video 1; fresh per-role reports and successful completion lines verified |
| Native A–M | Completed; detailed ledger and original captures in Evidence |

Four new Backend cases cover default task/workflow state, save-versus-finish, deployed immutability/version copy and draft operations. The HTTP/WS suite covers the acknowledged end-to-end flow and replica/restart/language state. The existing UE draft guard assertion was updated only for the intentional new Save-keeps-editing contract; conflict, offline and unsaved-data checks remain. No historical planner failure was suppressed.

One final Map trial crashed inside `UCesiumGaussianSplatSubsystem::Tick` between PIE tests. It is retained as `map-cesium-crash-diagnostic.log`, is not counted as PASS, and exposed stale-report acceptance in the helper. The helper now requires exit code zero, a fresh report and the expected successful completion lines. Earlier repaired compile/test diagnostics are labelled separately. Fresh final UE results, not a prior report or process existence, are the acceptance source.

## Native and recovery evidence

See [native ledger](../Evidence/TASK-P5.1/native/acceptance.md). Default Command capture size was 1007 × 953; the actual desktop supplied less height than the illustrative 1000 × 1100 requirement. New Plan, task assignment, route request, check and deploy were all completed without maximizing. Map/Video captures are 1007 × 542. These are three independent windows, not a composed three-monitor image.

- Map stop during saved editing: Command reports offline and retains three saved points. Restart restores the same Plan/Task/UAV, route and editing mode.
- Command restart: same workspace, current route step, task and saved route restored. Map retains its additional unsaved point.
- Backend stop/restart: clients remain running, show reconnecting, and each returns ONLINE with `hydrated=true`. Context, preferences, video and complete plan snapshots compare equal.
- Whole-system shutdown/relaunch: all four domains compare equal and all three clients hydrate. Final shutdown checks the previously recorded owned PIDs and finds none live; the launcher ownership list is empty.

`record_acceptance.py` is a read-only snapshot oracle, not a UI substitute. Native registry fixtures are explicitly synthetic, loopback-only and source-free. No real UAV availability, camera/video, flight, terrain safety or operational deployment acceptance is claimed.

## Localization

New strings use the existing ProductText/FText en and zh-Hans catalog, generated PO/archive/manifest/locres. GatherText completed with commandlet exit 0; the process retained the inherited GameFeatureData startup-error exit behavior. Command → Chinese and Video → English were observed on all three roles, with active plan and route preserved. User-entered plan/task names intentionally remain unchanged when culture changes. Existing OS-formatted timestamps can retain Chinese formatting in the English UI.

## Retained conditions and scope limits

- Two historical Backend planner failures remain. Cesium imagery/network/cache dependency, low-height route occlusion, 1082 versus 1080, GameFeatureData/plugin diagnostics and 6 GB VRAM capacity risk remain inherited conditions. No business coordinates were altered to hide geometry.
- The Cesium PIE crash trial is disclosed above. Successful workflow tests do not prove the third-party plugin cannot crash under other editor/lifecycle loads.
- Command cards use content-dependent widths and step labels may wrap. The smaller tiled Map/Video shells retain small secondary text; Map statistics/waypoint details require scrolling. The core flow was usable at the recorded default size, but visual comfort and first-time discoverability remain for the user's requested experience confirmation.
- Saved route/context recovery was verified. A forcibly terminated process cannot recover local points that were never acknowledged as saved; no durable local autosave capability was introduced.
- Deployment remains business configuration only. PX4/MAVLink/Jetson, real cameras, RTSP/WebRTC, AI detection, real flight/execution, LAN and multi-PC are outside scope.

## Handoff

Launch `DroneSecurityLauncher.cmd` from this worktree for manual experience review. Saved native demonstration plans remain available. Stop here after local closure: await the user's UX acceptance before any push, merge or Stage 1 tag.
