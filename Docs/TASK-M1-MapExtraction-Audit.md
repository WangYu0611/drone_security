# TASK-M1 — Map Extraction Audit

Read-only audit completed before implementation, 2026-09-14. TASK-A4 is paused.

## Git baseline

Clean checkout at `f958fc9` (`feat/triple-screen-video-v2-ui`). The graph is linear from Command A3 `50c631d` through Video V1 and V2 to this commit. Both frozen nodes are ancestors of the new `feat/triple-screen-map-v1` branch. Use `f958fc9` directly; no integration/main merge, project copy or additional repository. Frozen refs must remain unchanged.

## Existing ownership and coupling

| Component / feature | Existing coupling | M1 owner |
|---|---|---|
| CommandShell Header, Fleet/DroneList, Overview, AlarmPanel, Tasks | Registry projections and AlertStore | Command |
| SelectedInfo, SelectedMetrics, SelectedMission / Tactical Data | Registry telemetry/task data, currently inside central MissionPanel | Command |
| CenterHost | Transparent overlay over the real CesiumWorld game viewport; not a second SceneCapture | Map; Standalone keeps legacy presentation |
| MapToolbar, Map2DButton, Map3DButton, Locate | Shell callbacks into CommandMapInteractionService | Map |
| MissionPanel / MissionActions drawer | Contains focus, plan, delete waypoint, confirm, cancel, pause/resume, Sequence, Geographic, save, stop playback, Speed | Split presentation: map tools to Map; pause/resume to shared business service consumed by Command |
| PathEditor / DronePathActor / DroneWaypointActor | PC owns edit state, waypoint input, actor selection; path actor owns spline and waypoint display | Shared implementation; Map runtime presentation |
| Confirm / Cancel | Existing SequenceDispatchPanel adapters call PC workflow and PreviewConfirmPopup | Map; retain implementation and dispatch/local-preview choice |
| Local Playback | Existing Sequence/PC/path playback state and actor transforms | Shared data/workflow; Map visualization |
| Geographic | UIManager → ScreenManager → retained GeographicTargetPanel; uses coordinate service and PC | Map |
| Sequence | Retained SequenceDispatchPanel, one instance per controller | Map |
| Speed | DefaultSegmentSpeedWidget uses PC default edit speed; new waypoints consume it | Map |
| Drone markers and highlight | GameMode spawns receivers/shadows from Registry; PC applies selection highlighting | Map |
| PrimarySelection | Registry GameInstance subsystem, shell delegates and PC selection projection | Shared code, independent per-process state |
| Cesium initialization | Module OnPostWorldInitialization → world-owned CommandMapInteractionService; GameMode PreInitializeComponents reuses it; PC initializes camera | Map only |
| Cesium source lifecycle | Early service caches sources/raster activation, starts ellipsoid 2D carrier, defers heavy sources, restores original sources on 3D; GameMode reapplies pending mode | Map; retain A2 lifecycle |
| A2MapMode | Existing automation assumes Command has map service and shell; checks picking, selection, paths, playback, drawers and roundtrip | Move startup expectation to Map; retain behavioral assertions |
| GameMode | Coordinate initialization, tile config, registry receiver/shadow spawn and tick | Map/Standalone; Command uses data subsystems without scene spawn |
| Controller | Legacy HUD, cameras, input bindings, tools; Video already has UI-only early startup | Command gets UI-only startup; Map retains existing interaction implementation |
| UIManager compatibility facade | Manager lookup currently requires CommandShell; fallback can create legacy map panels | Accept MapShell and explicitly reject map panels in Command/Video |
| Registry, NetworkManager, AlertStore, descriptors, telemetry, mission/path data, role resolver | Shared module / GameInstance subsystem code | Shared, never cross-process shared memory |

## Implementation strategy

1. Append Map to the shared enum without renumbering existing roles; explicit CLI wins over DefaultClientRole, unknown stays Standalone.
2. Build a small independent MapShell using retained tool adapters, with a map-first header and tool strip. Do not clone CommandShell. Retain Standalone's combined presentation via conditional construction.
3. Command constructs only command widgets; no CenterHost/map toolbar/path drawer. Extract pause/resume policy into a GameInstance business service and make alerts select directly through Registry.
4. Gate world initialization, source actors, scene spawning, camera and input by role. Command must remove map sources before construction and disable scene rendering; merely covering/hiding the Cesium viewport is insufficient.
5. Preserve Video V2 code and Standalone startup. Add role lifecycle assertions for unique shells and Command's absent map service/camera/sources; run existing map behavior under Map.
6. Physically run roles separately at target 1920×1080, record actual mouse interaction and limits. No simultaneous three-client startup.

## Risks and boundaries

- Blueprint BeginPlay and UIManager fallback may reintroduce panels; audit both, use native UI-only Command controller.
- Cesium sources load before GameMode BeginPlay; early World hook must enforce ownership before construction, not just suspend/hide later.
- Existing A2 test startup assumptions must change with ownership, not its business assertions. Retain the exact 1082 vs 1080 known failure; do not change coordinates to conceal low-altitude 3D occlusion.
- Command selection cannot focus another process in M1. **Cross-client PrimarySelection synchronization = DEFERRED**. Future A4 uses Backend / IPC / WebSocket events for Command → Map → Video.
- Physical imagery depends on local connectivity/proxy; historical A3/V2 passes do not count as M1 verification. Raw logs may contain credentials and must not be committed wholesale.
- No real Backend production, UAV, Jetson/PX4, AI detection, true multi-stream, second Video host, window placement or A4 acceptance.
