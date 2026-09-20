# TASK-A3 — One-Click PIE Startup + Command UI Visual Overhaul

## Scope and baseline

Baseline: `2c7f06240bfdcb70248a64a4a758756c3dbb2c2e`, completed TASK-A2 on `feat/triple-screen-command`. Work branch: `feat/triple-screen-command-a3`. TASK-A2 acceptance and its document remain unchanged. Terrain occlusion, the 1082/1080 assertion, and real Backend integration are outside A3.

## Phase 1 — startup audit and implementation

- `Config/DefaultEngine.ini` already sets EditorStartupMap and GameDefaultMap to `/Game/Level/CesiumWorld.CesiumWorld`, GlobalDefaultGameMode to `DroneOpsGameMode`. Retained the single map and GameMode. There is no project-specific GameInstance class; Registry, Network and AlertStore continue as GameInstance subsystems.
- `UCommandScreenManager::ResolveClientRole` is the shared role entry used by the module world initialization hook, GameMode, controller and map service. Project `[CommandClient] DefaultClientRole=Command` supplies the default. Explicit `-ClientRole=<Role>` takes precedence. Standalone and Video remain valid overrides; unrecognized values retain the existing Standalone fallback, never silently opt into Command.
- `OnPostWorldInitialization` prepares the existing map service for the Command game/PIE CesiumWorld before heavy Cesium construction. GameMode PreInitializeComponents reuses it, then loads the existing BP controller. Controller BeginPlay initializes one manager/shell. UIManager delegates retained panels into that shell. A2's old-HUD suppression and default-collapsed drawers remain intact.
- `CommandStartupTest.cpp` checks project defaults, explicit Command/Video/Standalone overrides, case-insensitivity and unknown-role fallback without mutating the global command line.
- Official UE 5.8 ModelContextProtocol, ToolsetRegistry and AllToolsets are enabled for Editor targets. MCP is editor tooling, not a runtime startup dependency. Actual discovery and usage are recorded below after connection.

### Confirmed Phase 1 core (retained from the preceding session)

`ZERO-CONFIG EDITOR PLAY = PASS` for the role/startup core: Explorer opened the normal `.uproject`; the actual UnrealEditor process had no `-ClientRole` or map argument. CesiumWorld loaded automatically, physical Play entered Command in default 2D, and the PIE log contained one Command Shell. The A2 initial 2D layer policy was retained. UE 5.8 Editor compilation succeeded: 35/35 actions (`Saved/CommandQA/A3/phase1-build.log`). This core result does not imply that the additional imagery and full interaction checks below are complete.

### Proxy cleanup and continuation — 2026-09-14

- Source of the connection failure: shared `Config/DefaultEngine.ini`, `[HTTP] HttpProxyAddress=127.0.0.1:7890`. No listener existed at that port; the development machine's existing proxy listened on a different port.
- Removed the machine-specific proxy from shared defaults. Retained loopback exclusions. No TLS policy or error suppression changes. A no-proxy installation no longer inherits a project-imposed loopback proxy.
- For this machine's acceptance only, set `[HTTP] HttpProxyAddress` in ignored `Saved/Config/WindowsEditor/Engine.ini`. The local value is deliberately not part of the commit. Developers may use that local config or UE's `-httpproxy` override as needed.
- Preserved the preceding Editor log at `Saved/CommandQA/A3/phase1-initial-editor.log`, closed Editor, and opened the `.uproject` again through Explorer without role/map/proxy command-line parameters.
- The pre-existing SimpleWebSocket UE 5.7 compatibility prompt was accepted with user authorization. Post-cleanup physical PIE displayed online imagery; cached QA UAV selection, three physical map waypoints, Confirm, local Playback movement, drawer switching and 3D were observed. Evidence: `Before/01-default.png` through `Before/08-speed.png`, `phase1-physical-proxy.log`. These are local QA states, not a live Backend acceptance. Phase 2 is now in progress.

