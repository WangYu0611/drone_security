# TASK-P1 acceptance evidence

Baseline: M1 acceptance `6a9dec83feaf3fb4faa9ce35c403670c0a2dfba0`. Native screenshots were captured from three separate UE5.8 game processes, target 1920x1080 at the host display scaling. No screenshot was redrawn or generated.

| Scenario | Command | Map | Video |
|---|---|---|---|
| 01 All online, UAV-01 / v19 | [screen](01-three-clients-online-command.jpg) | [screen](01-three-clients-online-map.jpg) | [screen](01-three-clients-online-video.jpg) |
| 02 Command clicks UAV-02 / v20 | [screen](02-command-select-uav-command.jpg) | [screen](02-command-select-uav-map.jpg) | [screen](02-command-select-uav-video.jpg) |
| 03 Map model clicks UAV-03 / v21 | [screen](03-map-select-uav-command.jpg) | [screen](03-map-select-uav-map.jpg) | [screen](03-map-select-uav-video.jpg) |
| 04 Video clicks UAV-01 / v22 | [screen](04-video-select-uav-command.jpg) | [screen](04-video-select-uav-map.jpg) | [screen](04-video-select-uav-video.jpg) |
| 05 Critical Alert selects UAV-03 / v23 | [screen](05-alert-sync-command.jpg) | [screen](05-alert-sync-map.jpg) | [screen](05-alert-sync-video.jpg) |
| 06 Map exits, Command selects UAV-02, new Map restores / v24 | [screen](06-map-restart-restore-command.jpg) | [new process](06-map-restart-restore-map.jpg) | covered by context/event log |
| 07 Backend stopped: all Offline | [screen](07-backend-reconnect-offline-command.jpg) | [screen](07-backend-reconnect-offline-map.jpg) | [screen](07-backend-reconnect-offline-video.jpg) |
| 07 Backend restarted: all Online / v24 | [screen](07-backend-reconnect-command.jpg) | [screen](07-backend-reconnect-map.jpg) | [screen](07-backend-reconnect-video.jpg) |

[Offline click rejected](07-offline-request-rejected-command.jpg): clicking UAV-01 while disconnected retains authoritative UAV-02.

- [Automated test counts and assertion errors](automation-summary.json)
- [Backend HTTP/WebSocket protocol results](protocol-results.txt)
- [UE selection latency by process](selection-timing.json): 8–51 ms request-to-Registry-apply, below 5 seconds; not a pixel-render latency measurement.
- [Sanitized P1 event logs](p1-events.txt)
- [Initial process evidence](01-processes.json), [surviving processes after Backend restart](07-surviving-processes.json)
- [Active alert and UAV atomic context](05-context.json), [Backend-generated alert](05-generated-alert.json)
- [Restored Backend context](07-restored-context.json), [three restored client identities](07-restored-clients.json)

Existing `M1.QA` labelled simulation data is used; ordinary mission/telemetry transport is isolated and no real UAV/camera/video stream is claimed. The independent P1 HTTP/WebSocket channel uses the actual compiled Backend. Source/slot highlights in Video are acceptance of selection, not playback. The full task report records initial failed attempts, repaired callback lifecycle, retained known failures and the final focused multi-selection ACK validation.
