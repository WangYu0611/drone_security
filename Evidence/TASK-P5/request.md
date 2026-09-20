# TASK-P5 — Stage 1 Final Integration / Triple-Screen Productization / Acceptance

## 0. Task Positioning

P5 是 **无人机安防三联屏项目阶段 1 的最后一个开发与验收任务**。

完成 P5 后：

> **Stage 1 = COMPLETE**

阶段 1 的最终目标不是接入真实无人机，也不是完成真实视频流或 Jetson/PX4，而是完成：

> **单台主机 + 三块显示器 + Backend + Command / Map / Video 三个独立客户端组成的一体化无人机安防指挥系统。**

P5 不重新设计 P4 已经完成的 Security Plan / Mission / Route / Localization / Video Target 业务架构。

P5 的核心目标是：

1. 三客户端真正形成一个整体系统；
2. 实现统一启动与关闭；
3. 自动识别并布局到三块显示器；
4. 建立统一系统状态与客户端健康状态；
5. 统一三屏视觉身份与运行体验；
6. 验证断线、重启、恢复；
7. 完成阶段 1 最终三屏真实运行验收。

---

# 1. Baseline

从 P4 最终交付开始。

Branch：

```text
feat/triple-screen-p4-workflow-localization
```

P4 Final HEAD：

```text
5237bb86cc992ef8f9c586492cb43c0cb888c4df
```

建立新的 P5 branch / worktree。

建议：

```text
feat/triple-screen-p5-stage1-integration
```

例如：

```text
C:/Users/wy331/Documents/UE5DroneControl-p5
```

开始之前必须记录：

```powershell
git rev-parse HEAD
git status --short
git branch --show-current
```

要求：

* P4 worktree 不得修改；
* P3 / P2 / P1 不得修改；
* main 不得修改；
* 不允许 reset/drop/stash 用户现有工作；
* 不允许重写历史 commit；
* 不允许直接 merge 到 main；
* 不允许 push，除非任务明确要求；
* P5 使用独立 branch/worktree。

如果基线与上述 HEAD 不一致，先停止业务修改并记录真实状态。

---

# 2. Stage 1 Final Architecture

阶段 1 最终结构：

```text
┌─────────────────────────────────────────────────────────────┐
│                     Drone Security System                   │
│                                                             │
│                  Shared Backend Authority                   │
│          HTTP / WebSocket / Persistent State                │
└──────────────┬────────────────┬─────────────────────────────┘
               │                │
       ┌───────▼──────┐ ┌───────▼──────┐ ┌──────────────▼──────┐
       │   COMMAND    │ │     MAP      │ │       VIDEO         │
       │  Left Screen │ │ Center Screen│ │    Right Screen     │
       │              │ │              │ │                     │
       │ Plans        │ │ 2D / 3D Map  │ │ Video Target        │
       │ Missions     │ │ Route Edit   │ │ 1 / 4 / 6 / Focus   │
       │ Fleet        │ │ UAV Spatial  │ │ Stream UI           │
       │ Alerts       │ │ Awareness    │ │ Future AI Overlay   │
       │ Logs         │ │              │ │                     │
       └──────────────┘ └──────────────┘ └─────────────────────┘
```

注意：

P5 不改变 Backend 权威原则。

禁止为 P5 新建第二套：

* 状态同步协议；
* WebSocket 通道；
* Plan 数据库；
* Operational Context；
* Video Target 状态；
* Localization 状态。

继续复用现有 Backend HTTP / WS / persistence 架构。

---

# 3. Final Screen Responsibility

P5 必须保持三屏职责边界。

## Command / Left Screen

主要承担：

* Security Plan 管理；
* Mission 管理；
* UAV Assignment；
* Validation；
* Review；
* Deploy；
* Copy as Draft；
* Fleet；
* Operational Status；
* Alerts；
* Logs / Events；
* 辅助 2D Tactical Overview；
* 向 Map 发起 Route Editing 请求；
* 向 Video 发起 View Video 请求。

Command 不是主要路线空间编辑器。

---

## Map / Center Screen

主要承担：

* 主态势地图；
* UAV 空间位置；
* Security Plan / Mission 空间显示；
* Route Editing；
* Waypoint 编辑；
* 2D / 3D 切换；
* Geographic / Sequence / Speed 等空间相关工具；
* 当前 Mission / UAV 空间上下文。

