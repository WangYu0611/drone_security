# P5-A baseline

Base: 5237bb86cc992ef8f9c586492cb43c0cb888c4df. New independent P5 branch/worktree initially clean. Protected refs captured in refs-before.txt. No business edits before baseline.

- Backend Release configure/build: PASS. Visual Studio bundled CMake used (cmake not on PATH).
- Backend suite: 58/60; RETAINED HISTORICAL FAILURE: SegmentDistanceTest.Crossing and AssemblyPlannerTest.ConflictHeightSeparation.
- UE5.8 Development Editor: Result Succeeded (120 actions). UBA retried one compile under memory pressure.
- P4 real HTTP/WS with synthetic clients: 21/21 PASS in protocol-trial2. Initial protocol fixture lacked UAV records, failed at assignment; retained in protocol/ and protocol.log. No assertion changed.
- UE targeted regression: Command 2/2, Map/localization 2/2, Video 1/1; discovered 5, completed Success 5. Raw logs retained locally at Saved/P5Baseline pending redacted evidence copies.
- Existing Cesium enum/GameFeatureData diagnostics retained. Legacy DroneNetworkManager still reads DefaultGame.ini endpoints whereas context uses P1Http/P1Ws; baseline context tests pass but launcher must configure both existing transports consistently.
- Ownership check initially refused a restart because PowerShell JSON parsed timestamp into DateTime; corrected comparison uses UTC ticks, same PID/executable/creation time verified before stopping only the owned fixture.

Scope: see ../scope-amendment.md. Physical multi-monitor gates removed by the user; Single-Screen Multi-Window Layout replaces them. No P5 implementation tests or native acceptance claimed yet.
