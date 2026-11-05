# P1 repairs and Mock interaction verification

This record covers the repairs requested on 2026-11-05. The repository baseline
was `6827f20`, on `main`, synchronized with `origin/main`, and descended from the
accepted `0fcbb6b` baseline. Existing workstation changes were preserved.

## Repairs

- `e05d373`: compute the distance between segment interiors as well as their
  boundaries. This fixes crossing-route conflict detection in the traditional
  `/api/arrays` assembly planner. Six new regressions cover skew/intersecting,
  nearly parallel, collinear, degenerate and preassigned production paths. The
  existing height-separation test now preserves the assignments used in production.
- `0290fb0`: require a completed input line before interactive ARM confirmation.
  EOF, unavailable input and a stopped bridge remain unconfirmed. Explicit
  `ARM_NOW=1` retains its documented behavior. Numeric overflow is rejected as a
  packet error; integer metadata is bounded to the Backend protocol types.
  The protocol test loader includes its production helper dependencies.

## Component and protocol evidence

| Check | Result |
| --- | --- |
| Fresh Backend Release build | Passed |
| Backend unit tests | 205/205 passed |
| Jetson protocol, ARM and video unit tests | 27/27 passed |
| Launcher unit tests | 63/63 passed |
| Launcher mode integration | 7 scenarios passed |
| Plan lifecycle and context integration | Passed, including restart persistence |

The Jetson tests use production protocol functions and a fake receive socket;
they do not start ROS, publish a setpoint or transmit a flight command. All
Backend integration uses fresh task-owned Mock stores and loopback-only UDP
targets. Test processes and threads from those integrations were reclaimed.

## UE and native interaction evidence

UE Editor built successfully (181 actions). The initial component suite passed
4/4; the initial rendered suite passed 5/6. The failed MapControls test searched
for a direct text child of the Fleet Move button, but the Figma button now has
an icon/row container. The test locator was changed to the existing stable
`group_move` widget name; all visibility and business-state assertions remain.
This test-only correction is committed as `695f5b0`.
The original failed report is retained locally. The final rebuilt module passed
10/10 discovered and completed cases with no failures:

| Final UE check | Passed |
| --- | --- |
| NullRHI camera, route components and coordinate parser | 4/4 |
| Map RHI configuration and draft guards | 2/2 |
| Command RHI presentation and active-task overview | 2/2 |
| Video RHI presentation and request feedback | 2/2 |

Five rendered cases succeeded with warnings concerning local audio compatibility,
offline/minimal fixtures, startup diagnostics and one Slate scroll-target warning.
They are preserved in the raw reports. Final-build results and its unchanged
module hash are recorded separately in `validation.json`.

Native mouse, keyboard and wheel checks used the actual Launcher and all three
clients with a fresh Singapore DEMO Backend, isolated writable registries and no
real UDP targets. These checks reached 33 recorded observation checkpoints:

- Launcher stopped/ready states, all role heartbeats and Chinese language sync.
- Online Google/Cesium 3D scene, four Mock UAVs and the 2D raster switch.
- Map device/marker selection and Video target ACK; independent Video selection
  and the four-slot layout; explicit no-source media state.
- Default dragging leaves selection unchanged; button-enabled rectangle selects
  all four aircraft; Escape exits, and box selection preserves the Video target.
- Terrain-clearance rejection, explicit Mock lift preview, cancel without an
  execution, confirmed movement, completion, pause with stable Backend position
  and progress, resume, and camera wheel zoom.
- Empty-name validation, cancelled creation without a stub, draft creation,
  two-UAV assignment and acknowledged save, four native waypoint clicks, exact
  longitude editing, manual zoom while editing, unsaved-exit protection, draft
  saving while retaining the editor, and explicit edit completion.
- Deployment creates a frozen snapshot without starting the mission. A separate
  confirmation names both aircraft and four waypoints. Native group pause/resume
  and two-step abort agree with both Backend execution records.
- Launcher shutdown returns to stopped with all owned processes reclaimed. The
  Backend used the existing process-stop fallback; this does not resolve its P2
  direct network-thread shutdown issue.

The native run used module SHA256
`939c76f9e560cc0b572ecd30e944dd240cb59104e0baefd9f4efbd68194a4d48`.
The subsequent locator correction changes only test code; its rebuilt module
and automation results have their own hash in the validation record. Native
middle-button panning and right-button orbit dragging were not injected by the
available input API; the 2D/3D camera component cases cover those input paths.
No physical aircraft, physical geospatial alignment or camera playback is covered.

All 22 protected original business/configuration files retained their SHA256.
Native runtime cleanup reported no errors and no retained process ownership.
Raw logs and Backend snapshots remain in ignored local `Saved` directories;
only synthetic screenshots, sanitized unit logs and summaries are published.

See [native checkpoints](native-checkpoints.json) and [validation](validation.json).
Representative screenshots: [online 3D Map](screenshots/04-map-online-3d-0.jpg),
[box selection](screenshots/09-box-select-four-0.jpg),
[unsaved route guard](screenshots/22-dirty-route-exit-guard-0.jpg),
[deployed without execution](screenshots/25-deployed-not-started-0.jpg),
[two-step abort](screenshots/30-group-abort-confirmation-0.jpg).

## Workstation dependencies

The missing Cesium plugin was installed from the official CesiumGS release into
the ignored project plugin directory. It is Cesium for Unreal 2.31.0 for UE 5.8;
the package SHA256 is
`6bab4f6e16d384c72e2a4ef9e69dc80933ef5132ee31cd5f7c09640fa87cfe91`.
Its Win64 BuildId matches the installed UE 5.8.4 build. No credentials were
copied, and the user's Cesium configuration asset was preserved.

Official package: [Cesium for Unreal 2.31.0](https://github.com/CesiumGS/cesium-unreal/releases/tag/v2.31.0).

## Scope boundaries

The previously reviewed P2 findings (Command tactical-map duplicate routes and
Backend blocking shutdown) are outside the requested P1 source repair scope.
Their existence must not be obscured by an interaction-test pass.