Plan/Mission CRUD 不迁移回 Map。

---

## Video / Right Screen

主要承担：

* Explicit Video Target；
* Video Feed；
* 1 / 4 / 6 / Focus Layout；
* Video source state；
* Retry / Offline / No Source；
* 为下一阶段真实视频 / AI detection / warning overlay 保留接口。

Global Active UAV 不得重新变成自动视频跟随控制。

---

# 4. P5 Scope

P5 只围绕以下六个方向开发：

```text
A. Unified Launcher
B. Multi-Monitor Placement
C. System Health / Presence
D. Unified Visual Shell
E. Recovery / Lifecycle
F. Stage 1 Final Acceptance
```

---

# 5. A — Unified Launcher

目前用户不应该需要分别：

* 开 Backend；
* 开 Command；
* 开 Map；
* 开 Video；
* 手动输入各种参数；
* 手动拖窗口到三个屏幕。

P5 必须提供统一 Stage 1 Launcher。

## Minimum Requirement

提供一个明确入口，例如：

```text
DroneSecurityLauncher.exe
```

或合理的项目 Launcher 方案。

启动：

```text
Launcher
   ↓
Backend
   ↓
Wait Backend Ready
   ↓
Command
Map
Video
```

### Backend readiness

不能只通过“进程已启动”判断 Backend ready。

必须检查真实：

```text
/api/context
```

或已有可靠 health endpoint。

Backend ready 后才启动三个 UE 客户端。

---

# 6. Launcher Runtime States

Launcher 至少识别：

```text
STARTING
READY
DEGRADED
FAILED
STOPPING
STOPPED
```

每个组件至少显示：

```text
Backend
Command
Map
Video
```

例如：

```text
BACKEND   ONLINE
COMMAND   ONLINE
MAP       ONLINE
VIDEO     ONLINE

SYSTEM READY
```

如果 Map 未启动：

```text
SYSTEM DEGRADED
MAP OFFLINE
```

不要把三个进程存在等同于 System Ready。

---

# 7. Unified Shutdown

必须提供统一关闭。

用户点击：

```text
Shutdown System
```

或关闭 Launcher 后，能够按照安全顺序结束：

```text
Command
Map
Video
Backend
```

可以根据 UE 生命周期调整具体顺序，但必须：

* 不留下本任务启动的后台孤儿进程；
* 不杀死任务启动前已经存在的无关 UE；
* 不杀非本项目进程；
* 明确记录 PID ownership。

---

# 8. Existing Backend Handling

如果 Backend 已经运行：

不得无条件再次启动。

必须：

1. 检查端口；
2. 检查真实 API；
3. 判断是否为兼容 Backend；
4. 允许复用；
5. Launcher 明确显示：

```text
BACKEND
CONNECTED TO EXISTING INSTANCE
```

不要仅通过“8080 被占用”判断为成功。

---

# 9. B — Multi-Monitor Detection

P5 必须支持三屏。

目标物理布局：

```text
LEFT          CENTER          RIGHT

Command       Map             Video
```

必须获取 Windows monitor 信息。

至少记录：

```text
Monitor count
Resolution
Working Area
Primary monitor
Coordinates
DPI / scale if accessible
```

例如：

```text
Display 1
1920x1080
X=-1920
Y=0

Display 2
1920x1080
X=0
Y=0
PRIMARY

Display 3
1920x1080
X=1920
Y=0
```

---

# 10. Automatic Placement

正常三屏配置：

```text
Left   → Command
Center → Map
Right  → Video
```

客户端启动后自动落到对应屏幕。

用户不需要手工拖动。

---

# 11. Borderless Fullscreen

阶段 1 推荐：

```text
Borderless Windowed Fullscreen
```

而不是 Exclusive Fullscreen。

目标：

* 无标题栏；
* 无 Windows 边框；
* 完整覆盖各自 monitor；
* Alt+Tab 正常；
* Launcher 能控制；
* 不抢占其他 monitor。

禁止为了“看起来全屏”直接修改 Windows 系统设置。

---

# 12. Fallback

如果只有一个显示器：

