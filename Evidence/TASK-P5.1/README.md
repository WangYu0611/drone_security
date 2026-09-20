# TASK-P5.1 evidence

The original scope is preserved in [request.md](request.md). Native actions use the real Launcher and independent Command, Map and Video windows. Synthetic UAV registry data has no camera, flight or telemetry source; see [fixture boundary](native/fixture-boundary.md).

- `visual/final-*.jpg`: final-build raw native window captures, with timestamp/window/dimension metadata. These are JPEG bytes returned by the native capture tool, without cropping, compositing or repainting.
- `visual/01-*` through `18-*`: earlier diagnostic UX pass before final visual refinements. They are not final UI acceptance images.
- `automation/`: Backend GoogleTest JSON, P4/P5/P5.1 HTTP/WS results, Launcher tests, UE reports, build/localization logs. Files explicitly labelled diagnostic or initial-failure retain repaired defects or failed trials, not passing evidence.
- `recovery/`: read-only authoritative snapshots, domain comparisons and process ownership checks. Historical OFFLINE client registrations may coexist with a new hydrated ONLINE instance.
- `logs/`: normalized, credential-redacted runtime copies; source hashes identify originals retained under Saved.
- `closure/`: protected-ref comparison and repository checks.

Native workflow decisions are made through UI. `Scripts/P5.1/record_acceptance.py` only observes APIs and ownership records. Protocol tests and UE automation use separate loopback ports 19580/19581 and synthetic clients. Native Launcher uses 19480/19481.

See [closure report](../../Docs/TASK-P5.1-CommandPlanUXClosure.md) for evidence boundaries, final results and retained issues. No Stage 2 acceptance is implied.
