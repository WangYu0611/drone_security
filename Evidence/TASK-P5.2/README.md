# TASK-P5.2 evidence index

Final interpretation and A–U ledger: `../../Docs/TASK-P5.2-SecurityMissionExecutionClosure.md`.

- `baseline/`: actual P5.1 HEAD/status and protected refs before implementation.
- `backend/results.xml`: 77 discovered / 75 passed / two allowed historical failures; 13 new execution tests pass. `launcher-tests.log`: 7/7.
- `protocol/`: isolated **synthetic** P4 (21), P5 (8), P5.1 (11), P5.2 (14) HTTP/WS tests and persistent recovery. Not native acceptance.
- `ue/`: fresh exported Command (3), Map (3), Video (1) automation results, compiler and localization logs. Successful tests include inherited warnings; discovery/completion counts are explicit.
- `native/`: original window screenshots, actual UI-created business records, read-only time series, recovered states, independent Home, conflict/isolation evidence, and process/resource observations. No screenshots are composited or redrawn.
- `native/uninterrupted-execution-18.json` and `native/uninterrupted-execution-22.json`: complete created/started/four-waypoint/completed timelines, with timestamp, position, state and progress. No control actions intervened; E22 uses the final repaired rendering.
- `native/observations.jsonl.gz`: lossless compressed original time series (2,946 observations). The uncompressed SHA-256 and round-trip check are in `closure/observation-archive.json`; the local original is retained under `Saved/P5.2-native-raw`.
- `native/validation.json`: invariants checked against the read-only observations. Reproduce using `python Scripts/P5.2/validate_native.py --offline`.
- `native/command-gc-crash-before-fix.log`: preserved failed development trial. The lifetime repair was followed by forced-GC automation and successful native copying. This failed trial is not counted as PASS.
- `native/pre-fix-projection-trail.jpg`: failed visual trial, retained for audit. Final `06-executing-map.jpg`, `07-waypoint-progress.jpg`, and `12-completed-map.jpg` show the repaired receiver/shadow presentation without the duplicate curtain.
- `closure/`: protected-ref comparison and final owned-process shutdown proof.

Native captures span multiple runs. Recovery/control captures from earlier runs remain valid behavioral evidence; final-width CTA and completion screenshots document the repaired layout. Do not infer field-flight, real camera, multi-host, or performance certification from any Stage 1 simulation evidence.
