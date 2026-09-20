# TASK-P5.1 — Command Center UX & Security Plan Workflow Refactor

## 0. 任务定位

本任务属于 **Stage 1 最终冻结前的 UX 收口任务**。

当前 P5 已完成：

* 单机 Backend + Command + Map + Video 三个独立客户端
* Single-Screen Multi-Window Layout
* Launcher 统一启动、恢复、关闭
* Command / Map / Video 状态同步
* 中英文同步
* Plan / Mission / Route / Deployment 基础业务链路
* Map 航线编辑
* Review / Deploy
* 客户端及 Backend 恢复
* 持久化状态恢复

P5 当前结论为：

**CONDITIONAL PASS — STAGE 1 COMPLETE**

但人工测试发现两个严重 UX 问题：

1. Command 左侧主界面信息结构混乱、操作不直观、缺少明显主次关系。
2. 安保方案新建 → 任务 → Map 编辑 → Review → Deploy 虽然技术链路可运行，但用户无法自然理解当前状态和下一步操作。

因此在正式 Push / Tag Stage 1 前，执行本任务。

---

# 1. 最终目标

本任务不增加新的底层业务能力。

核心目标：

> 将当前“数据结构驱动”的操作方式，重构为“用户工作流驱动”的操作方式。

用户第一次进入系统后，应当能够不依赖说明文档完成：

```text
新建安保方案
    ↓
填写方案信息
    ↓
选择无人机 / 配置任务
    ↓
前往 Map 编辑航线
    ↓
保存 / 完成航线
    ↓
部署前检查
    ↓
部署
```

整个过程中用户必须始终知道：

* 当前正在编辑哪个安保方案
* 当前是哪一个任务
* 当前在哪一步
* 是否有未保存修改
* Map 是否正在编辑
* 下一步应该做什么
* Deploy 是否已经完成
* Deploy 是否意味着执行

---

# 2. 约束

## 2.1 不允许破坏现有 P5 架构

必须继续保留：

```text
Backend
 ├─ Command
 ├─ Map
 └─ Video
```

继续使用：

```text
-ClientRole=Command
-ClientRole=Map
-ClientRole=Video
```

不得：

* 复制 UE 项目
* 新建第二 Backend
* 新建第二 authoritative store
* 修改为单进程 UI
* 删除现有恢复机制
* 破坏 P4/P5 HTTP/WS 协议
* 破坏现有 Launcher
* 修改 main/P1/P2/P3/P4 分支

---

# 3. Git 约束

从当前 P5 Final HEAD：

```text
53f63380cfccf1f42a1e524fd2716a6298039049
```

建立新分支：

```text
feat/triple-screen-p5-1-command-plan-ux
```

建议独立工作树。

不得直接修改：

```text
main
feat/triple-screen-p1-...
feat/triple-screen-p2-...
feat/triple-screen-p3-...
feat/triple-screen-p4-...
feat/triple-screen-p5-stage1-integration
```

完成后：

```text
git status
```

必须 clean。

不得：

* merge
* push
* tag
* reset P5
* stash 覆盖其它任务

直到人工验收完成。

---

# 4. 第一部分：Command 主界面重构

当前 Command 左侧主界面需要重新整理。

目标：

> Command 是整个系统的“业务主控台”，而不是把所有功能简单堆在一个左侧列表。

Command 应承担：

* 系统总览
* 安保方案管理
* 当前任务状态
* UAV 管理
* 告警
* 日志
* Map / Video 工作状态
* 操作入口

但不能让所有功能具有同样视觉权重。

---

# 5. Command 推荐信息架构

Command 重构为：

```text
┌──────────────────────────────────────────────┐
│ Product Header                              │
│ Backend | Command | Map | Video | Language  │
├──────────────┬───────────────────────────────┤
│              │                               │
│ Primary Nav  │ Main Workspace                │
│              │                               │
│ Overview     │                               │
│ Security     │                               │
│ Plans        │                               │
│ UAV Fleet    │                               │
│ Alerts       │                               │
│ Logs         │                               │
│              │                               │
├──────────────┴───────────────────────────────┤
│ Global Status / Operation Hint              │
└──────────────────────────────────────────────┘
```