不得崩溃。

提供开发 fallback。

例如：

```text
Command : left third
Map     : center third
Video   : right third
```

或者：

```text
Windowed debug arrangement
```

必须明确标识：

```text
DEVELOPMENT SINGLE-DISPLAY MODE
```

单屏 fallback 不算正式三联屏验收。

---

# 13. Display Configuration

禁止把显示器编号永久硬编码：

```text
Monitor 1 = Command
Monitor 2 = Map
Monitor 3 = Video
```

因为 Windows Display ID 不保证等于物理左右顺序。

应按 Desktop Coordinate / 用户配置确定。

建议支持配置：

```yaml
display:
  command: left
  map: center
  video: right
```

如果存在更合适的已有配置结构，可以复用。

---

# 14. C — Client Presence / Health

Backend 已有 context heartbeat。

优先扩展现有机制。

不要额外建立 Presence Server。

每个客户端向 Backend 表明：

```text
role
client_instance_id
build/version
last_seen
state
```

例如：

```text
Command
ONLINE

Map
ONLINE

Video
ONLINE
```

---

# 15. Client State

建议至少：

```text
STARTING
ONLINE
DEGRADED
OFFLINE
```

必要时可以扩展：

```text
RECONNECTING
ERROR
```

不要把：

```text
Process alive
```

等同：

```text
Client ONLINE
```

ONLINE 至少意味着：

* UE client running；
* Backend connection established；
* heartbeat/context functioning。

---

# 16. System Status

建立系统级状态：

```text
SYSTEM READY
SYSTEM DEGRADED
SYSTEM OFFLINE
```

例如：

全部在线：

```text
BACKEND  ONLINE
COMMAND  ONLINE
MAP      ONLINE
VIDEO    ONLINE

SYSTEM READY
```

Map 崩溃：

```text
BACKEND  ONLINE
COMMAND  ONLINE
MAP      OFFLINE
VIDEO    ONLINE

SYSTEM DEGRADED
```

---

# 17. Status Presentation

三个屏幕不需要放巨大的监控面板。

推荐：

Header / Status Bar 中用轻量统一状态显示：

```text
● SYSTEM ONLINE
● CMD
● MAP
● VID
```

或者：

```text
SYSTEM READY
3 / 3 CLIENTS
```

保持专业，不要像 Debug HUD。

---

# 18. D — Unified Visual Shell

P5 必须让三个客户端第一次真正具有：

> “它们属于一个系统”

的视觉感。

但不要在 P5 重写全部 Widget。

重点建立统一 Shell / Design Tokens。

---

# 19. Global Header

Command / Map / Video 使用统一 Header 语言。

至少保持：

```text
System Name
Screen Role
System State
Language
Time
Optional Active Plan / UAV
```

示意：

```text
DRONE SECURITY COMMAND SYSTEM

COMMAND CENTER                 SYSTEM READY
```

```text
DRONE SECURITY COMMAND SYSTEM

TACTICAL MAP                   SYSTEM READY
```

```text
DRONE SECURITY COMMAND SYSTEM

VIDEO INTELLIGENCE             SYSTEM READY
```

中文对应：

```text
无人机安防指挥系统

指挥中心
战术地图
视频情报
```

具体用词可以根据已有 Localization Key 调整，但必须正式进入 Localization Catalog。

禁止硬编码中文/英文字符串绕过 P4 localization architecture。

---

# 20. Visual Language

统一：

* 背景层级；
* Panel；
* Divider；
* Border；
* Selected；
* Hover；
* Disabled；
* Critical Warning；
* Normal / Warning / Error；
* Font hierarchy；
* Button height；
* Corner style；
* Status chip；
* spacing scale。

尽可能集中为共享 Style / Token。

禁止三个 Client 各复制一套几乎相同参数。

---

# 21. Cross-Screen Continuity

三块屏幕并排以后应该像一个整体。

重点检查：

```text
Header height
Top margin
Bottom status region
Panel visual density
Typography
Color semantics
Selected state
Warning colors
```

不要求做一张跨三个进程真正连续绘制的图像。

要求的是视觉系统一致。

---

# 22. Screen Identity

虽然视觉统一，但不能让操作员搞不清屏幕。

