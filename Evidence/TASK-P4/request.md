# TASK-P4 — Security Plan Workflow / Localization / Explicit Video Target

## 0. 任务目标

在现有 P1/P2/P3 三客户端架构基础上完成 P4。

P4 的核心目标不是增加真实无人机执行能力，而是把当前已有的 Security Plan 基础改造成一套真正适合操作者使用的三联屏业务工作流，并完成三客户端统一中英文以及 UAV Selection / Video Target 的正式解耦。

P4 完成后，操作者应能够顺畅完成：

**创建安保方案 → 添加任务 → 分配 UAV → 在 Map 编辑航线 → 配置检查 → 人工审阅 → 业务部署 → 查看已部署内容 → 明确调取视频。**

同时：

**Command = 业务操作主控。**

**Map = 空间规划与航线编辑。**

**Video = 视频查看。**

**Backend = 状态、权限、版本和持久化权威。**

P4 Deploy 依然只代表：

> 当前版本的 Security Plan 已被业务确认并冻结。

不得启动无人机，不得调用 `/api/arrays`，不得调用 PX4/MAVLink、ExecutionEngine、AssemblyController、起飞、移动、返航或任何真实执行链路。

---

# 1. Git 与基线

首先执行完整 Recon。

确认当前基线来自已完成的：

```text
feat/triple-screen-p3-security-plan
```

必须：

* `git status` clean；
* 记录 P3 当前 HEAD；
* 确认 main、P1、P2、P3 protected refs；
* 不修改 P3；
* 不 merge main；
* 不 reset/stash/drop 用户已有工作；
* 不删除已有 Evidence；
* 不修改无关业务代码。

从当前 P3 最终 HEAD 创建：

```text
feat/triple-screen-p4-workflow-localization
```

P4 所有实现、测试、Evidence 和文档都在独立 P4 worktree / branch 完成。

正式开发前先执行：

* Backend Release baseline build；
* UE5.8 Development Editor baseline build；
* 当前 P3 核心自动化；
* 记录现有失败，不得通过降低测试标准让历史失败消失。

P3 已知问题继续保留，不能因为 P4 通过就自动关闭。

---

# 2. P4 最终边界

## 2.1 P4 必须完成

P4 分为四个交付域：

### P4-A — Security Plan Workflow

重构 Plan / Mission / Route / Review / Deployment 用户工作流。

### P4-B — Unified Localization

Command / Map / Video 三客户端统一中英文，并由 Backend 同步工作席语言偏好。

### P4-C — Explicit Video Target

将视频目标从 Global Active UAV 中彻底拆出。

### P4-D — Integration & Acceptance

完成 Backend、HTTP/WS、UE 自动化及三原生客户端验收。

---

## 2.2 P4 明确不做

本轮严禁顺手实现：

* Security Plan 实际执行引擎；
* MissionRun / Run 完整运行状态机；
* 起飞、降落、返航、中止飞行；
* PX4 / MAVLink 新执行接口；
* `/api/arrays` 与 Security Plan Deploy 串联；
* UAV 自动调度和跨 Plan 资源预留；
* 多 UAV Mission；
* 禁飞区；
* 地形净空；
* 时空航线冲突求解；
* 续航计算；
* 复杂安保区域多边形编辑；
* AI 识别；
* 云台视觉跟踪；
* 视频录像；
* 视频回放；
* 真正 4/6 路并发播放器；
* 完整 Alarm 工单系统；
* After Action Review；
* Launcher / 三屏自动摆窗；
* 坐标体系大改；
* 为掩盖现有 3D 遮挡而修改航线高度。

不要扩大 P4。

---

# 3. 业务模型

保留现有：

```text
Security Plan
 └─ Mission
     ├─ assigned_uav_id
     └─ Route
         └─ Waypoints
```

UAV 仍然是 Registry 中的独立资源。

禁止把 UAV 变成 Mission 的子资产。

删除 Mission 不删除 UAV。

当前每个 Mission：

```text
0..1 UAV
0..1 Route
```

继续保持。

---

# 4. Plan 状态设计

Plan 正式业务状态继续只有：

```text
DRAFT
READY
DEPLOYED
```

不要增加：

```text
RUNNING
PAUSED
COMPLETED
ABORTED
```

这些属于后续 Runtime。

含义：

### DRAFT

配置尚不完整，或者内容修改后需要重新检查。

### READY

当前内容版本已经通过 Backend 配置检查。

注意：

READY 仅表示：

```text
CONFIGURATION VALID
```

不能在 UI 中描述成：

```text
SAFE TO FLY
FLIGHT READY
安全可执行
飞行安全检查通过
```

### DEPLOYED

该版本完成业务部署并冻结。

UI 必须明确显示：

```text
已部署，执行尚未启动
Deployed. Execution has not started.
```

---

# 5. 引入 content_revision

不要继续用当前 transaction/version 直接承担 Review 有效性。

在 Plan 增加：

```text
content_revision
```

