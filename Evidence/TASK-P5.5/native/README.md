# Native evidence boundary

These are unmodified screenshots from the real Stage 1 Map window, using the explicitly synthetic `/Game/Tests/P55OfflineMap`. They are not satellite imagery and do not prove field/aircraft execution. The actual local QA plan was created through the existing Backend protocol (`../native-fixture.json`); subsequent selection, edit, speed/height entry, drag, Save Draft, Finish and Discard were mouse/keyboard interactions.

## Screenshot index

| Evidence | Observation |
|---|---|
| 01, 02, 08 | Same final 2D frame, intentionally reused for full-route, altitude and closed-loop views. No duplicated first waypoint. |
| 03, 04, flow-interval-0/1/2, flow-times.json | Same route contains 1 m/s P01→P02 and 15 m/s P04→P05 and closure P05→P01. Interval times are 153 and 159 ms. FlowRate is 0.15 versus 2.4 cycles/s (16×). Still images plus time/parameter/model evidence are used; no video claim. |
| 05, 06 | Same stable native 3D overview and altitude view. |
| 07-joint-closeup | Three presentation-fixture routes with waypoint actors disabled. Caps/linear segments remain connected. |
| 09-edit-selected | Actual plan P02 selected, unsaved drag; current-route Core remains altitude-colored. |
| 10, conflict-pulse-interval, conflict-cleared | Explicit component conflict on fixture P02→P03. Other segments retain altitude/flow. Clear restores original presentation. This does not test the conflict detector itself. |
| 11-active-execution | **Presentation fixture only. NOT NATIVE VERIFIED as a Backend execution transition.** No fabricated execution progress. |
| 12-multi-route | Three independent fixture routes plus the actual plan at the right. |
| dark-surface-2d/3d | Synthetic dark material; production lighting remains present. |
| occlusion-3d | Opaque, non-colliding test block hides the middle of P01→P02. Core and Halo respect depth. This is intentional occlusion, not a mesh gap. |
| vertical-3d | P01/P02 share latitude/longitude and differ in height. No exploding or twisted mesh. |
| legend-zh-Hans | Native language switch; English shown in 01. |
| edit-wheel-lock | Actual 2D edit: wheel input leaves waypoint projections unchanged. |
| edit-drag-fixed | Actual P02 native drag after explicit mesh publish fix; adjacent segments follow. |
| edit-altitude-immediate | P02 60→120 m: unsaved route geometry/color changes immediately. |
| edit-cancel-restores | Discard restores saved P02 position and 60 m height. Disabled Inspector text may retain its old value; waypoint and actual saved route are restored. |
| edit-3d-lock-before/after | Actual 3D edit, wheel −600 leaves camera/route projections unchanged. |
| edit-3d-drag | P01 moved from approximately (817,296) to (817,260); adjacent segments immediately follow, other scene geometry stays fixed. |
| save-draft-3d-locked / finish-3d-unlocked | Save keeps editing and lock. Finish restores browsing controls with no camera jump. |
| perf-100-segments | 100-segment presentation fixture. Native zoom responds and shader animation continues. Dense inherited waypoint labels overlap; this is not a label-layout acceptance. Drag/churn coverage for 100 segments is component automation. |

## Retained failures and limits

- `failure-01-dark-core.png`: initial exposure clamp made the Core nearly black. Fixed by reducing the EyeAdaptation divisor floor from 0.001 to 1e-8.
- `02-precontrast-overview.png`: earlier contrast iteration, not final acceptance.
- `edit-drag-after.png` and `edit-drag-settled.png`: before the explicit UpdateMesh fix, moved handle/Inspector but old rendered line position. Retained rather than overwritten.
- The drag API returns after release. Within-gesture no-flicker is supported by deferred-preview/component identity assertions, not continuous native video.
- Sustained native right/middle-button 3D orbit/pan gestures are unsupported by this tool: **NOT NATIVE VERIFIED**. Their input ownership remains covered by preserved Camera Lock automation.
- Cesium imagery/real buildings are unavailable in the credential-free public copy. White/bright/dark synthetic surfaces and an opaque occluder are bounded substitutes. Real satellite readability and real terrain alignment remain **NOT NATIVE VERIFIED**.
- Production scene exposure/Lumen warnings and inherited waypoint label exposure are retained; no global exposure, camera or depth-test workaround was applied.