三屏必须保留明确角色：

```text
COMMAND
MAP
VIDEO
```

不要为了统一，把三屏设计得完全相同。

---

# 23. Existing P4 Localization

所有 P5 新 UI：

必须支持：

```text
en
zh-Hans
```

必须：

* NSLOCTEXT / FText；
* stable keys；
* 正式 localization target；
* Backend confirmation；
* 与 P4 language persistence 一致。

不允许新增：

```cpp
if (Chinese)
   TEXT("中文")
else
   TEXT("English")
```

作为正式实现。

---

# 24. Language Switching

任何一个允许修改 language preference 的客户端切换后：

Backend authoritative preference 更新。

其它在线客户端同步更新。

必须验证：

```text
Command → 中文
Map → 中文
Video → 中文
```

以及：

```text
Video → English
Command → English
Map → English
```

不得通过重启三个客户端才能生效。

---

# 25. E — Lifecycle Recovery

这是 P5 最重要的工程验收之一。

必须验证：

```text
Backend restart
Command restart
Map restart
Video restart
```

---

# 26. Individual Client Restart

例如 Video 被关闭。

Command / Map 应继续运行。

Backend：

```text
Video = OFFLINE
```

Launcher：

```text
VIDEO OFFLINE
SYSTEM DEGRADED
```

重新启动 Video：

```text
VIDEO ONLINE
SYSTEM READY
```

恢复之后：

* video_target_uav_id 正确 hydrate；
* language 正确；
* global active UAV 正确；
* Browser 不应该因为恢复产生不合理隐式切源。

---

# 27. Map Restart

Map 重启后：

恢复共享：

```text
Active Plan
Active Mission
Active UAV
Language
Plan state
```

不要恢复一个已经失效的 dirty route editor session。

如果之前正在编辑 Route，而客户端异常退出：

Backend session/lease 按 P4 规则自然失效。

重启后应该：

```text
No fake dirty editor
```

但已保存 Backend Route 必须正确恢复。

---

# 28. Command Restart

Command 重启：

必须恢复：

```text
Plan
Mission
Deployment
Validation/review state
Operational context
Language
Client presence
```

不得自动创建新 Plan。

不得自动 Deploy。

---

# 29. Backend Restart

Backend 重启属于更高风险情况。

P5 必须验证：

Backend 停止：

三个 client 进入：

```text
BACKEND OFFLINE
RECONNECTING
```

不能：

* crash；
* 清空 UI；
* 随机切语言；
* 自动修改业务数据。

Backend 恢复后：

自动 reconnect。

重新 hydrate authoritative state。

进入：

```text
SYSTEM READY
```

---

# 30. No Fake Offline Success

如果 Backend offline：

禁止出现：

```text
Saved
Deployed
Language changed
Video target changed
```

等假成功。

P4 原有：

```text
failed/offline request does not mutate local authoritative preference
```

原则继续保持。

---

# 31. Startup State

Launcher 启动完成后：

三个 Client 应进入稳定 READY。

禁止依赖：

```text
用户分别点击三个窗口
用户重新选择 UAV
用户重新选 Plan
用户切一次语言
用户打开 Console
```

才能变成正确状态。

---

# 32. Preserve P4 Business Rules

以下 P4 行为必须保持。

## Security Plan

仍然：

```text
Create
Mission
Assignment
Route
Validation
Review
Deploy
Read-only
Copy as Draft
```

---

## Route Draft

仍然：

```text
Begin
Dirty
Save
Discard
Cancel
Lease
Session
Conflict protection
```

不能因为 Launcher / reconnect 简化掉。

---

## Video Target

仍然：

```text
video_target_uav_id
```

独立于：

```text
active_uav_id
```

不得恢复旧：

```text
Active UAV changes → Video automatically switches
```

行为。

---

# 33. Do Not Implement in P5

以下明确不属于 P5。

不要范围扩张。

### 不做真实飞机

不接：

```text
PX4
MAVLink
Autopilot
Real UAV
```

---

### 不做真实执行

P5 Deploy 继续保持：

```text
CONFIGURATION DEPLOYMENT
```

不是：

```text
EXECUTION START
```

不得调用：

```text
/api/arrays
AssemblyController
ExecutionEngine
```