---

# 6. Command 左侧导航

左侧导航控制在 5 个一级入口。

推荐：

```text
总览
安保方案
无人机
告警
日志
```

英文：

```text
Overview
Security Plans
UAV Fleet
Alerts
Logs
```

不要继续增加大量同级入口。

---

# 7. Overview 总览页

进入 Command 默认显示 Overview。

Overview 重点回答：

> 当前系统现在发生了什么？

建议内容：

```text
系统状态

Backend       READY
Command       READY
Map           READY
Video         READY
```

下面：

```text
当前安保方案

园区夜间安保
DRAFT / DEPLOYED
任务：1
无人机：UAV-01

[打开方案]
```

然后：

```text
无人机

在线：4
执行中：0
告警：1
```

以及：

```text
最近事件

18:42  Plan saved
18:41  UAV-01 selected
18:40  Map ready
```

Overview 不允许变成复杂配置页面。

---

# 8. Security Plans 独立成为核心模块

“安保方案”必须成为 Command 中非常明确的一级业务入口。

页面：

```text
安保方案

[ + 新建安保方案 ]

草稿
园区夜间安保
DRAFT
最后修改 18:42

已部署
园区 A 区安保
DEPLOYED
```

每个 Plan 卡片至少显示：

```text
名称
状态
任务数量
最后修改时间
```

操作：

```text
打开
复制
删除草稿
基于此方案创建新版本
```

DEPLOYED Plan 不允许直接进入编辑状态。

---

# 9. 安保方案核心逻辑重构

现有后台逻辑保留：

```text
Plan
 └ Mission
     └ Route
         └ Revision
```

部署：

```text
Plan
 ↓
Review
 ↓
Deployment
```

但 UI 不再要求用户直接理解所有内部数据对象。

---

# 10. 新建安保方案

用户点击：

```text
+ 新建安保方案
```

弹出：

```text
新建安保方案

方案名称 *
[                    ]

说明
[                    ]

[取消]

[创建并继续]
```

用户不需要手工创建 Mission。

点击：

```text
创建并继续
```

系统必须自动：

```text
Create Plan
Create Mission 01
Set ActivePlan
Set ActiveMission
Enter Plan Workspace
```

Mission 默认名字：

```text
任务 01
```

英文：

```text
Task 01
```

---

# 11. 新建后禁止返回普通列表

这是本任务的重要修改。

当前问题：

用户创建 Plan 后，很难感知自己已经进入编辑状态。

修改后：

```text
New Plan
   ↓
Create
   ↓
直接进入 Plan Workspace
```

不得：

```text
Create
 ↓
回普通列表
 ↓
用户自己再找刚创建的 Plan
```

---

# 12. Plan Workspace

方案打开后，进入专门的：

# 安保方案工作区

顶部必须持续显示：

```text
← 安保方案

园区夜间安保

DRAFT
```

下面必须显示工作流步骤：

```text
① 基本信息
② 任务配置
③ 航线
④ 部署前检查
⑤ 部署
```

当前步骤必须明显。

例如：

```text
① ✓
② ●
③
④
⑤
```

---

# 13. Workflow 状态

建议增加前端工作流状态：

```text
PlanWorkflowState
```

至少包括：

```text
BasicInfo
TaskConfig
RouteEditing
PreDeployReview
Deployed
```

该状态用于 UI。

不得替代 Backend 原有 Plan/Mission/Deployment 业务状态。

---

# 14. 当前 Plan Context

建议显式维护：

```text
ActivePlanId
ActiveMissionId
ActiveRouteId
WorkflowState
IsRouteEditing
HasUnsavedChanges
```

必须可被 Command 和 Map 同步感知。

如果已有 Shared Context 能承载，不应另建第二套 authoritative state。

---

# 15. Step 1 — 基本信息

页面：

```text
基本信息

方案名称
园区夜间安保

描述
夜间巡逻

创建时间
...

[保存]

[下一步：任务配置 →]
```

如果创建时已经填写，直接视为完成。

---

# 16. Step 2 — 任务配置

页面：

