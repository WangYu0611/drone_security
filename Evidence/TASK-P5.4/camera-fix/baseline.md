# Camera FIX-02 baseline evidence

- Worktree: C:/Users/wy331/Documents/UE5DroneControl-p5-4
- Branch: feat/triple-screen-p5-4-3d-multi-uav
- Initial HEAD: 3700f281030b5bd6f2fa0a729073ad43de8987a4
- Initial git status --short: empty (clean).
- Previous failure retained verbatim: previous-failure.log, from ue-regression-1790419021806383500/P54.CameraDampingAnd3DRoute/editor.log. This is the actual failing Camera run; the later 1790419225345131200 directory contains only MapLiveDraftGuard.
- Fresh baseline run: ue-regression-1790421811387014600; one discovered/performed, one success with warnings, zero failures, explicit completion, exit 0. Copy: before-test.log. This does not overwrite or retroactively turn the previous failure into a pass.
- Previous failure assertions are boolean-only: expected movement and a positive forward dot product; actual movement assertion false, direction assertion false. The old log did not record numeric states; they cannot be recovered honestly from it.
- Baseline native Stage1 Runtime: separate Backend, Command and Map launched through Launcher.runtime.Runtime. Saved/CameraFixQA is an isolated runtime store. A left drag from (700,360) to (700,440) moved the Mock UAV 01 label from y~340 to y~421 after settling. Expected: +80 screen pixels. Observed: approximately +81 pixels (screenshot measurement); direction correct. Screenshots before-native.png and before-down-settled.png.
- Inspection: target is changed by PanView, then velocity = offset/world delta seconds is integrated into the target on idle ticks. This is additional inertia, distinct from Actual-to-Target damping. Right/middle use polling and inverted MouseY while left uses absolute screen positions. Wheel unprojects Actual camera but applies the result to a possibly unsettled Target. UI preprocessor did not verify popup hit paths.
- Current-source root cause of the historical two assertions is NOT established: clean fresh run and native down drag did not reproduce them. Do not claim they prove reversed Y or a current click jump. Final tests must log explicit input/preconditions/target/actual and exercise the production input state machine without OS cursor polling.
