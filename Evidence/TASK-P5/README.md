# TASK-P5 — Stage 1 final integration

**CONDITIONAL PASS — STAGE 1 COMPLETE**, for the user-amended **Single-Screen Multi-Window Layout** scope. One computer, three independent UE clients, one shared authoritative Backend. Physical monitor detection/mapping, borderless mode, DPI/display IDs and physical triple-screen gates are removed, not pending failures.

Branch: `feat/triple-screen-p5-stage1-integration`. Base: `5237bb86cc992ef8f9c586492cb43c0cb888c4df`. Runtime recovery fix: `d73b027`; reproducibility tools: `aae1694`. Final branch HEAD is the closure commit containing this report; exact SHA is returned with delivery.

## Selected final results

| Layer | Result | Evidence |
|---|---|---|
| Backend Release | Build PASS; 58/60 tests, same two historic failures | health/backend-build-trial2.log; backend/tests.json/log |
| UE5.8 Development Editor | Build PASS | automation/ue-build-trial2.log |
| Launcher | 5/5 tests, real double-click launch and owned shutdown | automation/launcher-tests-final.log; native/doubleclick-ready.jpg; recovery/shutdown-final.json |
| P4 real HTTP/WS | 21/21 PASS, synthetic clients | protocol/p4-final/; protocol/p4-final.log |
| P5 presence/protocol recovery | 8/8 PASS, synthetic replicas | protocol/trial1/p5-presence.json |
| UE P4 regression | Discovered 5, Success 5, Failed 0, NotRun 0 | automation/verified-summary.json; ue-command/map/video/index.json |
| Native three-client workflow | PASS in amended scope | native/acceptance-ledger.md; native/demo-*.jpg |
| Native recovery | Video, Map, Command, Backend and whole-system relaunch PASS | recovery/*recovered*.json/log; recovery/relaunch.json |
| Native language | Three clients zh-Hans then en | native/demo-08 through demo-13 |
| Final shutdown | No owned processes remain | recovery/shutdown-final.json |
| Git protection | All recorded non-P5 branches unchanged | closure/protected-refs.txt; closure/p4-status.txt |

Full report: [TASK-P5-Stage1FinalIntegration.md](../../Docs/TASK-P5-Stage1FinalIntegration.md). Start instructions: [Launcher README](../../Launcher/README.md).

## Evidence classification

- **Selected Final Result**: the table above, final native sequence and exact domain comparisons. Native screenshots are original uncomposited window captures; PID/presence and launcher rectangles establish concurrent processes and layout.
- **Diagnostic**: raw startup/build/gather logs, initial native sessions, performance observations, legacy warnings. Logs under `logs/` are redacted copies; manifest maps original local Saved logs and SHA-256 values. Original logs remain intact.
- **Historical Failing Trial**: baseline protocol trial1, initial locked-executable build, initial shell build errors, initial offline button defect, and the UE runner count bug. See [diagnostics.md](diagnostics.md). Failed trials have not been deleted or represented as PASS.

## Boundaries

Retained historical failures: `SegmentDistanceTest.Crossing`, `AssemblyPlannerTest.ConflictHeightSeparation`. Retained Cesium map/network/cache, 1082 vs 1080, low-altitude route occlusion, GameFeatureData diagnostics, legacy registry aliases/bilingual offline captions and dense tiled UI. Focus/maximize is used for detailed work. No OOM/GPU crash/repeated UE crash observed. VRAM observation: 5672/6144 MiB, not a performance certification.

Synthetic aircraft, no camera source and no aircraft execution. Stage 2 remains multi-host Video migration, real RTSP/WebRTC/camera, Jetson/AI, real UAV/PX4/MAVLink and execution safety. A configuration deployment is not a flight authorization or flight start.