```text
任务 01

执行无人机
[ UAV-01 ▼ ]

任务类型
[ 巡逻 ▼ ]

航线
尚未配置

[ + 添加任务 ]

[下一步：编辑航线 →]
```

当前 Stage 1：

默认只需要一个 Task。

允许保留：

```text
+ 添加任务
```

但不要为了本任务增加复杂多任务业务逻辑。

---

# 17. Mission 对用户的表达

后台继续使用：

```text
Mission
```

但 UI 中文统一显示：

```text
任务
```

不要主要显示：

```text
Mission ID
mission-7
route-8
plan-6
```

ID 只允许出现在：

```text
Details
Debug
Developer Info
```

---

# 18. 编辑航线按钮

Step 2 必须提供唯一明显 CTA：

```text
前往地图编辑航线 →
```

英文：

```text
Edit Route in Map →
```

不得要求用户自己切换 Map 后再寻找方案。

---

# 19. Command → Map 编辑请求

用户点击：

```text
前往地图编辑航线
```

必须完成完整链路：

```text
Set Active Plan
 ↓
Set Active Mission
 ↓
Resolve/Create Route
 ↓
Request Route Edit
 ↓
Map receives context
 ↓
Map enters Route Edit Mode
```

如果可以安全实现：

自动将 Map 窗口带到前台。

如果系统环境不允许可靠 focus：

Command 至少显示：

```text
已发送至 Map

[切换到 Map]
```

---

# 20. Command 在 Map 编辑期间的状态

Command 不允许看起来像什么都没发生。

必须显示：

```text
③ 航线编辑

● 正在 Map 中编辑

方案
园区夜间安保

任务
任务 01

无人机
UAV-01

航点
3

最近保存
18:43

[切换到 Map]
```

如果 Map offline：

```text
⚠ Map 客户端离线

当前方案草稿不会丢失。

[重试]
```

---

# 21. 第二部分：Map UX 重构

当前 Map 的最大问题：

> 普通态势浏览和方案航线编辑几乎没有明显区别。

必须明确拆分：

```text
Normal Map Mode
```

和：

```text
Route Edit Mode
```

---

# 22. Normal Map Mode

用于：

* 查看 UAV
* 查看部署
* 查看态势
* 查看路线
* 普通地图操作

顶部：

```text
MAP
Operational Map
```

不显示航线编辑工具。

---

# 23. Route Edit Mode

进入方案编辑后，Map 必须明显改变。

顶部示例：

```text
● 航线编辑中

园区夜间安保

任务 01 · UAV-01

DRAFT
```

必须能够让用户一眼判断：

> 我正在编辑一个安保方案。

---

# 24. Map Route Edit Header

至少显示：

```text
安保方案名称
任务
UAV
Plan 状态
Route 保存状态
```

例如：

```text
园区夜间安保

任务 01
UAV-01

DRAFT

● 已保存
```

或者：

```text
● 有未保存修改
```

---

# 25. Route Editor 工具

Map 编辑状态下增加明确 Route Editor。

至少：

```text
航点列表

航点 01
高度
速度

航点 02
高度
速度

航点 03
高度
速度
```

统计：

```text
航点数量
距离
预计时间
```

操作：

```text
撤销
清空
保存草稿
完成航线编辑
```

---

# 26. 保存草稿与完成编辑必须分离

## 保存草稿

行为：

```text
Save Route Revision
```

然后：

```text
RouteEditMode remains ACTIVE
```

不能退出。

提示：

```text
✓ 已保存
18:43:25
```

---

# 27. 完成航线编辑

点击：

```text
完成航线编辑
```

执行：

```text
Validate
 ↓
Save if dirty
 ↓
Mark Route configured
 ↓
Exit Route Edit Mode
 ↓
WorkflowState = PreDeployReview
```

完成后 Command 必须同步。

---

# 28. 未保存修改保护

若存在：

```text
HasUnsavedChanges = true
```

用户：

```text
返回
关闭编辑
切换方案
```

必须提示：

```text
当前航线存在未保存修改

[继续编辑]

[保存并退出]

[放弃修改]
```

不得静默丢失。

---

# 29. Route Edit 恢复

如果 Map 重启：

必须恢复：

```text
ActivePlan
ActiveMission
Route
Workflow State
```

