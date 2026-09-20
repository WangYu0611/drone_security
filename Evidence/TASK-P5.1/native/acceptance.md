# Native acceptance ledger

All workflow actions used the real native UI with the launcher's default single-screen layout. Command was never maximized to complete the workflow. Captures report 1007 x 953 for Command and 1007 x 542 for Map/Video on this desktop; this is the actual available desktop layout, rather than a claim of an exact 1000 x 1100 viewport.

| Scope | Evidence / observation |
|---|---|
| A | `final-00-system-off`, `final-10-launcher-ready`; four independent owned process identities in recovery snapshots |
| B/C | `final-04-new-plan-modal`, `final-05-created-default-task`, `final-06-task-config`; P5.1 UX Test Plan creates Task 01 and opens workspace directly; UAV-01 choice saves assignment |
| D | `final-07-command-editing`, `final-08-map-editing`; unique current Plan/Task/UAV and highlighted route step |
| E | `final-09-saved-still-editing`; three native map clicks saved, editor remains active. Fourth native click followed by Exit opens `final-14-unsaved-guard` |
| F | Continue editing preserves four points; Finish saves and exits (`final-15-map-finished`); Command auto-advances to `final-16-predeploy` |
| G/H | `final-17-deployed-readonly`, `final-18-new-version`; deployed page has no edit controls, explicitly says aircraft has not taken off and execution has not started; copy opens a distinct DRAFT |
| I | `final-19-command-zh` through `final-24-map-en`; Command Chinese and Video English synchronize across roles without losing active draft or four-point route |
| J | `final-11-map-offline`, `final-12-map-restored`; saved three-point editing context returns after Map restart, including Plan/Task/UAV and active edit mode |
| K | `final-13-command-restored`; restart returns to same route workflow and task, while Map retains its fourth unsaved point |
| L | `final-25-backend-offline`, `final-26-all-reconnecting`, recovery snapshots; Backend disconnect disables mutations, retained clients rehydrate |
| M | `final-28-shutdown`, `final-29-full-relaunch-context`; four owned roots absent after shutdown, complete saved domains equal after relaunch |

The UI workflow was exercised twice: initial diagnostics and final-build acceptance. Saved data retains both runs and therefore includes duplicate human-readable plan names; IDs remain distinct internally. User-entered plan/task names are preserved across culture changes.

Retained visual limits: short Map/Video tiles still use smaller inherited shell text; the Map tool list scrolls for statistics and waypoint details. Native guard choices were exercised without maximizing, but their smaller text and the Command workspace's content-sized cards remain subjects for the user's requested final experience review. No screenshot is a mockup or an image-generation output.