规则：

创建 Plan：

```text
content_revision = 1
```

以下操作成功时必须 `+1`：

```text
update_plan
add_mission
rename_mission
delete_mission
assign
unassign
save_route
copy 后的新草稿初始化
```

以下操作不得增加 `content_revision`：

```text
select
validate
review
deploy
language change
video target change
```

Plan 原有：

```text
version
```

可以保留作为兼容/事务版本，但 Review、Validation 和 Deployment 的业务一致性必须绑定：

```text
content_revision
```

---

# 6. Validation 与 Review 分开

## 6.1 Validation

Backend 继续负责配置检查。

保留当前检查，并输出结构化 issue：

```text
mission_id
code
params
```

至少保留：

```text
NO_MISSIONS
NO_UAV_ASSIGNED
INVALID_UAV
UAV_ALREADY_ASSIGNED
ROUTE_MISSING
ROUTE_TOO_SHORT
INVALID_WAYPOINT
```

Plan Validation 增加：

```text
validated_content_revision
```

只有：

```text
validation.ready == true
AND
validated_content_revision == content_revision
```

Plan 才能显示 READY。

任何内容修改：

* Plan → DRAFT；
* Validation 失效；
* Review 失效。

---

## 6.2 Review

新增正式 Backend action：

```text
review
```

只能由 Command 发起。

Review 成功条件：

* Plan 存在；
* Plan 未 DEPLOYED；
* Plan 状态 READY；
* validation 对应当前 `content_revision`；
* 不存在活动 Route Edit Session；
* Backend 当前复检通过。

记录：

```text
review:
{
    content_revision,
    reviewed_at,
    reviewed_by
}
```

不要增加 `REVIEWED` Plan 状态。

Review 是独立记录。

UI 应显示：

```text
已审阅当前版本
Reviewed revision N
```

内容发生变化后自动失效。

---

# 7. Deployment Record

当前 Deploy 不能再只有 status 修改。

新增：

```text
deployments
```

每次成功部署创建独立：

```text
deployment-N
```

最少包含：

```text
id
plan_id
content_revision
deployed_at
deployed_by
reviewed_by
reviewed_at
snapshot
```

`snapshot` 必须固定本次部署的：

* Plan；
* Mission；
* UAV assignment；
* Route；
* Waypoints；
* 关键 Route 参数。

部署记录必须独立于之后的新草稿。

Deploy 前 Backend 必须再次检查：

```text
Plan READY
validation revision == content_revision
review revision == content_revision
no active edit session
expected version valid
```

然后一个原子 transaction 内：

1. 创建 Deployment；
2. Plan → DEPLOYED；
3. 所有 Mission → DEPLOYED；
4. 写 `PLAN_DEPLOYED`；
5. 持久化；
6. 广播。

任何一步失败：

**完整状态保持原样。**

---

# 8. 已部署方案修改方式

DEPLOYED Plan 全部只读。

不得原地修改。

新增：

```text
copy_plan
```

主要由 Command 使用。

行为：

```text
DEPLOYED Plan
→ Copy as Draft
→ 新 Plan ID
→ 新 Mission IDs
→ 新 Route IDs
→ 深拷贝航线
```

新草稿：

```text
status = DRAFT
content_revision = 1
review = null
deployment = null
```

保留：

```text
source_plan_id
source_deployment_id
```

修改新草稿绝不能修改原部署内容。

---

# 9. 客户端职责重构

## 9.1 Command

Command 成为 Security Plan 业务操作主控。

Command 必须可以：

* 新建 Plan；
* 修改 Plan Name；
* 修改 Description；
* 查看全部 Missions；
* 添加 Mission；
* 重命名 Mission；
* 删除 Mission；
* Assign UAV；
* Unassign UAV；
* 查看 Route 配置状态；
* 发起“在 Map 编辑”；
* Validate；
* 查看逐 Mission Validation Issue；
* Review；
* Deploy；
* 查看 Deployment Record；
* 对 DEPLOYED Plan 执行 Copy as Draft。

Command 不允许：

* 拖航点；
* 添加航点；
* 删除航点；
* 修改 Route geometry。

---

## 9.2 Map

Map 只负责空间操作。

Map 必须可以：

* 查看当前 Plan / Mission；
* 查看该 Mission planned UAV；
* 查看保存航线；
* 开始 Route 编辑；
* 添加 waypoint；
* 移动 waypoint；
* 删除 waypoint；
* Clear Route；
* Save Route；
* Discard Route Draft；
* Focus Route；
* 查看 Validation 对应的空间问题。

Map 不允许：

* 创建 Plan；
* 修改 Plan name；
* 创建/删除 Mission；
* Assign UAV；
* Review；
* Deploy；
* Copy deployed Plan。

---

## 9.3 Video

Video 只负责：

* 浏览可用 UAV；
* 明确指定 Video Target；
* 播放当前 Video Target；
* 查看媒体连接状态；
* Retry；
* 4/6/Focus 布局；
* `SET AS ACTIVE UAV`。

