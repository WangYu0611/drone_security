# Diagnostic and historical trials

These records are retained, not used as selected PASS evidence.

Text evidence copies normalize line endings and trailing whitespace for Git. This changes formatting only; failure messages and trial contents remain. Original local Saved logs and SHA-256 values are retained in the manifest.

- Baseline protocol trial1 lacked synthetic aircraft records; assignment failed. Trial2 passed all 21 checks after fixture setup. Assertions were not relaxed.
- Backend P5 build trial1 could not link a running owned baseline executable. After verifying and stopping that owned process, trial2 built successfully.
- UE shell build trial1 reported include-order errors; after fixing source, the build succeeded. Trial2 also succeeded.
- GatherText trial1 used a relative project path, trial2 had PowerShell argument splitting at `.ini`. Trial3 used an absolute project path and one quoted config argument. Localization generated successfully, with retained GameFeatureData diagnostics at engine exit.
- Initial native launcher process predates `-unattended` and displayed the inherited SimpleWebSocket 5.7 descriptor warning. Updated double-click launch used the already compiled UE5.8 plugin without an interactive prompt. Initial Video recovery is diagnostic; final startup/recovery uses the corrected arguments.
- Initial Backend recovery revealed an actual launcher defect: health polling could silently ignore a click while busy. Commit d73b027 queues user actions during polling and keeps controls enabled. The final Backend outage/recovery passed using the launcher UI and unchanged client PIDs.
- Recovery oracle trial1 expected canonical keys while the earlier snapshot used endpoint names; it raised KeyError before comparing all domains. Added key normalization only. Full later comparisons passed without changing saved snapshots.
- `recovery/backend-recovered-check.log` is an actual offline timeout from the earlier ignored-click trial. `backend-recovered-final.log/json` is the selected successful rerun.
- UE regression runner trial1 incorrectly treated succeededWithWarnings as failure although Command reported two Test Completed Success results. `automation/runner-trial1-summary.json` retains that incorrect runner summary; the raw UE report is authoritative. Runner now adds succeeded + succeededWithWarnings and still requires the expected test count and zero failures. No assertion or test expectation changed.
- `native/command-ready-recovered.jpg` captured an occluding window crop and is diagnostic only. Use `command-restored-deployment.jpg` and recovery snapshots instead. All images are unmodified original captures; none is a synthesized desktop.

Native fixture uses synthetic UAV records with no telemetry or camera. Legacy registry display aliases (`QA original name`) and occasional bilingual offline captions are retained P4 content; product health uses the common localized P5 header. Historical Cesium imagery/terrain/cache and GameFeatureData diagnostics remain out of scope unless they block the native workflow.