除非仅做现有边界审计。

---

### 不做真实 Jetson

不要求：

```text
Jetson
RTSP hardware
physical camera
```

---

### 不做完整 WebRTC

可以为下一阶段预留 adapter/interface。

但 P5 不需要完成 production WebRTC。

---

### 不做 AI Detection

不开发：

```text
YOLO
Detection Box
AI inference
Threat classification
```

---

### 不修所有历史 Cesium 问题

以下继续保留：

```text
1082 vs 1080
3D low-altitude route occlusion
Cesium map/network dependency
SQLite cache concurrency
6GB VRAM capacity risk
GameFeatureData diagnostics
historic planner tests
```

除非直接阻塞 P5 三屏启动和阶段验收。

---

# 34. Launcher Safety

Launcher 不能使用危险式：

```powershell
taskkill /IM UnrealEditor.exe /F
```

来完成关闭。

必须只管理：

```text
自己启动的 process IDs
```

记录：

```text
PID
Role
Start Time
Executable
```

---

# 35. Single Instance

防止重复启动：

```text
Command × 2
Map × 2
Video × 2
```

正常模式应限制角色 single-instance。

开发 / automation 模式可以通过明确 flag 放宽。

---

# 36. Command-Line Role

继续使用已有：

```text
-ClientRole=Command
-ClientRole=Map
-ClientRole=Video
```

或现有等价实现。

不要创建三个复制的 UE project。

目标仍然是：

> 一个 UE 工程，通过 ClientRole 形成三个客户端。

如果当前项目结构实际已有差异，优先保留现有可工作的角色解析方式。

---

# 37. Configuration

统一配置至少包含：

```text
Backend endpoint
WS endpoint
Command screen
Map screen
Video screen
Window mode
Auto start
Log directory
```

不要把：

```text
127.0.0.1:8080
Display coordinates
Executable path
```

散落硬编码在多个类。

---

# 38. Logs

Launcher 创建独立日志。

例如：

```text
Logs/
  launcher.log
  backend.log
  command.log
  map.log
  video.log
```

或者保持 UE Saved/Logs，但 Launcher 至少记录：

```text
Launch
PID
Ready
Disconnect
Reconnect
Exit
Error
```

---

# 39. Sensitive Data

Evidence 中：

不得提交：

```text
Cesium token
API keys
credentials
private URL credentials
```

如果 log 中出现：

允许脱敏复制。

不得删除原测试失败内容来制造 PASS。

---

# 40. Development Sequence

按以下顺序开发。

不要同时大改所有模块。

---

## P5-A — Baseline

先完成：

```text
P4 HEAD verification
clean worktree
new P5 branch
Backend build
UE build
existing P4 targeted regression
```

如果 P4 baseline 本身失败：

先记录。

不要把 baseline failure 伪装成 P5 regression。

---

## P5-B — Launcher

完成：

```text
Backend launch
Backend health
Command launch
Map launch
Video launch
PID ownership
Shutdown
```

先不处理三显示器。

---

## P5-C — Display

再加入：

```text
Monitor enumeration
Role mapping
Automatic placement
Borderless
Fallback
```

---

## P5-D — Health

加入：

```text
Presence
Online/offline
System READY/DEGRADED
Header status
```

---

## P5-E — Visual Shell

统一：

```text
Header
Status
Role identity
Localization
Styles
```

不要先做纯视觉，然后返工架构。

---

## P5-F — Recovery

做：

```text
Client restart
Backend restart
Reconnect
Hydration
```

---

## P5-G — Final Acceptance

最后做：

```text
Three real clients
Three displays
Unified launcher
Workflow
Language
Video target
Restart
Shutdown
```

---

# 41. Required Automated Tests

至少增加以下自动测试。

命名可以合理调整，但必须覆盖真实行为。

## Launcher

```text
Launcher.BackendReadiness
Launcher.ProcessOwnership
Launcher.RoleArguments
Launcher.SafeShutdown
```

---

## Presence

```text
Presence.ThreeClientsOnline
Presence.ClientDisconnectReconnect
Presence.SystemReadyDegraded
```

---

## Localization

```text
Stage1.LocalizationSyncAcrossClients
```

---

## Recovery