如果编辑会话已经完成：

不要错误进入编辑模式。

如果编辑尚未完成：

允许继续编辑。

---

# 30. Step 4 — 部署前检查

完成航线后 Command 自动进入：

```text
部署前检查
```

页面建议：

```text
部署前检查

方案
园区夜间安保

任务
任务 01

无人机
UAV-01

航线
3 个航点
2.7 km

状态

✓ 基本信息完整
✓ 任务已配置
✓ UAV 已分配
✓ 航线已配置
✓ Backend READY
✓ Map READY

[返回修改]

[确认并部署 →]
```

---

# 31. Review 逻辑重构

后台可继续使用：

```text
Review
Reviewed
```

但不要把 Review 作为一个用户难理解的独立按钮。

UI 使用：

```text
部署前检查
```

和：

```text
确认并部署
```

后台内部可以：

```text
DRAFT
 ↓
REVIEWED
 ↓
DEPLOYED
```

但用户不需要理解 Reviewed 是什么。

---

# 32. 校验失败

如果配置不完整：

禁止 Deploy。

例如：

```text
✕ 未选择 UAV
✕ 航线为空
```

按钮：

```text
确认并部署
```

必须 Disabled。

并提供直接操作：

```text
[选择无人机]

[编辑航线]
```

---

# 33. Step 5 — Deploy

确认后：

```text
Create Deployment
```

成功显示：

```text
✓ 安保方案已部署

园区夜间安保

Deployment #...

任务
任务 01

无人机
UAV-01

航点
3

当前状态

已部署
尚未执行
```

---

# 34. 必须明确

UI 必须明确写：

```text
部署仅表示配置已生效。

无人机尚未起飞。
任务尚未开始执行。
```

禁止出现任何可能让用户误认为：

```text
Deploy = Start Mission
```

的表达。

---

# 35. 已部署 Plan 编辑规则

DEPLOYED Plan 禁止直接编辑。

打开已部署方案时：

```text
园区夜间安保

DEPLOYED

[查看]

[基于此方案创建新版本]
```

点击：

```text
基于此方案创建新版本
```

执行：

```text
Copy Plan
 ↓
Create new Draft
 ↓
new revision / draft
 ↓
Enter Plan Workspace
```

不得直接改变历史 Deployment 对应的配置。

---

# 36. Command UI 视觉原则

本任务要求同时优化 Command UI。

必须做到：

## 明确视觉层级

一级：

```text
当前工作内容
```

二级：

```text
状态
```

三级：

```text
辅助信息
```

不要所有文字/卡片/按钮具有相同权重。

---

# 37. 主操作按钮规则

每一个页面最多一个 Primary CTA。

例如：

任务配置：

```text
前往地图编辑航线 →
```

部署检查：

```text
确认并部署 →
```

其他按钮使用 Secondary / Tertiary。

---

# 38. 降低 UI 密度

当前 Command 存在 dense tiled UI 问题。

本任务需要：

* 减少同时显示的信息
* 去掉重复状态
* 去掉开发者式 ID
* 增加分组
* 增加 whitespace
* 统一按钮尺寸
* 统一卡片 spacing
* 统一标题层级

---

# 39. 响应式要求

Single-Screen Multi-Window Layout 下：

Command 当前默认约占屏幕左半边。

必须保证：

```text
约 1000 × 1100
```

窗口下：

核心工作流可正常使用。

不得要求最大化后才能完成：

```text
New Plan
Task
Route request
Review
Deploy
```

Focus/maximize 可以用于复杂查看，但不能成为基本操作前提。

---

# 40. Localization

所有新增字符串必须：

```text
zh-Hans
en
```

必须进入现有 ProductText / FText localization 体系。

不得硬编码中英文混用。

必须测试：

```text
Command → 中文
```

三客户端同步。

以及：

```text
Video → English
```

三客户端同步。

语言切换不得：

* 重置 ActivePlan
* 重置 Route
* 退出 Plan Workspace
* 清除编辑状态

---

# 41. 状态恢复

必须验证：

### Command 重启

恢复：

```text
Active Plan
Current Step
Mission
Route state
```

### Map 重启

恢复：

