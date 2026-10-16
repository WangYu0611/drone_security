# 3D Fleet Control evidence

Baseline: `1e31715ceb7d3f35f1819c9fe6dfdc503d15c2cf`.

Scope: offline Mock, UE 5.8 Editor. No real flight or online Cesium terrain claim.

- `verification-1792129883275578200`: retained first failures; not PASS evidence.
- `verification-1792130424600612300`: final functional automation, 5 discovered / 5 completed / 5 passed (2 with environment warnings); HTTP/WS protocol 7 scenarios passed with +50 m target height.
- `backend-focused.*`: 35/35 passed. `backend-full.*`: 135/137 passed; two pre-existing failures retained.
- `ue-build-3.log`: functional version used for automation and native ascent. `tested-build-3.json`: its DLL hash.
- `ue-build-4.log`: final version, only the height reset button label changed; final native descent and language checks use this build.
- `native/`: genuine mouse/keyboard inputs with read-only Backend snapshots. Four UAVs ascended 30 to 80 m with pause/resume; final build descended 80 to 50 m. Invalid text and view change remain preview-only.
- `localization*.log`: gather steps exit 0, overall process exit 1 due existing GameFeatureData Asset Manager startup errors. Both language resources generated and natively checked.
- `runtime-logs/`: stopped task-owned native QA processes. `manifest.json`: evidence and final binary SHA-256 hashes.

Detailed operation, failure corrections, results, and limitations: `Docs/MAP-Fleet-Control-3D.md`.