```text
Stage1.VideoRestartHydration
Stage1.MapRestartHydration
Stage1.CommandRestartHydration
```

---

## P4 Regression

至少重新验证关键：

```text
Explicit Video Target
Route Draft Guard
Command Workflow
Localization
```

无需无意义重跑整个历史测试库。

---

# 42. Backend Tests

完整 Backend Release build。

运行 Backend test suite。

两项已知历史 planner failure：

如果仍然同样失败：

记录：

```text
RETAINED HISTORICAL FAILURE
```

不要修改 expected result 来让它变绿。

如果出现新的 Backend failure：

P5 不允许 FINAL PASS。

---

# 43. Protocol Regression

重新验证：

```text
HTTP
WS
Client presence
Context hydration
Language
Video target
Plan state
```

P4 21/21 可以作为基线。

P5 新增部分单独统计。

不能用“服务器返回 200”代替实际状态检查。

---

# 44. Native Acceptance

P5 必须做真正三客户端验收。

不能只用 unit test / automation 代替。

建议正式 Native Acceptance：

```text
A–P
```

---

# 45. Native A — Unified Launch

操作：

```text
双击 Launcher
```

不得手工分别启动 UE。

验收：

```text
Backend started
Command started
Map started
Video started
```

结果：

```text
PASS / FAIL
```

---

# 46. Native B — Three-Screen Placement

真实三显示器：

```text
Left   Command
Center Map
Right  Video
```

检查：

```text
Correct display
Correct resolution
Borderless
No accidental overlap
```

截图：

至少一张能证明三客户端真实同时运行。

---

# 47. Native C — System Ready

启动完成：

三端：

```text
Backend ONLINE
Command ONLINE
Map ONLINE
Video ONLINE

SYSTEM READY
```

Launcher 与客户端状态一致。

---

# 48. Native D — Operational Selection

Command：

选择 UAV-A。

Map：

正确反映 active UAV。

Video：

不得自动改变 Video Target。

验证 P4 independent video target 未回归。

---

# 49. Native E — Explicit Video

从 Command 或 Video：

明确：

```text
View Video UAV-B
```

Video：

切换到 UAV-B。

Command active UAV 可以仍为 UAV-A。

证明：

```text
Operational selection != Video target
```

---

# 50. Native F — Plan Workflow

执行最小真实流程：

```text
Create Draft
Create Mission
Assign UAV
Request Map Edit
Add Waypoints
Save
Validate
Review
Deploy
```

无需像 P4 一样重复极深边界测试。

这里主要验证：

> Launcher / 三屏集成没有破坏业务流程。

---

# 51. Native G — Map Route Edit

Map：

进入 Route Edit。

添加 waypoint。

Command 不得获得直接空间编辑权限。

Video 不得出现 Route Editor。

保存成功后同步。

---

# 52. Native H — Localization

Command 切：

```text
中文
```

三屏：

```text
Command 中文
Map 中文
Video 中文
```

然后从另一个 Client：

```text
English
```

三屏恢复 English。

保存六张 final screenshot：

```text
Command zh
Map zh
Video zh

Command en
Map en
Video en
```

---

# 53. Native I — Video Restart

关闭 Video。

验证：

```text
VIDEO OFFLINE
SYSTEM DEGRADED
```

重新启动。

验证：

```text
VIDEO ONLINE
SYSTEM READY
```

并恢复：

```text
Video Target
Language
```

---

# 54. Native J — Map Restart

关闭 Map。

Command：

Map status offline。

Route Edit 请求：

必须明确失败：

```text
MAP OFFLINE
```

不得伪进入编辑。

重启 Map。

系统恢复 READY。

---

# 55. Native K — Command Restart

关闭 Command。

Map / Video 不崩溃。

重新启动 Command。

恢复：

```text
Plan
Mission
Deployment
Active UAV
Language
```

---

# 56. Native L — Backend Restart

Backend 停止。

三客户端：

```text
OFFLINE / RECONNECTING
```

但不得崩溃。

恢复 Backend。

三个客户端自动 reconnect。

重新 hydrate 状态。

最终：

```text
SYSTEM READY
```

---

# 57. Native M — Launcher Shutdown

点击：

```text
Shutdown System
```

验证：