Video 不允许修改：

* Plan；
* Mission；
* UAV assignment；
* Route；
* Deploy。

---

# 10. Backend Security Plan 权限矩阵

重构当前 `PlanRequest()` 权限。

Command 允许：

```text
create_plan
update_plan
add_mission
rename_mission
delete_mission
assign
validate
review
deploy
copy_plan
select
request_map_route_edit
```

Map 允许：

```text
select
validate
begin_route_edit
mark_route_dirty
save_route
discard_route_edit
```

Video：

```text
所有 Security Plan mutation = 403
```

Command 不允许：

```text
save_route
begin_route_edit
mark_route_dirty
discard_route_edit
```

Map 不允许：

```text
create_plan
update_plan
add_mission
rename_mission
delete_mission
assign
review
deploy
copy_plan
```

禁止通过放宽 generic OperationalContext PATCH 来实现。

继续保持 Backend 权威角色校验。

---

# 11. Route Draft / Edit Session

这是 P4 的重点。

当前 UE Route 草稿存在 `EditingPaths` 中，Backend 不知道 Map 是否有未保存修改。

P4 增加轻量的 Backend Route Edit Session。

不要实现多人协同编辑器。

最小结构：

```text
edit_session_id
plan_id
mission_id
owner_instance_id
owner_client_id
base_content_revision
state
started_at
last_seen_at
lease_expires_at
```

state：

```text
EDITING
DIRTY
SAVING
```

---

## 11.1 begin_route_edit

Map 开始编辑 Mission Route 前必须调用：

```text
begin_route_edit
```

Backend：

* 验证 Plan/Mission；
* 验证不是 DEPLOYED；
* 验证 revision；
* 验证没有其他有效 session；
* 创建 session；
* 返回 `edit_session_id`。

成功后 UE 才进入真正可修改状态。

---

## 11.2 DIRTY

第一次发生：

* add waypoint；
* move waypoint；
* delete waypoint；
* clear route；
* 修改 route 参数；

必须将本地状态变成：

```text
DIRTY
```

并通知 Backend：

```text
mark_route_dirty
```

不要每一帧拖动都发送。

只需要从 clean → dirty 时通知一次。

---

## 11.3 Save

Save Route 必须：

```text
DIRTY
→ SAVING
→ Backend ACK
→ CLEAN / editor closed
```

只有 Backend 成功确认后：

```text
SetMissionPathEditing(false)
```

严禁像 P3 一样：

**请求还没成功就退出编辑模式。**

保存失败：

```text
DIRTY 保留
Route Actor 保留
Waypoints 保留
Undo 状态保留
编辑器保持打开
```

VERSION_CONFLICT：

* 不覆盖本地 Draft；
* UI 显示冲突；
* 给操作者“放弃本地 / 重新加载”明确操作；
* 不允许静默覆盖。

---

## 11.4 Discard

新增：

```text
DISCARD CHANGES
```

操作者确认后：

* 结束 Edit Session；
* 丢弃本地 Draft；
* 重新载入 Backend authoritative Route。

---

## 11.5 离开保护

以下操作在 DIRTY 状态下必须弹出：

```text
Save
Discard
Cancel
```

包括：

* 切换 Mission；
* 切换 Plan；
* 关闭 Plan route editor；
* 响应远端 Map Edit Request；
* 尝试退出相关页面。

禁止静默丢失。

---

## 11.6 Session Lease

Edit Session 不永久锁死。

复用现有客户端在线/WS 心跳。

使用有限 lease。

推荐：

```text
15~30 s
```

同 `instance_id` 重连后可恢复 session。

进程真正消失且 lease 到期后，Backend 自动清理。

Review / Deploy 遇到有效 Edit Session：

```text
409 PLAN_EDIT_IN_PROGRESS
```

返回：

```text
mission_id
owner_client
state
```

不要把这套机制扩成多人实时协同。

---

# 12. Command UI 重构

不要继续把所有逻辑堆进当前 `USecurityPlanPanel`。

拆职责。

推荐新建：

```text
USecurityPlanWorkspaceWidget
UPlanMissionListWidget
UPlanReviewWidget
UMapMissionRouteWidget
```

实际类名可以根据现有代码风格调整，但必须实现职责拆分。

Command Center 推荐呈现：

```text
SECURITY PLAN

[方案列表 / 当前方案]

Plan Information
Name
Description
Status

MISSIONS
Mission A   UAV-01   Route Ready
Mission B   UAV-02   Route Missing
Mission C   Not Assigned

[+ ADD MISSION]

Configuration
2 / 3 UAV assigned
1 / 3 routes ready

[CHECK CONFIGURATION]

Review & Deployment
[REVIEW]
[DEPLOY]
```

每个 Mission 行必须明确下一步，例如：

```text
缺少 UAV → Assign
缺少 Route → Edit on Map
配置完成 → Ready
```

