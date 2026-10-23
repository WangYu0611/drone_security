# Single-aircraft manual takeover

An operator can select one UAV executing a deployed mission and confirm a manual
movement target. The Backend terminates only that UAV's original execution and
creates one independently controlled fleet movement. The original mission does
not automatically resume. The deployed plan, saved route and other group members
remain intact; collision and real-transport failure protections still apply.

Send `POST /api/security-plans` from an online subscribed `Map` or `Command` client:

```json
{
  "instance_id": "<subscribed client>",
  "action": "execution_takeover_move",
  "request_id": "<unique request ID>",
  "execution_id": "<original execution ID>",
  "uav_id": "UAV-01",
  "control_version": 3,
  "target": {"latitude": 1.28, "longitude": 103.86, "altitude": 80.0}
}
```

The target uses WGS84 degrees and ellipsoid metres, matching `execution_group_move`.
`target_area_radius_m` is optional and must be a finite positive number when present.
Do not include `assigned_uav_id` or `assigned_uav_ids`: takeover accepts one explicit
aircraft and requires its matching original execution. Eligible original states
are `EXECUTING`, `RETURNING` and `PAUSED` with a deployment and plan. Configuration
locks block takeover movement. `Video` cannot issue it.

All validation and predicted separation checks run before transport changes. On
success a single `SecurityPlansChanged` snapshot contains:

- Original execution: `ABORTED`, `completion_reason: MANUAL_TAKEOVER`, a
  `takeover_execution_id` backlink, and its retained route, progress and deployment.
- New execution: a new `execution_id` and independent `group_id`,
  `control_kind: GROUP_MOVE`, and `source_execution_id`, `source_plan_id`,
  `source_mission_id`, `source_deployment_id`. The response `execution_id` is the new
  movement. Normal pause/resume/abort controls apply to this new group.
- Event `MISSION_MANUALLY_TAKEN_OVER` and the new movement's `EXECUTION_CREATED` event.

The original group's controls cannot target the new movement. Ordinary
`execution_group_move` continues to reject occupied UAVs. Reusing the same
`request_id` and request body returns the original result; changing the body is
`REQUEST_ID_REUSED`. Stale `control_version` is `EXECUTION_STALE`, and mismatched or
missing `uav_id` is `EXECUTION_UAV_MISMATCH`.

The Real adapter first clears the old command queue and requests a measured-position
hold. Rejected hold leaves both executions `FAILED` with `REAL_HOLD_REJECTED`; the
HTTP request fails, repeats return the same failure, and the new movement never
dispatches. The existing Real group failure rule then holds remaining original
group members. If persistent storage fails after hold, both in-memory executions
become `FAILED` with `REAL_TAKEOVER_PERSIST_FAILED` and the request fails. Operator
review is required; the prior persisted Real execution still follows the existing
restart rule that pauses it until explicit resume. Failed storage cannot be claimed
as a durably committed takeover. WebSocket publication failure after a successful
Real commit does not roll back accepted control intent.

Regression coverage is in `tests/test_p52_execution.cpp`: `ManualTakeover.*` and
`P54RealAdapter.ManualTakeover*`. These exercise Mock motion and synthetic Real
adapter callbacks; they are not physical-flight verification.
