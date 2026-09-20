# TASK-M1 — Independent Map Client

Implementation branch: `feat/triple-screen-map-v1`; baseline `f958fc9` (Video V2, already contains Command A3 `50c631d`). No merge to integration/main. TASK-A4 paused.

## Architecture

One UE5DroneControl project and codebase, one role per process. `UCommandScreenManager::ResolveClientRole` remains the shared startup contract. Enum Map is appended, preserving existing numeric role values. Explicit CLI overrides DefaultClientRole=Command; unknown values still resolve to Standalone.

| Role | Shell | Runtime ownership |
|---|---|---|
| Command | unique CommandShell | Fleet, selected Tactical Data, Alerts, mission status, pause/resume requests. No map service, source actors, scene rendering, path panel or map input bindings. |
| Map | unique MapShell | CesiumWorld, A2 default 2D camera/source lifecycle, scene receivers/shadows, map selection/highlight, route/waypoint display, planning, Confirm/Cancel, local playback, Geographic, Sequence, Speed. |
| Video | unique VideoShell | V2 monitor UI and Registry source selection; existing Video code unchanged. |
| Standalone | legacy combined CommandShell presentation | Existing legacy scene and tool startup retained. |

MapShell is a new small presentation class, not a copy/subclass of CommandShell. It hosts existing Sequence/Geographic/Speed widgets; all path editing, confirmation, dispatch conversion and local playback still use existing PC/panel/actor implementations. The native game viewport displays Cesium directly, with a compact header and toolbar, rather than a second render target.

Command constructs no central map host or map controls. Before game-world construction, the module World hook removes Cesium tilesets/georeference/sun-sky actors from that transient Command world. It does not modify the level asset or Editor preview world. GameMode skips coordinate initialization and receiver/shadow spawning, uses the native controller, and the controller enables UI-only input and disables world rendering. Thus the Command process may load the shared CesiumWorld package, but it has no active Cesium map sources or hidden map service. EndPlay restores viewport rendering state.

`UDroneMissionService` is a GameInstance subsystem containing the existing pause/resume availability and request policy, with no map/camera/world-actor dependency. Map's retained adapter delegates to it. Command alert selection writes its local Registry directly; Map alert adapter retains local focus behavior. Registry, NetworkManager, AlertStore, descriptors, telemetry, mission/path data and role resolver remain shared code. Every process owns its own GameInstance/subsystems and state.

**Cross-client PrimarySelection synchronization = DEFERRED.** Command and Map each select independently. No static global variable, shared-memory mechanism or hidden single-process sharing was added. Future A4: Backend / IPC / WebSocket event, Command selection → Map focus → Video focus.

## Modified files

- `Map/MapShellWidget.h/.cpp`: independent map-only shell, compact UAV selector, toolbar, retained drawer adapters and map hit testing; a non-interactive cyan selection frame projects the locally selected sender pawn each frame.
- `Shared/DroneMissionService.h/.cpp`: per-process mission request policy extracted from map adapter.
- `Command/CommandScreenManager.h/.cpp`: shared Map role parsing, unique shell lifetime and map-tool guards.
- `Command/CommandShellWidget.cpp`: conditional construction keeps Command strictly command-only, Standalone combined layout retained.
- `Command/CommandAlarmPanel.cpp`: local Registry selection without map dependency.
- `Command/CommandMapInteractionService.cpp`: Map camera ownership and shared mission-service delegation; source policy and business coordinates retained.
- `UE5DroneControl.cpp`, `DroneOps/Control/DroneOpsGameMode.cpp`, `DroneOpsPlayerController.cpp`, `UI/UIManagerBlueprintLibrary.cpp`: World, scene, input and compatibility-facade role boundaries.
- `Tests/CommandStartupTest.cpp`, `CommandMapModeTest.cpp`, `VideoClientTest.cpp`, `MapExtractionTest.cpp`, `MapVisualQA.cpp`: startup precedence, retained A2 behavior through MapShell, role lifecycle, Command isolation and explicit local visual fixture.

All source paths above are under `Source/UE5DroneControl`.

## Startup

Use the same `.uproject` with UE5.8:

