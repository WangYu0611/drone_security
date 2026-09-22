# Repository publication

This repository starts with a sanitized snapshot of Stage 1 P5.2.

## P5.3 update (2026-09-22)

- Latest source revision: `97c644f9b2e624d494fa3410ff6de8253a7b2d9e`. Adds closed routes, atomic whole-plan movement, closed-loop Mock execution and Windows first-run tooling.
- Applied the P5.2-to-P5.3 tracked-file changes onto the existing public repository, preserving its sanitized assets and history. No original private history was imported.
- Removed embedded JWTs from 32 newly added evidence logs before publication. File-level counts are in `P5.3-Publication-Sanitization.json`; original local evidence was retained.
- Portability verification: 7 environment scenarios, 7 launcher regressions and 1 build orchestration test passed; the PowerShell entry point and live local dependency detection passed. Build orchestration testing used mocked compiler processes. A full clean compile and native run on the second computer remain unverified.
- The native local P5.3 demonstration completed a four-waypoint closed loop at 100% in 260 seconds after whole-plan movement. This is Mock execution; it is not a runtime test of this sanitized publication copy.
- Start with `Docs/First-Run-Windows.md`. Cesium must be installed separately into UE 5.8, and each user must configure their own ion credentials.

## Original P5.2 publication

- Source revision: `e9ce39402f3bc6222e9d7b2f3c93ef65b5870941` (original repository; history is not included here).
- Includes current tracked source, Unreal assets, Stage 1 audit documents, and audit evidence.
- Embedded access tokens were removed from historical evidence and the Cesium server asset in this publication copy. Configure a locally owned Cesium ion token in Unreal Editor before using ion services. Asset replacements preserve serialized byte lengths; runtime validation of this publication copy has not been performed.
- Audit and test records describe the original revision and retain their original limitations. Stage 1 execution is Mock/Simulation, not real flight.
- Build caches, local Saved data, and the duplicate Stage1.zip archive are not included.
- The original workspace and its Git history were not modified.
- The original `px4_msgs` gitlink is preserved at `392e831c1f659429ca83902e66820d7094591410`. Its working directory is empty and the source repository supplies no `.gitmodules` URL, so this snapshot does not add its contents.