### Phase 2 implementation and incremental verification

- Shared `CommandTheme.h` defines graphite surfaces, borders, text hierarchy, cyan selection, green/amber/red status colors, spacing and rounded controls. Toolbar, Fleet, alert cards, selected Tactical Data Card and retained drawers use the same tokens.
- Region 1 toolbar: compile 6/6 PASS (`region1-build.log`). Region 2 Fleet: compile 4/4 PASS (`region2-build.log`), physical mouse Play and primary selection PASS (`After/region2-default.png`, `After/region2-selected.png`, `region2-physical.log`). Viewport metadata reports 1920x1080; Computer Use captures logical window pixels at the current display scale.
- Region 3 alert hierarchy and selected Tactical Data Card: compile 4/4 PASS after correcting the ScrollBox slot API (`region3-build-retry.log`); selected card refresh and empty-alert layout physically observed (`After/region3-card.png`). Missing telemetry remains N/A. A local white map region was observed; its root cause is not established and it is not claimed to be a proxy failure.
- Region 4 drawers: initial compile 11/11 PASS. Physical inspection found low-contrast Sequence text and a Geographic opening crash in the Slate font cache. UE5.8 `UEditableTextBox::SetWidgetStyle` forwards the argument address to Slate; the Theme helper had supplied a local style. Fixed it to use the widget-owned persistent style, reset drawer content tint, and clip long Sequence status messages with a full-text tooltip. Compile 12/12 PASS (`region4-fix-build.log`), followed by 13/13 PASS (`final-build.log`) for contrast fixes. Post-fix physical Geographic, Sequence and Speed opening/switching PASS; no repeated crash. Initial failure evidence is retained in `region4-crash.log`.
- Region 5 path display: ordered waypoint labels retain A2's camera-facing scaling. Map presentation reads existing planning/execution state for amber planning, cyan confirmed/idle, green active and muted completed paths; conflict color retains priority. No path/waypoint transforms, altitude, mission data or Playback data changed. `M_CommandPath` is the only new UE asset: an unlit spline-compatible material with exposure compensation. The built-in debug material was rejected after real PIE reported missing SplineMeshes usage. The asset was generated with Unreal Editor Python through its console, not MCP; its reproducible creation script is included.
- `CommandQA.AlertVisual` is an explicitly invoked development-only fixture using existing AlertStore delegates. Every injected message is prefixed `QA visual state`; it does not run at startup or transmit Backend data.
- During the initial Region 3 build, UBA killed one compiler process for memory pressure (36.7 GB committed). Subsequent builds use `-MaxParallelActions=2`. This does not establish the cause of separate Codex client interruptions reported by the user.

### Automated / override evidence from this continuation

| Check | Actual result | Evidence under `Saved/CommandQA/A3` |
|---|---|---|
| StartupRole + AlertStore | Discovered/executed 2; Success 2, Failed 0, NotRun 0; both explicitly `Test Completed. Result={Success}` | `StartupUnit/index.json`, `startup-unit.log` |
| Explicit `-ClientRole=Video` | Minimal unattended game startup logs `Command UI skipped: Video role is reserved`; default Command did not override Video | `override-video.log` |
| Explicit `-ClientRole=Standalone` | Minimal unattended game startup logs `Shell created, role=Standalone`; default Command did not override Standalone | `override-standalone.log` |
| Whitespace check | `git diff --check` passed; only Git line-ending conversion notices | current working tree |

The override checks use NullRHI and immediate quit after world startup; they prove role precedence, not physical Video or Standalone UI acceptance.

### Final Automated PASS / retained failure

Final UE 5.8 build: **26/26 actions, Succeeded, 118.20 seconds**, `Saved/CommandQA/A3/final-material-build.log`. The original A2/CesiumWorld assertions were not edited or relaxed.