```text
UnrealEditor.exe <project>.uproject -game -ClientRole=Map -windowed -ResX=1920 -ResY=1080
UnrealEditor.exe <project>.uproject -game -ClientRole=Command -windowed -ResX=1920 -ResY=1080
UnrealEditor.exe <project>.uproject -game -ClientRole=Video -windowed -ResX=1920 -ResY=1080
UnrealEditor.exe <project>.uproject -game -ClientRole=Standalone -windowed -ResX=1920 -ResY=1080
```

Run each independently for M1 acceptance. No three-process launcher or window placement is included. No role argument continues to default to Command.

## Automation / physical verification

| Process role / suite | Discovered and executed | Success | Fail | NotRun |
|---|---:|---:|---:|---:|
| Command: StartupRole, AlertStore, M1.CommandRegression, RoleLifecycle | 4 | 4 | 0 | 0 |
| Map: StartupRole, A2MapMode, RoleLifecycle | 3 | 3 | 0 | 0 |
| Video: StartupRole, V2 RoleLifecycle | 2 | 2 | 0 | 0 |
| Standalone: RoleLifecycle | 1 | 1 | 0 | 0 |
| Standalone: unchanged legacy Command.CesiumWorld combined regression | 1 | 0 | 1 known | 0 |

Every reported result has an explicit `Test Completed` log entry; exit code 0 alone is not treated as success. Success totals include succeededWithWarnings. The legacy combined suite is run with Standalone because Command intentionally no longer owns a map. Its source is unchanged and **its only error is the original expected 1080 / actual 1082 assertion**. No functional assertion failed. The existing A2MapMode source only changes the shell include/getter/startup diagnostic from CommandShell to MapShell; behavioral assertions remain intact.

Command regression asserts real runtime absence: map service=null, no MapShell/VideoShell, unique CommandShell after repeated Initialize/Show, zero tilesets/georeferences, world rendering disabled, no map panel creation through legacy facade/direct entry points, and no map hit surface or toolbar/planning widgets. It also checks list selection, Tactical Data, task data, alert event projection and the offline pause/resume guard.

Video V2 regression retains its aircraft/source selection, layout/slot lifecycle and unique browser checks. No Video implementation file is modified. Standalone keeps the combined shell and the complete old functional test passes apart from the retained viewport-height failure.

Reproduce automation in separate UE5.8 Editor processes using `-ClientRole=<role> -unattended -ExecCmds="Automation RunTests <suite names joined by +>" -TestExit="Automation Test Queue Empty" -ReportExportPath=<directory>`. Raw reports are under `Saved/MapQA/{command,map,video,standalone,known-height}`. Final Map highlight recheck (`Saved/MapQA/map-final`) discovered/executed 3, Success 3, Failed 0, NotRun 0; all three have explicit successful Test Completed entries.

### Physical mouse/window verification

Real `-game` OS windows, target 1920×1080, one role at a time. Captures show logical window dimensions at current Windows scaling, not a changed business viewport target. `-unattended` is used at launch to handle the existing SimpleWebSocket engine-version warning without changing the plugin. This is an explicit-role game-window acceptance, not a new zero-config Explorer/PIE claim.

