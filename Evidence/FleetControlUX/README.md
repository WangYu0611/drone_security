# MAP fleet UX evidence

Baseline: `f4e3870a1ce9cb338d21ae09c921d54fde9f85c6`.

All execution evidence uses an isolated local Mock Backend. Native actions use mouse/keyboard; HTTP snapshots are read-only witnesses.

- `verification-1792142136280641800`: 10 discovered/completed, 8 passed; 2 failures caused by missing existing regression fixtures. Retained as failures.
- `verification-1792142438650404200`: fixtures prepared on a separate runtime; both failed suites rediscovered/completed/passed. All 10 unique tests have passing completion evidence. Five have environment warnings.
- Each verification directory contains a passing 7-scenario HTTP/WS protocol run. Fixture scripts are synthetic integration evidence, not native input.
- `functional-build-6.json`: automated test DLL hash. `native-tested-build-8.json`: full native acceptance DLL hash. Final binary and evidence SHA-256 values are in `manifest.json`.
- `native/`: before/after layout, step guidance, real slider and keyboard input, preview, group pause/resume/completion, stop confirmation/cancellation/abort, and authoritative snapshots.
- First native move: 30 to 80 m, pause/resume/completion. Second: 80 to 60 m completion. Third: target 200 m, paused then explicitly aborted before reaching it. Video Target remained identical.
- `ue-build-1.log`: retained failed compilation due to a local variable shadowing `UWidget::Slot`; later builds fix it without changing compiler checks.
- `localization*.log`: gather steps exit 0 and both resources contain all 43 new keys; overall exit 1 retains existing GameFeatureData startup errors.
- `runtime-logs/`: copied only after all task-owned native processes stopped.

Detailed instructions, changes, results, and limitations: `Docs/MAP-Fleet-Control-UX.md`.