```text
当前 Route context
```

### Backend 重启

恢复后：

```text
Command / Map / Video
```

重新 hydrate。

方案不得丢失。

---

# 42. Video 边界

Video 不参与 Plan 创建。

不要增加：

```text
Plan 编辑
Mission 编辑
Route 编辑
Deploy
```

到 Video。

Video 只保持当前已有职责：

```text
查看视频
选择 Video Target
```

安保方案中 UAV assignment 与 Video Target 必须继续保持解耦。

例如：

```text
Active UAV = UAV-02
Video Target = UAV-01
```

必须仍然允许。

---

# 43. Logs

Command Logs 页面保留。

新增工作流事件：

```text
PLAN_CREATED
PLAN_OPENED
MISSION_CONFIGURED
ROUTE_EDIT_STARTED
ROUTE_SAVED
ROUTE_EDIT_COMPLETED
PLAN_REVIEW_READY
PLAN_DEPLOYED
PLAN_VERSION_CREATED
```

日志使用用户友好的文本。

---

# 44. Alerts

业务流程错误进入 Alerts / inline error。

例如：

```text
Map unavailable
Backend offline
Route invalid
UAV unavailable
Deploy failed
```

不要只写 console log。

---

# 45. 不允许做的范围

本任务禁止加入：

```text
PX4
MAVLink
真实无人机
起飞
Start Mission
执行安全状态机
Jetson
AI detection
RTSP
WebRTC
第二台 PC
LAN 多主机
真实 Camera
```

这些全部属于 Stage 2。

---

# 46. 自动测试要求

必须保留并重新运行：

## Backend

当前基线：

```text
60 discovered
58 PASS
2 historical failures
```

允许保留：

```text
SegmentDistanceTest.Crossing
AssemblyPlannerTest.ConflictHeightSeparation
```

不得新增失败。

---

# 47. P4/P5 回归

必须重新执行：

```text
P4 HTTP/WS
P5 protocol/presence
Launcher
UE automation
```

目标至少保持：

```text
P4 HTTP/WS        21/21
P5 protocol       8/8
Launcher          5/5
UE automation     5/5
```

如数量因新增测试增加，应报告新总数。

---

# 48. 新增自动测试建议

至少覆盖：

```text
Create Plan → auto Mission
Open Plan Workspace
Set ActivePlan
Request Map Route Editing
Save Route without leaving Edit Mode
Finish Route Editing
PreDeployReview
Deploy
Deployed Plan cannot edit directly
Copy deployed plan creates Draft
Language change preserves workflow
Command restart preserves workflow
Map restart restores edit context
```

---

# 49. Native 人工验收流程 A

从：

```text
System OFF
```

开始。

启动：

```text
DroneSecurityLauncher.cmd
```

确认：

```text
Backend READY
Command READY
Map READY
Video READY
```

---

# 50. Native 人工验收流程 B

Command：

```text
安保方案
 ↓
+ 新建安保方案
```

创建：

```text
P5.1 UX Test Plan
```

点击：

```text
创建并继续
```

必须：

```text
直接进入 Plan Workspace
```

不得返回普通列表。

---

# 51. Native 人工验收流程 C

确认：

```text
Task 01
```

已经自动存在。

选择：

```text
UAV-01
```

点击：

```text
前往地图编辑航线
```

---

# 52. Native 人工验收流程 D

Map 必须明显进入：

```text
航线编辑中
```

并显示：

```text
P5.1 UX Test Plan
Task 01
UAV-01
DRAFT
```

---

# 53. Native 人工验收流程 E

增加：

```text
Waypoint 1
Waypoint 2
Waypoint 3
```

点击：

```text
保存草稿
```

必须仍然停留在 Edit Mode。

然后修改第四个点。

触发退出。

必须出现未保存保护。

---

# 54. Native 人工验收流程 F

继续编辑。

点击：

```text
完成航线编辑
```

回 Command。

Command 必须显示：

```text
部署前检查
```

并列出：

```text
Plan
Task
UAV
Route
Waypoints
System readiness
```

---

# 55. Native 人工验收流程 G

点击：

```text
确认并部署
```

必须：

```text
DEPLOYED
```

