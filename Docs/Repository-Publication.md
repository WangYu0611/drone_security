# Repository publication

This repository starts with a sanitized snapshot of Stage 1 P5.2.

- Source revision: `e9ce39402f3bc6222e9d7b2f3c93ef65b5870941` (original repository; history is not included here).
- Includes current tracked source, Unreal assets, Stage 1 audit documents, and audit evidence.
- Embedded access tokens were removed from historical evidence and the Cesium server asset in this publication copy. Configure a locally owned Cesium ion token in Unreal Editor before using ion services. Asset replacements preserve serialized byte lengths; runtime validation of this publication copy has not been performed.
- Audit and test records describe the original revision and retain their original limitations. Stage 1 execution is Mock/Simulation, not real flight.
- Build caches, local Saved data, and the duplicate Stage1.zip archive are not included.
- The original workspace and its Git history were not modified.
- The original `px4_msgs` gitlink is preserved at `392e831c1f659429ca83902e66820d7094591410`. Its working directory is empty and the source repository supplies no `.gitmodules` URL, so this snapshot does not add its contents.
