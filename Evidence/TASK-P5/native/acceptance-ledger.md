# Native acceptance — single-screen scope

Actual independent UE Command / Map / Video processes use the real P5 Backend on 19480/19481. Aircraft are synthetic and have no telemetry/camera. Read-only API snapshots support native UI observations; protocol replicas are counted separately.

| Gate | Result / evidence |
|---|---|
| A Unified Launch | PASS: Explorer double-click of DroneSecurityLauncher.cmd; four owned processes; doubleclick-ready.jpg and launcher JSONL |
| B Physical three-monitor placement | REMOVED by user; replaced with normal single-screen multi-window layout. Window placement rectangles and three independent PIDs recorded |
| C System Ready | PASS: exact instance + fresh hydrated presence; launcher and shared headers READY |
| D Operational selection | PASS: Command UAV-01 leaves video null, then explicit Video UAV-02 leaves active UAV-01 |
| E Explicit video | PASS: video-restored-target.jpg / deployment and recovery snapshots; NO SOURCE is honest |
| F Plan workflow | PASS: native Create Draft → Mission → Assign → Map request → two points → Save ACK → Validate → Review → Deploy; deployment-5 |
| G Map edit boundary | PASS: map-dirty-route.jpg / map-saved-route.jpg; Command hands edit to Map |
| H Localization | PASS: Command zh, then Video en; command/map/video-zh/en.jpg |
| I Video restart | PASS: video-offline-final.jpg and video-recovered-final.json/log; actual Video process replaced, other processes continue, all domains unchanged |
| J Map restart | PASS: map-offline.jpg; map-offline-request-rejected.jpg shows Map client unavailable; map-recovered.json compares all domains unchanged |
| K Command restart | PASS: command-offline.jpg; command-restored-deployment.jpg and command-recovered.json; other clients stay running |
| L Backend restart | PASS: backend-offline-final.jpg → backend-ready-recovered.jpg; unchanged UE PIDs; all four domain snapshots identical |
| M Shutdown | PASS: all owned processes exit; shutdown-initial.json shows no owned processes, separate baseline Backend remains reachable |
| N Relaunch | PASS: relaunch.json compares all four domain snapshots with before.json; no manual data repair |
| O Single-screen layout | PASS: three normal windows arranged automatically; Focus enlarges detailed work, Arrange restores overview; no physical-monitor gate |
| P Continuous final demonstration | PASS: demo-00-off through demo-15-stopped; actual deployment-10 with three waypoints; demo-complete.json and shutdown-final.json |

No console, manual UE process startup, manual window dragging, Backend state file editing or repair during the continuous demonstration. Earlier diagnostic/recovery sessions are kept separate. Screenshots are original window captures, not composites. Maximized windows in detailed steps use the product Focus/native maximize controls.

## Continuous final sequence

2026-09-19 18:00–18:08 local time. System STOPPED → Launch System → three roles READY → additional Video stop/restart check → open persisted draft plan-6 and select mission-7 → request Map editing → add third waypoint → Save ACK (revision 2) → check configuration → Review → Confirm Review → Deploy Plan → Confirm Deploy → deployment-10. Select operational UAV-02; video view version remains 1. Explicit task OPEN VIDEO changes target to UAV-01/version 2 while operational UAV remains UAV-02. Command switches zh-Hans; Map and Video converge. Video switches en; Map and Command converge. Inspect Command operation log and zero active alerts (one expected session warning from Video stop). Shutdown System → STOPPED with all owned processes gone.

The initial UAV-02 click before selecting the mission did not result in an acknowledged UAV change; the actual acknowledged selection is the later 18:05:53 operation and is shown in demo-07-active-uav.jpg. This does not substitute a click attempt for authoritative state.

Source data was never cleared between these runs. The original deployment-5 remains intact alongside deployment-10. Backend acknowledged snapshots, rather than UI text alone, establish final persistence and independence.