```text
Command exited
Map exited
Video exited
Backend exited
```

不得遗留 P5 自己启动的进程。

---

# 58. Native N — Relaunch

再次启动整个系统。

无需手工恢复状态。

验证：

```text
Language
Plan
Deployment
Active UAV
Video Target
```

与 Backend persisted state 一致。

---

# 59. Native O — Single Monitor Fallback

允许在独立开发 fixture / automation 中验证。

只验证：

```text
does not crash
all roles visible
dev mode clearly indicated
```

不算 Stage 1 三联屏正式 PASS 的替代品。

---

# 60. Native P — Final Stage 1 Demonstration

最后执行一次连续完整 Demo。

从：

```text
System OFF
```

开始。

步骤：

```text
1. Launch
2. Three screens ready
3. Select UAV
4. Open / select Security Plan
5. Edit Route
6. Validate / Review
7. Deploy configuration
8. Explicit View Video
9. Switch language
10. Return language
11. Check logs / alerts
12. Shutdown
```

要求：

全程不使用：

```text
UE Console
manual process start
manual window dragging
manual backend repair
manual state file editing
```

这是 Stage 1 最重要的最终证据。

---

# 61. Stage 1 Visual Acceptance

除功能外，还必须做视觉验收。

要求：

三个屏幕并排截图。

检查：

```text
Header aligned
Role names clear
System status consistent
Font hierarchy consistent
Panel language consistent
No obvious debug controls
No temporary developer labels
No overlapping text
No bilingual mixing
No inconsistent UAV ID formatting
```

允许保留已明确列入 retained issues 的 legacy 内容。

但新的 P5 Shell 不允许引入明显新混乱。

---

# 62. Performance Observation

P5 不建立新的高强度性能 Gate。

但最终三 UE client 同时运行时记录：

```text
CPU
RAM
GPU VRAM if available
FPS / frame condition
```

作为观察性 evidence。

不因为单次轻微波动直接 FAIL。

但如果出现：

```text
OOM
GPU crash
UE repeated crash
system unusable
```

Stage 1 不能通过。

---

# 63. Evidence Structure

创建：

```text
Evidence/TASK-P5/
```

建议：

```text
Evidence/TASK-P5/
  README.md
  request.md

  baseline/
  launcher/
  display/
  health/
  localization/
  recovery/
  native/
  automation/
  backend/
  protocol/
  closure/
```

---

# 64. Evidence Classification

继续使用 P4 的证据原则。

Evidence 分：

```text
Selected Final Result
Diagnostic
Historical Failing Trial
```

不要删除失败 Trial。

不要因为最终 PASS 就删除之前真实失败。

---

# 65. Screenshots

至少保存：

```text
three-screen-final.png

command-zh.png
map-zh.png
video-zh.png

command-en.png
map-en.png
video-en.png

video-offline.png
system-degraded.png
system-ready-recovered.png
```

如果真实三屏截图由不同图像组合：

必须说明。

不能伪造一张“合成截图”冒充真实同时运行证据。

---

# 66. Final Report

创建：

```text
Docs/TASK-P5-Stage1FinalIntegration.md
```

必须至少包含：

```text
Final Result
Git/Base/Final HEAD
Architecture
Launcher
Display placement
Presence/Health
Visual shell
Localization
Recovery
P4 regression
Backend tests
HTTP/WS
Automation
Native A-P
Performance observation
Known issues
Retained issues
Deferred
Evidence index
Git checks
Stage 1 conclusion
```

---

# 67. Git Commit Strategy

分阶段提交。

建议：

```text
baseline
launcher
display
presence
visual-shell
recovery
tests
evidence
docs
```

不要最后一个巨大 commit。

---

# 68. Protected Refs

任务结束时检查：

```text
main
P1
P2
P3
P4
```

与任务开始前保持一致。

P5 只修改 P5 branch。

---

# 69. Clean Gate

最终必须：

```powershell
git status --short
```

为空。

如果 Evidence 尚未提交：

不能声明：

```text
working tree clean
```

---

# 70. Final Result Classification

允许三种：

```text
PASS
CONDITIONAL PASS
FAIL
```

---

# 71. PASS Definition

P5 可以 PASS 的条件：