| Test | Executed | Result | Errors | Warnings |
|---|---:|---|---:|---:|
| DroneOps.Command.A2MapMode | 1 | PASS | 0 | 59 |
| DroneOps.Command.AlertStore | 1 | PASS | 0 | 0 |
| DroneOps.Command.StartupRole | 1 | PASS | 0 | 0 |
| DroneOps.Command.CesiumWorld | 1 | Known failure | 1 | 72 |

The first process discovered/executed 3 tests; all have explicit `Test Completed. Result={Success}`. Report totals: succeeded 2, succeededWithWarnings 1, failed 0, notRun 0. The separate CesiumWorld process discovered/executed 1 test and has explicit `Result={Fail}`: **only** `Expected 'viewport height' to be 1080, but it was 1082.` Exit code 0 is not interpreted as test success. Logs and full reports: `final-material-a2.log`, `FinalMaterialAutomation/index.json`, `final-material-cesium.log`, `FinalMaterialCesiumWorld/index.json`, under `Saved/CommandQA/A3`. A reviewable result extract is committed at [automated-results.json](QA/TASK-A3/automated-results.json); raw logs remain local because they can contain network credentials.

A2MapMode rechecks default 2D, map clicks, path display scaling, primary selection, local Playback, the 2D/3D roundtrip and Geographic's retained 480×420 layout and drawer behavior. Warnings include unavailable local Backend connections and existing engine/plugin messages; no real Backend result is inferred.

### Unreal MCP evidence and current boundary

Unreal MCP plugin configured, but MCP tools were not exposed to the active Codex session.

In the preceding session, the official UE console generated `.codex/config.toml` with `ModelContextProtocol.GenerateClientConfig Codex` and started the official server. A thin HTTP JSON-RPC client successfully initialized, called `tools/list`, enumerated 52 toolsets through `list_toolsets`, and called `describe_toolset` for EditorAppToolset, SlateInspectorToolset and ConfigSettingsToolset. Evidence: `Saved/CommandQA/A3/mcp-transcript.jsonl`. This is protocol/discovery evidence only; no UI implementation, scene editing or physical mouse verification is attributed to those toolsets. Computer Use remains the physical-window verification path.

### Physical mouse/window verification PASS — scoped local acceptance

`ZERO-CONFIG EDITOR PLAY = PASS`. Normal `.uproject` association launch, no role/map arguments; actual mouse Play loaded CesiumWorld and one Command Shell in default 2D. Final process command line is preserved in `Saved/CommandQA/A3/final-material-commandline.txt`. The existing Editor world may initialize its own Cesium preview sources: the deferred-source assertion applies to the **Command PIE world**, not every Editor preview request in the process. The final PIE log shows Google/OSM suspended with source=2 and Terrain using the A2 ellipsoid carrier before the first 3D click; all restore to their original source on 3D.

- Fleet and primary selection/highlight: PASS, QA UAV 02 retained across map mode switches. No old HUD, duplicate list or automatic top-level mission drawer appeared. These are cached QA descriptors; LIVE telemetry, multi-selection coming from a real Backend and numeric live telemetry are not physically verified.
- Mission Tools open/collapse and map hit testing: PASS. Three actual map clicks created three additional waypoints (plus the existing origin). Amber planning line, waypoint markers and ordered labels are visible in [planning](QA/TASK-A3/03-path-planning.jpg). Bright imagery/white regions still reduce label contrast; no claim of perfect readability on every background.
- Confirm and Local Playback: PASS, actual popup and local-preview choice, visible UAV movement and increasing elapsed time. Selection and the route persist through 3D → 2D. The route displays existing conflict-priority red during this QA run. Conflict-free Active green, Confirmed cyan and Completed gray are implemented presentation mappings, but were not each separately physically exercised; no business state was forced for screenshots.
- Geographic / Sequence / Speed: PASS for opening, closing and mutually exclusive replacement. Geographic remains 480×420 with scroll; original objects/callbacks are retained. Post-fix Geographic was opened in successive runs without repeating the Slate crash. Sequence's empty-file state and Speed's expanded input were checked. No saved real mission file was available for physical Sequence dispatch validation.
- Alert: PASS for CRITICAL, WARNING and INFO hierarchy, timestamp/title/target, scrolling and text bounds. All injected messages explicitly say `QA visual state`; the development-only console fixture uses the existing AlertStore event path.
- Online imagery: connectivity restored using ignored local proxy config; satellite imagery is visible. **Conditional** display result because local rectangular white map regions recur. Their cause was not established in A3 and is not concealed in screenshots.