不能再主要依赖两个 ComboBox 猜层级关系。

---

# 13. “在 Map 编辑”跨屏流程

Command Mission 行新增：

```text
EDIT ROUTE ON MAP
在地图编辑航线
```

它不是简单改变语言或依赖共享状态刷新。

应发送明确的：

```text
MapRouteEditRequested
```

至少包含：

```text
request_id
plan_id
mission_id
requested_by
timestamp
```

Backend 必须先验证：

* Plan/Mission 存在；
* Plan 非 DEPLOYED；
* Map Client 在线。

Map 收到后：

1. 切换到该 Plan/Mission；
2. Focus route / mission；
3. 打开 Route Editor；
4. 调用 begin_route_edit；
5. 成功后进入编辑。

如果 Map Offline：

Command 明确显示：

```text
Map client unavailable
地图客户端未连接
```

不要假装成功。

---

# 14. Map UI 整理

当前 Map 顶栏旧 Path 工具与 PLAN EDITOR 重叠。

P4 必须整理。

普通 Operations 模式：

保留必要的：

```text
2D
3D
Locate
```

Legacy 路径工具如果仍需要保留：

放到明确的：

```text
LEGACY PATH TOOLS
```

或折叠区域。

Mission Route Edit 模式时，只显示：

```text
Plan / Mission
Assigned UAV
Waypoint count
Edit status

ADD WAYPOINT
DELETE WAYPOINT
CLEAR ROUTE
SAVE
DISCARD
EXIT
```

不得同时显示：

```text
Plan
Confirm
Save route
PLAN EDITOR
```

两套业务语义。

---

# 15. Review UI

Command 增加完整审阅页。

必须展示：

```text
Plan name
Description
content_revision
Mission count
```

每个 Mission：

```text
Mission name
Assigned UAV
Route waypoint count
Route status
关键参数摘要
```

还需：

```text
Blocking Issues
Warnings
Map view / locate entry
```

审阅确认文案必须明确：

```text
本次审阅确认当前方案配置版本。
部署只会冻结业务配置，不会启动无人机。
```

English：

```text
This review confirms the current plan configuration.
Deployment freezes the business configuration and does not start aircraft execution.
```

---

# 16. Deploy UX

DRAFT 状态：

不要一直显示可点击 Deploy。

显示：

```text
CHECK CONFIGURATION
```

READY 但未 Review：

显示：

```text
REVIEW PLAN
```

Review 有效后：

显示：

```text
DEPLOY PLAN
```

Deploy 最终确认面板内才出现：

```text
CONFIRM DEPLOY
CANCEL
```

成功后：

```text
DEPLOYED
Execution has not started.
```

并进入只读详情。

---

# 17. Localization 架构

P4 必须使用 UE 正式 Localization 体系。

禁止长期维护：

```cpp
if (Chinese) ...
else ...
```

禁止继续大量新增：

```cpp
FText::FromString(TEXT("硬编码产品文案"))
```

---

## 17.1 支持语言

正式支持：

```text
zh-Hans
en
```

---

## 17.2 统一文本来源

建立一个产品级 Localization Target。

建议 Namespace / Group：

```text
Common
Command
Map
Video
Plan
Errors
Events
```

共享术语必须只有一个权威 Key。

例如：

```text
Common.UAV
Plan.Title
Plan.Mission
Plan.Deploy
Plan.Ready
Plan.Deployed
Video.Open
Video.Target
Log.Title
Common.Save
Common.Cancel
```

产品固定文本使用稳定 Key。

动态显示使用：

```text
FText::Format
```

用户输入：

```text
Plan name
Mission name
Description
备注
```

保持原文，不翻译。

ID 不翻译：

```text
UAV-01
plan-12
mission-14
route-15
deployment-18
```

---

# 18. UI Language Subsystem

新增统一：

```text
UUILanguageSubsystem
```

或等价 GameInstanceSubsystem。

职责：

* 保存当前确认语言；
* 应用 UE Current Language；
* 处理远端语言更新；
* 广播 `OnLanguageChanged`；
* 提供共享 FText / Formatting 辅助；
* 不操作业务状态。

语言变化禁止：

* 重建 GameInstance；
* 重开关卡；
* Backend reconnect；
* Registry 重建；
* 改变 Active UAV；
* 改变 Plan/Mission selection；
* 重载 Route；
* 清空 Route Draft；
* 重建 Video Browser；
* 重设 Video URL；
* 修改 Plan revision；
* 使 Review 失效。

只刷新显示文本和必要布局。

---

# 19. Backend UI Preference Context

不要把 language 加入现有 OperationalContext。

新建独立 Backend Store：

```text
UIPreferenceStore
```

建议持久文件：

```text
ui_preferences.json
```

结构：

```text
language
ui_preferences_version
updated_at
updated_by
```

接口：

```text
GET   /api/ui-preferences
PATCH /api/ui-preferences
```

PATCH：

```text
instance_id
language
```