```text
Unified Launcher PASS
Three-monitor placement PASS
Three clients online PASS
System READY PASS
Localization PASS
Explicit Video Target regression PASS
Plan workflow regression PASS
Client restart PASS
Backend reconnect PASS
Unified shutdown PASS
Native final demo PASS
Git protection PASS
```

历史已知非 P5 Blocking Issue 可以继续保留。

---

# 72. CONDITIONAL PASS

只有以下情况才允许 CONDITIONAL PASS：

核心 Stage 1 三联屏已经正常工作，

但存在：

```text
non-blocking visual issue
historical Cesium issue
legacy planner failure
minor locale inconsistency
known diagnostic
```

必须明确写出边界。

---

# 73. FAIL

以下任一项失败：

```text
Cannot launch three clients
Cannot place three clients
Shared context broken
P4 workflow regression
Video auto-follow regression
Localization broken
Client restart loses authoritative state
Backend reconnect fails
Launcher kills unrelated process
Stage 1 final demonstration cannot complete
```

则 P5 FAIL。

不能用“以后修”改成 CONDITIONAL PASS。

---

# 74. Stage 1 Completion Definition

当 P5 最终满足 PASS 或合理的 CONDITIONAL PASS 后：

正式记录：

```text
STAGE 1 COMPLETE
```

Stage 1 deliverable：

> 单机三屏无人机安防指挥系统

包含：

```text
Backend
Command
Map
Video
Security Plan workflow
Route editing
Explicit Video Target
Localization
Shared operational context
Unified launch
Automatic monitor placement
Health / presence
Recovery
Unified shutdown
```

---

# 75. Stage 2 Boundary

P5 完成后再进入 Stage 2。

Stage 2 才考虑：

```text
Video client migration to second PC
LAN multi-host deployment
Real RTSP/WebRTC
Jetson
AI detection
AI alert
Real camera
Real UAV
PX4 / MAVLink
Execution safety
```

不要在 P5 提前实现。

---

# 76. Important Engineering Rules

整个任务必须遵守：

### Rule 1

不要为了做 Launcher 重写 P4 Backend。

### Rule 2

不要为了三屏同步引入第二套状态系统。

### Rule 3

不要把 Active UAV 和 Video Target 再耦合。

### Rule 4

不要把 Deploy 变成 Execute。

### Rule 5

不要通过修改测试掩盖 regression。

### Rule 6

不要通过清空 Saved 数据让恢复测试通过。

### Rule 7

不要因为 Client Process 存在就判定 ONLINE。

### Rule 8

不要用强制杀全部 UnrealEditor 作为 shutdown。

### Rule 9

不要修改 P1/P2/P3/P4/main。

### Rule 10

最终结论必须由真实运行证据支持。

---

# 77. Required Final Response

任务完成后回复必须直接给出：

## Final Result

```text
PASS / CONDITIONAL PASS / FAIL
```

## Stage Result

```text
STAGE 1 COMPLETE
```

只有达到完成标准时才允许写 COMPLETE。

然后给出：

```text
Branch
Base HEAD
Final HEAD
git status
Backend tests
HTTP/WS
UE automation
Native acceptance
Launcher result
Three-screen result
Recovery result
Localization result
P4 regression result
Protected refs result
```

同时明确列出：

```text
Known Issues
Retained Issues
Deferred to Stage 2
```

最后提供：

```text
Evidence README
Final Report
```

---

# 78. Start Instruction

现在开始执行 TASK-P5。

第一步不要修改业务代码。

先：

1. 核对 P4 Final HEAD；
2. 核对 protected refs；
3. 建立独立 P5 branch/worktree；
4. 检查 worktree clean；
5. Backend Release baseline build/test；
6. UE5.8 Development Editor baseline build；
7. 运行最小 P4 regression；
8. 记录 baseline Evidence；
9. 然后开始 P5-B Unified Launcher。

不要跳过 baseline。

不要先做 UI 美化。

不要提前进入 Stage 2。

本任务目标只有一个：

> **把目前已经能够工作的 Command / Map / Video 三个客户端，正式收成一套可以一键启动、自动三屏布局、状态可见、故障可恢复、整体可演示的单机三联屏无人机安防指挥系统，并完成 Stage 1 最终验收。**