且显示：

```text
尚未执行
无人机尚未起飞
```

---

# 56. Native 人工验收流程 H

打开已部署 Plan。

不得允许直接编辑。

点击：

```text
基于此方案创建新版本
```

必须产生新的：

```text
DRAFT
```

并进入 Plan Workspace。

---

# 57. Native 人工验收流程 I

中文：

```text
Command → 中文
```

确认：

```text
Command
Map
Video
```

全部同步。

然后：

```text
Video → English
```

确认全部恢复英文。

方案编辑上下文不得丢失。

---

# 58. Native 人工验收流程 J

Map 编辑期间：

```text
Stop Map
```

Command 必须明确显示：

```text
Map offline
```

重启 Map。

必须恢复 Route Edit Context。

---

# 59. Native 人工验收流程 K

重启 Command。

必须恢复：

```text
Plan Workspace
Current Step
Active Mission
Route status
```

---

# 60. Native 人工验收流程 L

停止 Backend。

确认：

```text
DEGRADED / OFFLINE
```

重新启动。

三个客户端自动重新 hydrate。

方案不得丢失。

---

# 61. Native 人工验收流程 M

统一：

```text
Shutdown System
```

确认没有 owned processes 残留。

重新启动。

已保存方案仍然存在。

---

# 62. 视觉验收

必须提供未拼接的原始截图：

```text
Command Overview
Security Plan List
New Plan
Plan Workspace
Task Config
Command Route Editing State
Map Route Edit Mode
Unsaved Changes Dialog
Pre-Deploy Review
Deployment Success
Deployed Plan Read-Only
Create New Version
Chinese
English
```

---

# 63. Evidence

建立：

```text
Evidence/TASK-P5.1/
```

建议：

```text
README.md

automation/
native/
recovery/
localization/
visual/
closure/
```

---

# 64. 最终报告

创建：

```text
Docs/TASK-P5.1-CommandPlanUXClosure.md
```

必须说明：

```text
Final Result
Branch
Base
Final HEAD
Backend tests
Protocol tests
Launcher tests
UE tests
Native tests
Localization
Recovery
Known retained issues
Git protection
```

---

# 65. 最终验收标准

只有以下全部满足，才可以：

```text
PASS / CONDITIONAL PASS
```

### UX

* 新建 Plan 后用户直接进入工作区
* 用户始终知道当前 Plan
* 用户始终知道当前步骤
* Mission 默认自动创建
* Map 编辑状态明显
* Map 显示 Plan/Task/UAV
* Save 不退出
* Finish 才退出
* Command 能显示 Map 正在编辑
* Review 变为部署前检查
* Deploy 与 Start Mission 明确区分
* DEPLOYED 不允许直接修改

### UI

* Command 导航明显
* Overview 简洁
* Security Plans 独立
* Primary CTA 明确
* 约 1000×1100 Command 窗口可完成基本流程
* 无明显 dense tiled UX

### Architecture

* Backend authoritative state 不变
* 三客户端独立
* Launcher 不退化
* P4/P5 协议不退化
* Restart/recovery 不退化
* Localization 不退化

### Git

* P5 原分支不变
* main/P1–P4 不变
* 当前工作区 clean
* 未 push
* 未 merge
* 未 tag

---

# 66. 任务完成后的停止条件

完成开发、测试和 Evidence 后：

**停止。**

不要：

```text
push
merge
tag
进入 Stage 2
```

输出最终验收结果并等待人工体验确认。

人工确认 Command UX 和安保方案流程可接受后，再决定是否：

```text
Push
Tag stage1-final
```

---

# 67. 设计原则

本任务最重要的原则：

> 不要让用户操作数据库对象。

用户操作的是：

```text
安保方案
任务
无人机
航线
部署
```

不是：

```text
Plan ID
Mission ID
Route ID
Revision ID
Deployment ID
```

内部结构可以复杂。

用户流程必须简单。

最终 Stage 1 的安保方案流程应该让第一次接触系统的用户，不需要阅读开发文档，也能自然完成：

```text
新建
→ 配置
→ 编辑航线
→ 检查
→ 部署
```

这就是 TASK-P5.1 的最终验收目标。
