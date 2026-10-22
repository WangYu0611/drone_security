# Geographic Mock fixtures

`mock_execution_beijing.json` preserves the pre-Marina Bay default homes used by
the offline P5.5 map and historic Beijing route/trajectory regressions. Those
QA launchers explicitly select this file with startup reset disabled. Do not
relocate their saved routes or historical evidence when production defaults move.

The production launcher, startup visibility acceptance and
`Backend/tests/mock_position_transport.mjs` use `Backend/mock_execution.json`.
The PlanConfiguration runner also selects that production file whenever
`--mock-startup-reset` (alias `--reset-mock-startup`) is supplied. Avoid combining
that option with a historic Beijing route fixture.

Run `python Scripts/PlanConfiguration/test_fixture_policy.py` for the coordinate
and fixture-selection checks. Run `python Backend/tests/mock_startup_migration.py`
to verify the packaged default and two owned Backend restarts from synthetic old
state, without opening UE or accessing the user's saved runtime.
