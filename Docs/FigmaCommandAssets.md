# Figma Command assets and presentation

Source: [无人机安防指挥系统UI](https://www.figma.com/design/NcGzq4onhsQHsJFAe0JBJe?node-id=2522-143), inspected on 2026-10-20. The eight high-fidelity design responses are translated into the existing native UE 5.8 Command UI. Map and Video retain their existing presentation helpers.

## Source frames

| Screen | Figma node |
| --- | --- |
| Overview | `2522:144` |
| Security plans | `2522:285` |
| New security plan | `2524:584` |
| Fleet | `2524:676` |
| Fleet label settings | `2527:950` |
| Online devices | `2547:3600` |
| Alerts | `2547:3754` |
| Operational logs | `2547:3908` |

All 153 asset references were downloaded with `curl -L` from the asset URLs returned by those design responses. SHA-256 comparison produced 55 distinct SVG files under `Content/UI/Figma`; original bytes, root dimensions, and view boxes are retained. No design screenshot is an implementation asset. Temporary download URLs are absent from runtime source.

`Content/UI/Figma/asset-manifest.json` records the source screen, frame node, generated variable, source filename, canonical local filename, SHA-256, root dimensions, and byte count for every reference. Identical content shared across frames maps to one file. The source name `f21a9.svg` has two different byte contents: the plans frame's asset is retained as `f21a9-6f68a840.svg`, while the overview/dialog backgrounds use `f21a9.svg`.

## Theme and native rendering

`Source/UE5DroneControl/Command/FigmaCommandStyle.h/.cpp` provides the Command-only `FigmaUI` helpers. Tokens come directly from the generated design code: black background, `#191b23` surface, `#0e131c` inset, `#363e4c` dialog outline, white primary text, `#aab4c1` secondary text, and `#7636f2` selected accent. Inputs use the design's `#0c0e13` field background. Selected navigation buttons use white capsules with black text.

UE 5.8 provides `FSlateVectorImageBrush` in `Brushes/SlateImageBrush.h`; `SlateRHIResourceManager` routes vector brushes through `FSlateVectorGraphicsCache`. The engine's `SlateSVGRasterizer.cpp` reads SVGs through `FFileHelper`, so `DefaultGame.ini` stages `UI/Figma` as UFS for packaged builds. No new module or third-party runtime dependency is required.

`FigmaUI::Icon(Tree, Name, Size)` returns a hit-test-invisible `UImage` fitted proportionally inside the requested bounds. Parent layout slots should center the image rather than stretch it. Original SVG colors are retained. The helper keeps cached brushes alive; `UImage::SetBrush` copies the brush into the image's owned property. Callers may copy `GetBrush()` into a native button or slider style. Close controls use the source plus SVG with its design-specified -45-degree transform, so callers must not rotate them again.

## Semantic asset mapping

| Semantic name | Canonical file | Design use |
| --- | --- | --- |
| `brand-mark` | `7fed1.svg` | Header drone mark; byte-identical to source `f088c.svg` |
| `brand-wordmark` | `32639.svg` | Header outlined Chinese wordmark |
| `drone` | `d302f.svg` | Overview fleet count; byte-identical to `e63a4.svg` |
| `drone-preview` | `2da1b.svg` | Fleet label preview |
| `online` / `alarm` / `log` | `9a8cd.svg` / `0b02f.svg` / `dcb0b.svg` | Overview card icons |
| `task` / `plan` | `a87d1.svg` / `0c589.svg` | Fleet task detail / current plan |
| `folder` | `04d38.svg` | Plans empty state |
| `no-position` / `no-video` / `no-alerts` | `13c46.svg` / `88d4b.svg` / `eee63.svg` | Corresponding empty states |
| `edit` | `af5be.svg` | Fleet label settings; byte-identical to `53ce8.svg` |
| `altitude` / `speed` | `d1330.svg` / `f2f72.svg` | Fleet telemetry |
| `battery` / `link` | `fc097.svg` / `2ec8e.svg` | Fleet telemetry |
| `close` | `725b6.svg` | New-plan and online-device close, rotated -45 degrees |
| `close-alert` / `close-log` | `3d45a.svg` | Alerts/log close, rotated -45 degrees; byte-identical to `ff636.svg` |
| `arrow-right` | `8ccfc.svg` | Circular card navigation arrow |
| `chevron-right` / `chevron-down` | `e9dcb.svg` / `7a78a.svg` | Bare chevrons; down byte-identical to `71dcc.svg` |
| `plus` / `plus-plan` | `f21a9.svg` / `f21a9-6f68a840.svg` | Overview map zoom / plans create action |
| `plus-black` / `plus-label` | `967dd.svg` / `fc0e2.svg` | New-plan header / label size controls |
| `minus` | `8ea89.svg` | Map zoom control |
| `follow` / `refresh` | `b04c8.svg` / `525ec.svg` | Follow selection / refresh dialog |
| `fullscreen` | `7bd2d.svg` | Fleet video expand control |
| `check` / `check-dark` | `b84be.svg` / `5f292.svg` | Fleet selected checkbox / label preview checkbox |
| `health` | `ca9f9.svg` | Current mission card |
| `status-online` / `status-online-large` / `status-offline` | `07f8a.svg` / `b4f9d.svg` / `f261e.svg` | Header and client presence dots |
| `icon-disc` | `94272.svg` | Overview card icon backing disc |
| `label-color-0` through `label-color-5` | `661eb.svg`, `ef49a.svg`, `57f65.svg`, `66921.svg`, `51b48.svg`, `c90d1.svg` | Label palette in source ellipse12–17 order |
| `label-color-ring` / `label-slider-thumb` | `77ab9.svg` / `bb35b.svg` | Palette selection ring / slider thumb |
| `label-separator` | `8dd75.svg` | Label dialog separators |
| `fleet-divider` / `metric-divider` | `428c1.svg` / `d305f.svg` | Telemetry separators; vertical line retains the design's -89.9-degree rotation |

Descriptive aliases `label-light`, `label-gray`, `label-blue`, `label-purple`, `label-yellow`, `label-green`, `label-ring`, and `label-dot` refer to the same palette resources. Remaining source divider SVGs are preserved in the manifest for inspection and exact layout use.

Divider layout reserves at least one logical pixel so Slate's vector cache does not round its raster size to zero. Rotated dividers use an explicit canvas slot to prevent parent layout from squeezing the unrotated image. Text sizes are converted from design pixels to Slate points; the engine's composite font preserves Chinese fallback.

## Verification boundary

Asset verification checked every manifest reference for a non-empty file and matching SHA-256, and every semantic mapping for an existing file. All 55 SVGs were rendered into an inspection contact sheet to check their visible identity and proportions. Compilation and real Command viewport verification belong to the integration validation; asset hashing and offline SVG inspection do not establish native UI acceptance.