三个 ONLINE client：

```text
Command
Map
Video
```

都允许修改 Language。

只允许：

```text
zh-Hans
en
```

相同值请求：

* 不增加 version；
* 不广播重复事件。

变更后：

```text
UIPreferencesChanged
```

通过已有 WS 广播。

不要创建第二条 WebSocket。

客户端拒绝旧 version。

新加入和重连客户端必须恢复 Backend 最新语言。

Backend 不可达时：

* 保持最后确认语言；
* 显示同步失败；
* 不积压离线语言修改；
* 重连后不得用旧本地选择覆盖其他客户端的新语言。

---

# 20. 中英文覆盖范围

至少覆盖当前三个客户端可达产品 UI：

### Command

* Header；
* Fleet；
* Alert；
* Tactical Data；
* Security Plan；
* Mission；
* Review；
* Deploy；
* Operational Log；
* Button；
* Empty State；
* Status；
* Error。

### Map

* Header；
* Toolbar；
* 2D/3D；
* Route Editor；
* Plan/Mission；
* Route state；
* Error；
* Save/Discard；
* Status。

### Video

* Header；
* Aircraft List；
* Primary Video；
* Status；
* 4/6/Focus；
* Video Target；
* Set Active；
* Retry；
* Reserved / no source / connecting / playing / stalled 等。

### Plan Validation

所有 Issue Code 转为本地化说明。

例如：

```text
ROUTE_TOO_SHORT
```

UI 不应只直接显示错误码。

应显示：

中文：

```text
航线至少需要 2 个航点
```

English：

```text
Route requires at least 2 waypoints.
```

诊断区域可以保留 Code。

---

# 21. 字体与布局

配置支持简体中文的产品字体 / Composite Font fallback。

要求：

* 中文无缺字；
* 中英文按钮不会明显截断；
* ComboBox 不因为翻译变化错误触发 OnSelectionChanged；
* 语言刷新不回写业务选择；
* 长文本允许 wrap；
* zh-Hans 和 en 都要打包。

验收必须在实际运行的 packaged/game client 或同等原生客户端验证。

不能只看 Editor Designer。

---

# 22. Operational Event Localization

当前事件模型已有：

```text
Category
Level
Source
EventType
TargetId
Message
```

继续复用。

但 P4 开始：

**EventType / code / params 是显示语义权威。**

Message 退化为：

```text
fallback_message
diagnostic_message
```

不要解析英文 Message 来做翻译。

Plan Backend event 建议结构：

```text
event_type
category
target_id
params
timestamp
source
message   // fallback
```

例如：

```text
event_type = UAV_ASSIGNED

params:
{
  uav_id: UAV-01,
  mission_name: East Perimeter
}
```

客户端根据当前语言生成：

中文：

```text
UAV-01 已分配至任务 East Perimeter
```

English：

```text
UAV-01 assigned to mission East Perimeter
```

语言切换后：

**现有结构化 Event 必须能够重新生成当前语言显示。**

无法结构化的历史 P1/P2 message：

允许继续显示原始文本。

不要为了翻译旧日志解析自然语言。

---

# 23. Explicit Video Target

删除产品层面“持续跟随 Global UAV”的语义。

不要再使用：

```text
active_uav_id
+
bFollowTarget
```

决定播放器来源。

新增独立：

```text
VideoViewStore
```

Backend 持久化：

```text
video_target_uav_id
video_view_version
updated_at
updated_by
```

建议文件：

```text
video_view.json
```

接口：

```text
GET   /api/video-view
PATCH /api/video-view
```

三个 ONLINE Client 都可以明确发起视频调取。

目标必须：

* null；或
* Registry 中有效 canonical UAV ID。

同值不增加版本。

广播：

```text
VideoViewChanged
```

继续复用当前 WS。

---

# 24. Video 行为规则

正式定义：

```text
active_uav_id
```

= 三屏当前业务选中 UAV。

```text
assigned_uav_id
```

= Mission 计划 UAV。

```text
video_target_uav_id
```

= 明确调取的视频对象。

三者必须独立。

规则：

### 点击 UAV

只改变：

```text
active_uav_id
```

不得自动切视频。

### 点击 Mission

只改变：

```text
active_security_plan_id
active_mission_id
```

不得自动切视频。

### 点击 Alert

可以改变：

```text
active_alert_id
active_uav_id
operation_mode
```

不得自动切视频。

### 点击“查看视频”

才改变：

```text
video_target_uav_id
```

### Video 点击 UAV Feed

这是一个明确视频操作：

```text
video_target_uav_id = selected UAV
```

不得修改 `active_uav_id`。

### SET AS ACTIVE UAV

Video 中只有这个按钮才允许：

```text
active_uav_id = video_target_uav_id
```

---

# 25. 移除 FOLLOW TARGET 产品行为

现有：

```text
FOLLOW TARGET
FOLLOWING TARGET
bFollowTarget
```

不再作为正式产品行为。