Evidence: [Before / After and all requested states](QA/TASK-A3/README.md). Local logs: `final-drawers-alerts-physical.log`, `final-material-physical.log`; actual viewport metadata is 1920×1080, while window captures reflect the current Windows display scale. No broad DPI/responsive redesign was performed.

### Known issues / deferred / unverified

- Existing `1082 vs 1080` window-height assertion: retain unchanged; do not relax business tests.
- Low-altitude path occlusion by 3D Terrain: deferred until formal 3D map and altitude semantics are decided.
- Real Backend / Jetson / PX4 / flight chain: not validated by these local startup and unit checks.
- Pre-existing SimpleWebSocket UE 5.7 compatibility prompt on UE 5.8 normal project open: present as described above.
- AllToolsets previously produced a GameFeatureData AssetManager configuration warning; no unrelated asset-manager changes were made.
- Local rectangular white map regions and transient 3D Lumen exposure warnings remain visible. Online connectivity is restored, but complete imagery coverage/render quality is not a PASS claim.
- No conflict-free visual acceptance for every path state, no full physical multi-selection/live-telemetry matrix, no packaged/cooked build or multi-DPI acceptance. No flow/pulse animation was added.

### Modified files / UE assets

| Area | Files |
|---|---|
| Startup and proxy | `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`, `Command/CommandScreenManager.cpp/.h`, `Tests/CommandStartupTest.cpp` |
| Shared UI | `Command/CommandTheme.h`, `Command/CommandActionButton.cpp`, `Command/CommandAlarmPanel.cpp`, `Command/CommandShellWidget.cpp/.h`, `UI/DroneListItemWidget.cpp` |
| Retained tools | `UI/DefaultSegmentSpeedWidget.cpp`, `UI/GeographicTargetPanelWidget.cpp`, `UI/SequenceDispatchPanelWidget.cpp`, `UI/PreviewConfirmPopupWidget.cpp` |
| Path presentation | `PathEditor/DronePathActor.cpp/.h`, `PathEditor/DroneWaypointActor.cpp`, `Content/Command/Materials/M_CommandPath.uasset`, `Scripts/create_command_path_material.py` |
| QA and tooling | `Tests/CommandVisualQA.cpp`, `UE5DroneControl.uproject`, `.codex/config.toml`, this report and `Docs/QA/TASK-A3/*` |

C++ paths in this table are relative to `Source/UE5DroneControl`. **One new material asset; no existing UE map/Blueprint assets modified.** Registry, AlertStore, map interaction service, mission/Playback data and Video internals were not rewritten. A2's existing tests and report are unchanged. Workstation proxy values, logs and generated build products are not committed.

### Acceptance decision

**TASK-A3 = CONDITIONAL PASS** for zero-configuration Command Editor Play and the scoped visual overhaul. Automated business regression and the listed physical interactions pass. Conditions are the retained CesiumWorld geometry assertion and the explicitly documented imagery/3D and unverified-state boundaries above. This is not real Backend or flight acceptance.

Recommend review/integration into `feat/triple-screen` with these conditions visible; production/field sign-off still requires Backend and map-quality acceptance. This task only commits and pushes `feat/triple-screen-command-a3`; it does not merge into `feat/triple-screen` or `main`. The final commit hash and remote synchronization result are reported in the delivery message rather than embedded recursively in this commit.