| Check | Observed result / evidence |
|---|---|
| CesiumWorld and default 2D | PASS: satellite imagery, map-first MapShell, no Command/Video shell; `01-map-default.jpg`. |
| 2D → 3D → 2D | PASS: real mouse buttons, top-down/oblique camera and terrain/building layers; `02-map-3d.jpg` and subsequent 2D captures. Logs show original sources restored in 3D and ellipsoid carrier/heavy-source suspension restored in 2D. |
| Map UAV click / local primary selection | PASS: clicked UAV-2 on map, selector and local focus updated; `03-map-selected.jpg`. |
| Multiple waypoints / path | PASS: three physical clicks appended waypoints 02/03/04 to the origin, ordered labels and multi-segment route visible; `04-map-waypoints.jpg`. |
| Confirm | PASS: existing three-choice popup; real Backend dispatch disabled in this local fixture; `05-confirm.jpg`. |
| Local Playback | PASS: chose local preview, observed actor movement over successive captures and increasing segment time; `06-playback.jpg`–`10-speed-applied.jpg`. Red conflict-priority route color is retained. |
| Geographic | PASS for real opening and replacement by another tool, retained compact scrollable panel; `07-geographic.jpg`. Coordinate conversion/waypoint behavior is covered by retained automation. |
| Sequence | PASS for opening/closing and drawer replacement; `08-sequence.jpg`. Existing path-file list is empty; loading/dispatching a real saved sequence was not physically exercised. |
| Speed | PASS: expanded input, typed 2.0, clicked Apply and observed applied value; existing waypoint labels stayed at 1 m/s; `09-speed.jpg`, `10-speed-applied.jpg`. |
| Cancel | PASS: physical Cancel removed the temporary path and waypoint visuals; `11-cancel.jpg`. |
| Command startup / no map | PASS: full Command-only page; `12-command-startup.jpg`. Runtime source removal and no-map-service assertions separately passed. |
| Command Fleet / selection / Tactical Data | PASS: clicked UAV-1, then alert selected UAV-2; primary row styling and battery/mission projection updated; `13-command-selected.jpg`, `14-command-alert-selection.jpg`. |
| Command Alerts | PASS: target selection and mark-handled reduced unhandled count to zero; `15-command-alert-handled.jpg`. |
| Video V2 startup / selection / layout | PASS: unique VideoShell, clicked UAV-2 and switched 6-view to 4-view; `18-video-v2.jpg`, `19-video-selection.jpg`, `20-video-four-view.jpg`. NO SOURCE remained explicit; actual media playback is not claimed. |
| Command Mission controls | PASS for presence/data and offline disabled state. Real pause/resume transport is not claimed; production Backend is deferred. |

The M1.QA fixture is explicit, process-local and disables Backend sends. Existing cached scene/list labels can still display their prior QA names while Registry/Tactical Data show M1 QA names; none are real UAV data. At wide map zoom the inherited mesh glow was difficult to distinguish, so a Map-only cyan selection frame was added. **Final physical highlight PASS**: actual UAV click displays the frame (`16-map-highlight.jpg`), and a second physical planning/Confirm/local-preview run shows it following the moving pawn (`17-highlight-playback.jpg`). It uses Registry.GetSenderPawn and world-to-widget projection, is hit-test invisible, and draws beneath tool drawers. No path/waypoint coordinate, altitude, mesh transform or primary-selection logic changed.


Build: UE5.8 Editor Development succeeded, initial validated build 7/7 actions, 34.98 seconds (`Saved/MapQA/build.log`); final highlight build 4/4 actions, 16.87 seconds (`Saved/MapQA/final-highlight-build.log`). Initial direct Chinese-path build failed in compiler path decoding; existing `C:/Users/wy331/Documents/UE5DroneControl-command` junction points to this same project and is used for builds/runs. This is not another project or checkout. Two local C++ compile errors were corrected before the successful build.

Raw local evidence: `Saved/MapQA`. Only reviewed, sanitized summaries/screenshots are committed; raw network logs may contain credentials.

## Known issues and deferred

- Existing exact **1082 vs 1080** viewport-height assertion remains unchanged; no business test or coordinate change to obtain a green result.
- Low-altitude route occlusion in 3D remains Known Issue / Deferred. The actual 3D window also displayed the inherited Lumen exposure warning and white building surfaces. Satellite imagery loaded, but global imagery/render-quality perfection is not claimed. The Map selection frame is presentation-only and does not change business coordinates.
- Cross-client selection sync, simultaneous three-client startup, automatic placement, second Video host, production Backend, real UAV, Jetson/PX4, AI detection, true multi-stream and TASK-A4 are deferred.
- Local QA descriptors/telemetry/alerts are labelled fixtures. `M1.QA` is an explicit development console command, never automatic and never sends Backend mission data.

## Commit / branch / disposition

Branch: `feat/triple-screen-map-v1`.
Implementation commit: `f06a17dee72969707e5900247fa83282ab74544a`. Acceptance documentation and reviewed captures are recorded in the following documentation commit.
Frozen baselines remain Command A3 `50c631de9a55f9d1cd25e9f6520d11cf6fa888ec` and Video V2 `f958fc9adaf994b40448a6881fa23bfbaf3e0ebd`; no integration/main merge.
Status: **CONDITIONAL PASS** for the scoped local M1 extraction. Role startup, Map physical interactions and Command/Video local regressions passed. The unchanged 1082/1080 assertion remains a known failure; saved-sequence loading and live transport/media are not physically accepted by this run. Deferred scope remains deferred.