允许为了兼容测试短期保留内部方法，但：

* Product UI 不显示；
* `Refresh()` 不得根据 `active_uav_id` 自动重写视频目标；
* 最终 P4 测试不再依赖 Follow。

替换成：

```text
VIDEO TARGET / 视频目标
```

以及明确的：

```text
OPEN VIDEO / 查看视频
```

---

# 26. Video 媒体状态

继续复用当前单播放器。

必须严格区分：

```text
NO TARGET
NO SOURCE
CONNECTING
PAGE READY / WAITING VIDEO
PLAYING
STALLED
MEDIA ERROR
RETRYING
CLOSED
```

选择了 UAV ≠ 有视频。

有 URL ≠ 正在播放。

浏览器 URL 加载完成 ≠ 有视频帧。

不得制造成功状态。

---

# 27. 三屏“查看视频”入口

至少实现：

Command：

* 当前 Active UAV → 查看视频；
* Mission Assigned UAV → 查看视频；
* Alert → 查看视频。

Map：

* 当前选中 UAV → 查看视频；
* 当前 Mission assigned UAV → 查看视频。

Video：

* Aircraft row / feed card 本身即明确视频调取。

所有入口最终调用同一个 Shared Video View API。

---

# 28. Shared Context 边界

P4 后共享数据分四域：

```text
OperationalContext
SecurityPlan Domain
UIPreference Context
VideoView Context
```

OperationalContext 继续只保存：

```text
active_uav_id
active_alert_id
active_mission_id
active_security_plan_id
active_area_id
operation_mode
```

不要加入：

```text
language
video_target_uav_id
route dirty
```

Client Local State 保持：

```text
scroll
current page
cursor
map camera
local form draft
route actor draft
video layout
```

---

# 29. OperationalContextSubsystem 重构要求

现有 HTTP/WS 连接基础继续复用。

可以扩展现有 subsystem，或增加：

```text
USharedUIPreferenceSubsystem
UVideoViewSubsystem
```

但不要：

* 创建第二条 WS；
* 每个 Store 建独立长连接；
* 让多个 subsystem 重复 reconnect。

建议：

一个 transport / WS connection；

不同 domain 各自：

```text
version
snapshot
delegate
```

例如：

```text
OnPlansChanged
OnUIPreferencesChanged
OnVideoViewChanged
```

迟到 snapshot：

必须被 version 拒绝。

远端 apply：

不得 echo 回 Backend。

---

# 30. Backend 错误结构

新增及现有产品错误应至少返回：

```text
code
params
message
```

其中：

```text
code + params
```

是 UI 本地化依据。

`message` 是 fallback/debug。

至少新增：

```text
PLAN_EDIT_IN_PROGRESS
PLAN_REVIEW_REQUIRED
REVIEW_STALE
VALIDATION_STALE
EDIT_SESSION_CONFLICT
EDIT_SESSION_EXPIRED
MAP_CLIENT_UNAVAILABLE
INVALID_LANGUAGE
INVALID_VIDEO_TARGET
```

UI 不直接把 Backend 英文 message 当主要产品文案。

---

# 31. P4-A 测试

Backend 至少验证：

1. Command 可以创建 Plan。
2. Map 创建 Plan → 403。
3. Command 可以 add/rename/delete Mission。
4. Map 修改 Mission → 403。
5. Command assign/unassign。
6. Map assign → 403。
7. Map save_route。
8. Command save_route → 403。
9. Video 所有 Plan mutation → 403。
10. 内容修改 content_revision +1。
11. validate 不修改 content_revision。
12. review 不修改 content_revision。
13. 内容修改使旧 Validation/Review 失效。
14. Edit Session 存在时 Review 拒绝。
15. Edit Session 存在时 Deploy 拒绝。
16. Save 失败后 Session 保留。
17. Save 成功后 Session 正常结束。
18. lease 过期可恢复操作。
19. Deploy 创建不可变 Deployment Snapshot。
20. repeated deploy 不创建重复 Deployment。
21. Copy as Draft 深拷贝 IDs 和 Route。
22. 修改 Copy 不影响原 deployed Plan。
23. Backend restart 后 Plan/Deployment 持久恢复。
24. `/api/arrays` 调用数保持 0。

---

# 32. P4-B Localization 测试

至少验证：

1. 默认语言可恢复。
2. Command 切中文 → Backend version +1。
3. Map / Video 收到 zh-Hans。
4. Video 切 English → 三端收敛 en。
5. 相同语言请求 version 不增加。
6. stale event 不覆盖新语言。
7. reconnect 恢复最新语言。
8. 新 Client 加入恢复最新语言。
9. Backend offline 时不产生离线待提交语言覆盖。
10. 语言变化不改变 Plan revision。
11. 语言变化不改变 Review。
12. 语言变化不改变 Active UAV。
13. 不改变 Plan/Mission selection。
14. 不关闭 Route Draft。
15. 不重建 Video Browser。
16. 不改变 video_target。
17. 日志结构化事件切换语言后重新显示。
18. 用户输入 Plan/Mission name 不翻译。

