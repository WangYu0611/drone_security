# UX-01 baseline and scope

At entry, `git status --short` was empty in `C:/Users/wy331/Documents/UE5DroneControl-p5-4`.
Branch: `feat/triple-screen-p5-4-3d-multi-uav`.
HEAD: `f19d6e8b7e4abc9b5ff6c12cb48abccf789b383f` (`fix(p5.4): stabilize map camera input and damping`).
The existing isolated P5.4 checkout was reused. No reset, stash, merge, push, history rewrite, or main-branch change.

Native fixture is a separate copy of Saved/CameraFixQA/data, with provenance hashes in native-fixture-origin.json. Original QA data is preserved.
Native screenshot 01 was mistakenly identified as Map before the same-process-name capture ambiguity was detected; it shows Command and is NOT Map evidence. Screenshots 02 onward use a unique Map process.
Native screenshot 08 on build-02 reproduced an unintended fifth waypoint during an empty-map drag. Screenshot 09 records Undo restoring four points. This failure is retained and motivated native ground-click gesture ownership.

No Backend, plan schema, mission model, camera damping parameters, or P5.5 work is included.
