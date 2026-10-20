# Figma 指挥中心 UI 接入说明

设计来源：[无人机安防指挥系统 UI](https://www.figma.com/design/NcGzq4onhsQHsJFAe0JBJe?node-id=2522-143)。读取日期：2026-10-20。目标工程为 `D:\CodexPublish\drone_security`，使用 UE 5.8 原生 UMG / Slate 实现。

本文记录首次 UI 接入与当时验证。后续固定 Mock 起点、屏幕图标、单机选点移动、航线编辑视角和逐任务操作的行为与新验证见 [交互修复记录](UIInteractionFollowup.md)。

首次接入范围是 **Command 指挥客户端**：总览、安保方案、无人机三个主页面，以及设备、告警、日志、新建方案和标签设置弹窗。当时 Map / Video 继续使用原有界面；后续两个客户端的 Figma 接入见 [三客户端接入记录](FigmaThreeClientsIntegration.md)。Command 总览中的二维态势图仍复用 `UCommandTacticalMap`。

## 八个画板与业务关系

| Figma 画板 / 节点 | 原生组件 | 数据与交互 |
| --- | --- | --- |
| 指挥中心－总览 `2522:144` | `UCommandShellWidget::BuildFigmaShell`、`RefreshFigmaShell` | 无人机总数与在线数来自注册表和遥测；告警数来自未处理告警；日志数与最近事件来自事件仓库。统计卡片进入无人机页或对应弹窗。当前方案、当前执行摘要读取共享状态；态势图缩放与跟随操作复用原地图组件。 |
| 总览－在线设备 `2547:3600` | `RefreshFigmaDialog` 的 `devices` 表 | 列出实际注册设备、连接状态、电量与任务状态，未知遥测保留不可用表示。详情动作提交设备选择并进入无人机页；配置占用时显示不可切换的原因。刷新重新投影当前数据。 |
| 总览－告警 `2547:3754` | `RefreshFigmaDialog` 的 `alerts` 表、`UCommandAlertStore` | 告警内容来自现有告警仓库。选择告警使用共享上下文接口，处理动作使用仓库已有 `MarkHandled` 路径；空列表显示设计空状态。 |
| 总览－日志 `2547:3908` | `RefreshFigmaDialog` 的 `logs` 表、`UOperationalEventStore` | 展示实际事件类别、时间和已本地化消息；总览最近事件与完整日志来自同一事件来源。 |
| 指挥中心－安保方案 `2522:285` | `USecurityPlanWorkspaceWidget` | 从后端方案快照生成空状态或方案卡片。打开、复制、新版本、删除草稿沿用现有操作；任务配置、航线编辑、检查、部署和执行页保持可达。 |
| 安保方案－新建方案 `2524:584` | `SecurityPlanWorkspacePresentation.cpp`、现有配置保存流程 | 创建前申请配置租约。名称与多行描述写入同一份未保存草稿，名称为空或仅含空格时禁用创建。提交等待后端 ACK，失败保留输入并显示错误；取消和关闭进入已有未保存确认流程。 |
| 指挥中心－无人机 `2524:676` | `UFigmaFleetWidget`、`UGroupControlWidget` | 从注册表、遥测及共享选择状态生成设备列表、主机与多选状态、遥测和控制区。选择操作等待共享状态确认；视频目标通过独立显式动作提交。 |
| 无人机－标签设置 `2527:950` | `UFigmaFleetWidget::BuildLabelDialog`、`UDroneRegistrySubsystem` | 编辑名称、颜色、字号并预览。取消丢弃弹窗草稿；确认调用既有标签设置接口，广播变化并保存本地注册表。 |

头部语言切换仍使用共享 UI 偏好；系统、Command、Map、Video 在线状态读取同步服务状态，时间显示本机当前时间。设计中的示例编号、数量、时间和电量不作为运行时事实写死。

最近事件显示本地时间，与头部时钟一致；完整告警和日志表明确标注 UTC。视频选择项始终带固定设备 ID，避免可编辑名称重名导致目标混淆。多选变更等待后端确认期间，集群移动按钮与动作入口都暂停接受新提交；既有暂停、恢复和终止操作仍可用。

## 保留的状态边界

- **Backend authority / ACK**：方案和任务变更继续通过 `UOperationalContextSubsystem::SubmitPlan` 及后端版本校验提交，界面使用返回快照更新结果。提交中禁用相关重复操作，失败显示错误，不提前宣告创建、保存或部署成功。
- **配置租约与未保存输入**：继续使用 `begin_configuration` / `end_configuration`、拥有者校验和保存队列。名称、描述、任务与设备分配均参与脏状态判断。新建弹窗关闭时，保存、丢弃、继续编辑确认层位于弹窗之上。活动执行的暂停、恢复、终止入口仍由外层持久控制区提供。
- **Deployment ≠ execution**：部署激活经过检查的配置快照；开始执行仍需要独立的执行操作。已部署方案保持只读，修改走复制或新版本流程。Mock 与真实执行模式继续沿用已有标识。
- **选择 UAV ≠ 视频切换**：列表选择、主机设置和任务分配不写入 `video_target_uav_id`。只有视频选择器的显式打开动作调用 `OpenVideo`。Command 视频区域说明目标和独立 Video 客户端状态，不在 Command 中伪造播放画面，也不以目标已设置推断媒体流已到达。
- **设备占用与航线所有权**：后端 `DRONE_ACTIVE_PLAN_CONFLICT` 及不可用设备的占用上下文继续保留。航线仍采用已有 Route V2 展示所有权规则；本次没有改变 Map Camera Lock 行为。
- **标签是本地展示设置**：名称、颜色和字号经 `SetDroneLabelSettings` 保存到 `Saved/DroneRegistry.json` 的 `label_settings`，加载时恢复。它们不是后端共享的设备身份或任务配置；保存错误沿用注册表日志报告路径。

## 布局与资源

Command 使用 **1440 × 810 逻辑画布**，外层 `UScaleBox` 采用 `ScaleToFit` 等比缩放。页面、弹窗及其命中区域一起缩放；不同窗口比例通过外层黑色背景容纳。配置上下文与活动执行控制出现时，内容区向下让位，工作区保留滚动能力。

`FigmaCommandStyle` 提供 Command 的静态资源映射和后续客户端复用的样式。静态 SVG 使用原始本地资源；来源、哈希、语义映射、UE 原生向量渲染及打包配置见 [FigmaCommandAssets.md](FigmaCommandAssets.md)。截图仅用于视觉比较，不作为界面背景实现；运行时代码不依赖临时 Figma 下载地址。

主要实现入口：

- `Source/UE5DroneControl/Command/CommandShellFigma.cpp`：画布、总览、设备 / 告警 / 日志弹窗。
- `Source/UE5DroneControl/Command/FigmaFleetWidget.cpp`：设备、遥测、标签和显式视频动作。
- `Source/UE5DroneControl/Shared/SecurityPlanWorkspaceWidget.cpp` 与 `SecurityPlanWorkspacePresentation.cpp`：方案列表和创建弹窗。
- `Source/UE5DroneControl/Shared/SecurityPlanWorkspaceConfiguration.cpp`：既有租约、草稿保护和 ACK 保存队列。
- `Source/UE5DroneControl/Tests/FigmaCommandTest.cpp`：渲染自动化与关键交互检查。

## 可复现构建与验证

以下命令在 PowerShell 中执行，工作目录为 `D:\CodexPublish\drone_security`。首次环境准备参见 [First-Run-Windows.md](First-Run-Windows.md)。验证脚本要求正常构建的 `Backend/build/Release/DroneBackend.exe`，并通过 Launcher 配置定位 UE 编辑器；不接受历史或阶段专用后端替代。

```powershell
Set-Location 'D:\CodexPublish\drone_security'

& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
    UE5DroneControlEditor Win64 Development `
    '-Project=D:\CodexPublish\drone_security\UE5DroneControl.uproject' `
    -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles -NoUBA -MaxParallelActions=2
```

新增 Command 设计验证使用 RHI 渲染，不传 `--null-rhi`：

```powershell
python Scripts/PlanConfiguration/verify.py `
    DroneOps.Figma.CommandPresentation --role Command `
    --expected DroneOps.Figma.CommandPresentation=1 `
    --http-port 22582 --ws-port 22583 --timeout 300
```

该测试覆盖三个主导航、实时数量投影、地图缩放、设备 / 告警 / 日志弹窗、创建弹窗、空白名称约束、多行描述与未保存确认、标签取消、主机选择与视频目标独立性，以及英文界面渲染。测试按阶段保存设计画面截图。

配置流程默认回归会分别使用 Command 和 Map 角色：

```powershell
python Scripts/PlanConfiguration/verify.py `
    --http-port 22582 --ws-port 22583 --timeout 300
```

其他相关 Command 回归可按精确测试名串行执行：

```powershell
python Scripts/PlanConfiguration/verify.py `
    DroneOps.P4.Workflow.CommandLiveCRUD `
    DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo `
    DroneOps.P55.ReservationWidgets `
    DroneOps.P2.CommandLayoutSmoke `
    DroneOps.P2.EventLogProjection `
    --role Command --http-port 22582 --ws-port 22583 --timeout 300
```

视频目标独立性回归需要实际 Video 角色：

```powershell
python Scripts/PlanConfiguration/verify.py `
    DroneOps.P4.VideoTarget.ExplicitSelectionAndBrowserIsolation `
    --role Video --http-port 22582 --ws-port 22583 --timeout 300
```

既有 `DroneOps.P2` 回归会自行加载 `/Game/Level/CesiumWorld`；其地图与资源前置条件仍需满足。新增 Figma 验证使用 `/Game/Tests/P55OfflineMap`，避免把外部底图可用性混同为界面验证结果。

每个精确测试默认期望发现、完成并通过 1 项。若使用测试前缀，必须通过 `--expected TEST=COUNT` 提供正确预期数量。脚本为每项测试建立隔离 Mock 后端和运行目录；端口被占用时应选择未占用端口，不接管已有实例。

验证报告保存在 `Evidence/PlanConfiguration/verification-*`，包含执行命令、编辑器日志、UE 自动化报告与发现 / 完成 / 通过 / 失败数量；设计截图位于 `Saved/FigmaQA/<UTC timestamp>`。具体路径以测试日志记录为准。构建退出码为零、编辑器仍在运行或只有截图文件，都不能单独作为测试通过依据。

RHI 自动化截图与组件动作检查应标记为 **rendered automation evidence**。`--null-rhi` 只适用于不依赖截图的组件回归。真实鼠标操作应单独记录为 **native interaction evidence**；Mock 验证不能证明真实无人机飞行、真实相机视频或现场设备连通性。

## 本次验证记录

2026-10-20 完成。**8 个不同的相关测试均取得发现 1、完成 1、通过 1、失败 0 的结果**。这是组件 / 渲染自动化结果，真实鼠标交互为 **NOT NATIVE VERIFIED**。

| 验证项 | 结果 / 证据 |
| --- | --- |
| UE 5.8 Editor 构建 | PASS，退出 0；[最终构建日志](../Evidence/FigmaCommand/2026-10-20/build.txt) |
| `DroneOps.Figma.CommandPresentation` | RHI PASS 1/1；[最终运行](../Evidence/PlanConfiguration/verification-1792472214848644000/summary.json)；八画板与额外英文画面共 9 张 1920×1080 截图 |
| `DroneOps.PlanConfiguration.Workspace` | NullRHI PASS 1/1；实际创建、保存、配置租约、未保存确认、告警弹窗返回保护 |
| `DroneOps.P4.Workflow.CommandLiveCRUD` | NullRHI PASS 1/1；后端确认的方案 / 任务变更与角色权限 |
| `DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo` | NullRHI PASS 1/1；检查、只读、复制和选项关系 |
| `DroneOps.P2.CommandLayoutSmoke` | NullRHI PASS 1/1；新 Command 组件存在性及地图配置 |
| `DroneOps.P2.EventLogProjection` | NullRHI PASS 1/1；事件投影与去重 |
| `DroneOps.P55.ReservationWidgets` | 补齐原有方案与暂停执行夹具后 NullRHI PASS 1/1；[重跑证据](../Evidence/PlanConfiguration/verification-1792472253014576800/summary.json)；占用设备仍可见并标注拥有者、禁选，完成后恢复可选 |
| `DroneOps.P4.VideoTarget.ExplicitSelectionAndBrowserIsolation` | Video 角色 RHI PASS 1/1；[证据](../Evidence/PlanConfiguration/verification-1792472002389591600/summary.json)；选择、语言和视频目标独立 |
| 资源 / 代码检查 | 153 条资源引用、55 个 SVG 哈希一致；语义映射均存在；`git diff --check` 通过 |
| 本地化 | 新增 71 键，含无人机页 39 键；中英文资源生成，RHI 画面确认切换。GatherText 内部步骤退出 0，但整体进程退出 1，仍存在项目已有 `GameFeatureData` Asset Manager 配置错误；[日志](../Evidence/FigmaCommand/2026-10-20/localization.log)，不记为整条命令无错误 PASS |

五项通用 Command 回归在 [批次报告](../Evidence/PlanConfiguration/verification-1792471927693467200/summary.json) 中；该批次的第六项 ReservationWidgets 因缺夹具失败，故整批仍正确保留为 FAIL。重跑补齐现有 `Scripts/P55Fix02/fixture.mjs` 与 `Backend/tests/p52_execution_integration.mjs`，未改占用断言。所有本次运行的自有进程均已清理，二进制在各次运行期间未变化。

通用 Command / Video 回归使用模块 SHA-256 `8bf187a9f6b9ba2efd9a3e87424a3dcef3da0fd7e03259f5cafdeedb94de67db`。之后仅调整新建弹窗顶部留白；最终构建为 `7f29cfd4c6fc784e922ab01e4857a1b9d514c38b31377a629ab6f4b5693f1250`，Figma RHI 和 ReservationWidgets 已在最终构建复验。Backend 使用正常 `Backend/build/Release/DroneBackend.exe`，SHA-256 `5bae091ae8c677af33dbe5bae2e10a5d8abe6d279d57888908eb335e1defd067`。

首轮失败证据仍保留：[UI 首轮](../Evidence/PlanConfiguration/verification-1792471091546206700/summary.json)、[配置首轮](../Evidence/PlanConfiguration/verification-1792471122098484700/summary.json)。UI 测试的程序赋值现显式发送多行编辑通知；配置测试改为检查新告警弹窗，继续保留输入、租约和不修改后端的断言。原生鼠标、打包运行、真实无人机飞行和真实视频流均不在本次验证范围内。

最终预览（UE 渲染自动化 / Mock）：

- [总览](../Evidence/FigmaCommand/2026-10-20/01-overview.png)
- [设备状态](../Evidence/FigmaCommand/2026-10-20/02-devices.png)、[告警](../Evidence/FigmaCommand/2026-10-20/03-alerts.png)、[日志](../Evidence/FigmaCommand/2026-10-20/04-logs.png)
- [安保方案](../Evidence/FigmaCommand/2026-10-20/05-plans.png)、[新建方案](../Evidence/FigmaCommand/2026-10-20/06-new-plan.png)
- [无人机](../Evidence/FigmaCommand/2026-10-20/07-fleet.png)、[标签设置](../Evidence/FigmaCommand/2026-10-20/08-label.png)、[英文](../Evidence/FigmaCommand/2026-10-20/09-fleet-en.png)

公开证据采用项目既有脱敏规则，原始日志保留于忽略的 Saved 目录；断言、失败记录与测试计数不删改。