---

# 33. P4-C Video 测试

核心回归：

```text
Open UAV-01 Video
→ Select UAV-02
→ Video remains UAV-01
```

然后：

```text
Open UAV-02 Video
→ Video changes to UAV-02
```

还要验证：

1. Mission selection 不切视频。
2. Plan selection 不切视频。
3. Alert selection 不切视频。
4. Alert “View Video” 才切。
5. Video 本地 feed click 改 video target。
6. Feed click 不改 active UAV。
7. SET AS ACTIVE UAV 才改 active UAV。
8. reconnect 恢复 video target。
9. Backend restart 恢复 video target。
10. no source 不报告 PLAYING。
11. page loaded 不报告 PLAYING。
12. 只有真实 frame progress 才 PLAYING。
13. 4/6 布局仍然是 single active player + reserved slots，不伪装成 4/6 并发流。

---

# 34. UE Automation

为 P4 增加真实自动化覆盖。

建议测试类别：

```text
P4.Workflow.*
P4.Localization.*
P4.VideoTarget.*
P4.CrossClient.*
```

至少覆盖：

* Command Plan CRUD；
* Map Route edit guard；
* dirty state；
* save ACK；
* save failure 保留 draft；
* review invalidation；
* deployed read-only；
* copy draft；
* Localization FText refresh；
* ComboBox refresh 不回写选择；
* structured event re-render；
* Video target independence；
* stale version rejection。

所有自动化必须记录：

```text
Test Started
Test Completed
Pass / Fail
```

不能只写“build passed”。

---

# 35. 原生三客户端验收

使用真实 Backend：

```text
Command
Map
Video
```

三个独立 Unreal `-game` process。

不要合成截图。

不要重新绘制假 UI 作为 Evidence。

建议正式执行以下 A–N。

### A — Startup

* 三客户端 ONLINE；
* Command / Map / Video 正常；
* 记录 Backend snapshot。

### B — Create Plan

Command：

```text
创建 Security Plan
```

Command 显示：

```text
No missions / Add first mission
```

Map 同步显示 Plan。

### C — Mission Organization

Command 创建至少 3 个 Mission。

分别分配：

```text
UAV-01
UAV-02
UAV-03
```

重复 assignment 必须被 Backend 拒绝。

Video 不变化。

### D — Map Edit Request

Command 点击：

```text
Edit Route on Map
```

Map 自动定位 Mission 并进入 edit session。

### E — Dirty Draft

Map 修改 Route。

确认：

```text
DIRTY
```

此时 Command Review / Deploy 必须明确阻塞。

### F — Save Failure

制造 VERSION_CONFLICT 或受控失败。

确认：

* Map Draft 仍存在；
* Editor 未关闭；
* Waypoints 未丢失；
* 可以继续操作。

### G — Save Success

成功保存 Route。

重复完成三 Mission Route。

### H — Validation

Command 执行配置检查。

先验证一个失败场景，再修复。

最终：

```text
READY
3 / 3
```

### I — Review

打开完整 Review。

确认每个 Mission：

* UAV；
* Route；
* waypoint count；
* issue；
* revision。

执行 Review。

### J — Review Invalidation

在部署前修改一项内容。

确认：

* Plan → DRAFT；
* 旧 Review 失效；
* Deploy 不可用。

重新 Validate + Review。

### K — Localization

从 Command 切：

```text
zh-Hans
```

三屏切中文。

再从 Video 切：

```text
en
```

三屏切 English。

期间：

* Plan/Mission selection 不变；
* Route Draft 不丢；
* Video 不重载；
* Video Target 不变；
* Browser Generation 不因语言增加。

### L — Video Target

明确调取：

```text
UAV-01
```

再在 Command / Map 选择：

```text
UAV-02
```

Video 必须保持：

```text
UAV-01
```

再明确：

```text
View UAV-02 Video
```

Video 才切 UAV-02。

### M — Deploy

Command Review 当前 revision。

Deploy。

确认：

```text
Plan DEPLOYED
all Missions DEPLOYED
Deployment Record exists
```

UI 显示：

```text
已部署，执行尚未启动
```

确认：

```text
/api/arrays = 未调用
```

### N — Restart / Copy

Backend + 三客户端重启。

恢复：

* deployed plan；
* deployment record；
* language；
* video target；
* active selection。

Command：

```text
Copy as Draft
```

修改新 Draft。

确认原 Deployment Snapshot 完全不变。

---

# 36. Localization Screenshot 要求

至少保留：

Command 中文；
Map 中文；
Video 中文；

Command English；
Map English；
Video English。

截图必须证明：

* 无明显截断；
* 无 key 外露；
* 无乱码；
* 中文字体正常；
* 三端同语言。

---

# 37. Evidence 结构

建立：

```text
Evidence/TASK-P4/
```

建议：

```text
README.md
backend/
protocol/
automation/
native/
localization/
video/
git/
```

保存：

* build logs；
* test reports；
* HTTP request/response；
* WS messages；
* pre/post snapshots；
* restart comparisons；
* native screenshots；
* process command line；
* Git status；
* commit hashes；
* binary hashes，如已有 P3 做法则继续沿用。

所有失败 trial 保留。

修复后不要删除失败证据。

必须明确：

```text
diagnostic failure
historical retained failure
final selected result
```

三者区别。

---

# 38. Commit 规划

建议分阶段提交，不要最后一个巨大 commit。

推荐：

```text
feat(p4): add workflow domain contracts and review records
feat(p4): move plan management to command and route editing to map
feat(p4): add protected route draft sessions
feat(p4): add synchronized ui localization
feat(p4): add explicit shared video target
test(p4): add workflow localization and video acceptance
docs(p4): add acceptance evidence and final report
```

实际可根据实现合理合并，但至少应保持：

```text
Backend
Workflow UI
Localization
Video
Tests
Evidence
```

几个阶段可追溯。

---

# 39. 最终报告

创建：

```text
Docs/TASK-P4-SecurityPlanWorkflowLocalization.md
```

必须包括：

```text
Final Result
Git / Base / Final HEAD
Architecture
Security Plan Workflow
Role Matrix
Content Revision
Validation
Review
Deployment Record
Route Edit Sessions
Command UX
Map UX
Video Target
Localization Architecture
Event Localization
Persistence
HTTP/WS
Automation
Backend Tests
Native Acceptance
Known Issues
Deferred
Evidence Index
Final HEAD
```

结论只能是：

```text
PASS
CONDITIONAL PASS
FAIL
NOT EVALUABLE
```

不得为了好看把未验证项写成 PASS。

---

# 40. 历史问题处理

P3/P2 现有问题不得因为 P4 自动消失。

特别是：

* Standalone 1082 vs 1080；
* 两项 Backend legacy planner failure；
* low-altitude 3D route occlusion；
* XYZ / map 网络依赖；
* 6GB VRAM capacity 风险；
* P3 曾出现的孤立 UE startup assertion；
* 原生工具轴拖动限制。

如果 P4 没有专门修复并重新验收：

继续标记 Retained。

---

# 41. 不允许的实现捷径

禁止：

* 把 language 放进 Plan；
* language change 增加 Plan revision；
* 把 language 直接塞进 `OperationalContext`；
* 把 video target 继续等同 `active_uav_id`；
* 用 Follow Target 伪装 explicit video target；
* 用硬编码 Chinese/English if/else 大量复制文案；
* 通过解析英文 Backend message 做翻译；
* save request 发出后立即关闭 Route Editor；
* 切换 Mission 时静默丢 draft；
* Review 后内容变了仍允许 Deploy；
* DEPLOYED Plan 原地编辑；
* Deploy 调 `/api/arrays`；
* UI 报 READY 就写“安全可执行”；
* 把 4/6 个 UI 卡位说成 4/6 个真实播放器；
* 为了测试通过删除历史失败；
* 用 screenshot 替代真实运行；
* 修改测试期望来隐藏产品错误。

---

# 42. Definition of Done

P4 只有同时满足以下条件才允许收口：

```text
Command = Plan/Mission/UAV 业务主控
Map = Route/Waypoint 空间编辑
Video = 独立视频查看

Plan workflow 顺畅
Dirty Draft 不静默丢失
Validation / Review 分离
Review 与 content_revision 绑定
Deployment 有不可变快照
DEPLOYED 可复制但不可原地修改

zh-Hans / en 三端统一同步
语言切换不改变业务状态
结构化事件可跟随语言重新显示

active_uav_id 与 video_target_uav_id 独立
Selection 不隐式切视频
明确 View Video 才切换

Deploy 不启动无人机
/api/arrays 未被调用

Backend tests PASS（历史失败单独保留）
HTTP/WS P4 protocol PASS
UE P4 automation PASS
三进程原生 A–N 完成
Evidence 完整
Git clean
P3 protected
main protected
无 merge
```

---

# 43. 执行方式

不要只输出设计方案。

直接执行完整 TASK-P4：

```text
Recon
→ Branch
→ Backend
→ Workflow UI
→ Route Draft Protection
→ Localization
→ Video Target
→ Tests
→ Native Acceptance
→ Evidence
→ Git commits
→ Final Report
```

遇到非阻塞问题自行定位并继续。

不要因为局部 UI 或自动化问题停止整个任务。

如果某个验收受工具限制无法执行：

保留真实证据并标记：

```text
NOT EVALUABLE
```

不要伪造通过。

最终回复必须给出：

```text
最终结论
Branch
Base HEAD
Final HEAD
git status
Backend test result
UE automation result
HTTP/WS result
Native A–N result
Localization result
Video target result
Retained issues
Evidence path
Report path
是否修改 main
是否修改 P3
是否 merge
```
