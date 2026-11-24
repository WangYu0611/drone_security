# Drone Security Mock MCP 接入指南

接口契约 v0.2 · 2026-11-23

目标是让 LLM 通过 MCP 逐项使用指挥系统的 Mock 业务功能。v0.2 文档包含 69 项工具的参数、回执、权限、错误处理和验收，覆盖下列八个模块。

**当前进度**　49 项可在隔离工作区联调，包含查询及回放、方案与任务编辑、夹具航线、配置检查和部署、模拟设备管理和任务预演，以及共享选机、语言和视频目标。各工具在表中标记实现状态。草稿与业务数据由 Mock Backend 持有，工作区回放由 MCP 服务持有。启动、鉴权与自动化验收见 [Mock MCP 运行说明](../tools/mock_mcp/README.md)。 [业务演示手册](Stage1/Stage1-Demo-Runbook.md) · [编辑器 MCP 记录](TASK-A3-Startup-Visual.md#unreal-mcp-evidence-and-current-boundary)

## 当前系统功能盘点

范围以 2026-11-23 的 `main` 为准，核对了 Command、Map、Video、Pilot 和 Backend。历史 Stage 1 Mock、现行模拟机预演和 Mock Pilot 测试都属于本目录的业务来源。当前 Launcher 默认实时入口，模拟能力是否可用取决于测试配置。这里记录代码、说明和已有验收的证据，MCP 运行及原生界面仍需按各自用例重新验证。 [历史功能审计](Stage1/Stage1-Feature-Audit.md) · [当前运行说明](First-Run-Windows.md) · [近期预演验收](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

## 工具与权限

目录共 **69 项，契约均已写入**。其中 **49 项已实现，20 项等待实现**。点击工具名称可查看参数、成功回执、错误处理与逐项验收。文档沿用 v0.2 的 `contract_version`。

当前工作区查询包括方案详情与列表、部署、执行、模拟设备、预演、回放记录、健康、会话选择、语言、视频目标和事件。实际工具清单由 `tools/list` 按令牌权限返回。带 `session_id` 的工具使用启动时分配的会话 ID。

方案写入支持取得与释放配置、修改草稿、复制、重开、新增任务、重命名、模拟设备分配及配置检查。成功回执与业务变更同时落盘。配置句柄闲置 120 秒后失效，由同一身份成功提交的配置操作续期。`get_mock_security_plan` 返回当前身份所持有的句柄。

权限按工具和工作区判定。每个工具均需 `mock:read`，写工具还需表中所列权限。服务在发现和调用时校验身份及对象归属。业务写入以隔离 Mock Backend 确认或工具所属会话的结果为准。复核、部署与开始各自校验操作员确认。模拟写入仅面向测试数据和软件仿真，无硬件出口。

### 录制场景与回放

固定录制场景用于客户端联调。业务执行和任务预演完成后产生的回放记录来自隔离 Mock 工作区。Map 已具备轨迹回放控件，近期验收记录覆盖了采样和显示，相关原生鼠标交互仍待验收。 [Stage 1 演示手册](Stage1/Stage1-Demo-Runbook.md) · [任务预演验收](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`list_demo_scenarios`](#list_demo_scenarios) | `mock:read` | 已实现 | 列出固定录制场景和 `scenario_id`。 |
| [`get_sim_status`](#get_sim_status) | `mock:read` | 已实现 | 读取录制场景中的方案、任务、设备及当前身份的回放摘要。 |
| [`list_sim_alerts`](#list_sim_alerts) | `mock:read` | 已实现 | 查询固定场景录制告警。 |
| [`set_replay_state`](#set_replay_state) | `mock:read` + `mock:replay` | 已实现 | 播放或暂停已分配的录制回放。 |
| [`list_mock_replay_records`](#list_mock_replay_records) | `mock:read` | 已实现 | 列出工作区内可回放的模拟执行与预演记录。 |
| [`open_mock_record_replay`](#open_mock_record_replay) | `mock:read` + `mock:replay` | 已实现 | 从工作区回放记录打开独立回放会话。 |
| [`seek_mock_replay`](#seek_mock_replay) | `mock:read` + `mock:replay` | 已实现 | 定位到回放时间点。 |
| [`close_mock_replay`](#close_mock_replay) | `mock:read` + `mock:replay` | 已实现 | 关闭自己的回放会话。 |

工作区回放沿用 `set_replay_state` 的状态和版本校验，返回对应的录制来源。播放、暂停和定位均作用于回放会话。

### 安保方案与任务配置

Command 已支持方案与任务配置、草稿复制和删除、已部署方案重开。Backend 持有方案、任务、内容修订和配置占用。 [方案配置流程](PLAN-Configuration-Workflow.md) · [返回编辑与重部署](EditReturnVideo.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`list_mock_security_plans`](#list_mock_security_plans) | `mock:read` | 已实现 | 列出工作区内的方案及草稿、就绪、已部署状态。 |
| [`get_mock_security_plan`](#get_mock_security_plan) | `mock:read` | 已实现 | 按方案 ID 读详情、任务列表、修订与当前配置占用摘要。 |
| [`create_security_plan_draft`](#create_security_plan_draft) | `mock:read` + `mock:plan:draft` | 已实现 | 持久化空任务草稿并交接给指挥端。 |
| [`begin_mock_plan_edit`](#begin_mock_plan_edit) | `mock:read` + `mock:plan:draft` | 已实现 | 申请指定 Mock 方案的配置占用。 |
| [`end_mock_plan_edit`](#end_mock_plan_edit) | `mock:read` + `mock:plan:draft` | 已实现 | 结束自己取得的配置占用，报告仍待保存或退出的编辑会话。 |
| [`update_mock_security_plan_draft`](#update_mock_security_plan_draft) | `mock:read` + `mock:plan:draft` | 已实现 | 修改现有草稿的名称和说明。 |
| [`copy_mock_security_plan_draft`](#copy_mock_security_plan_draft) | `mock:read` + `mock:plan:draft` | 已实现 | 从可读方案复制新草稿及独立的任务和航线。 |
| [`delete_mock_security_plan_draft`](#delete_mock_security_plan_draft) | `mock:read` + `mock:plan:delete` | 已实现 | 经确认删除满足状态及占用条件的指定草稿。 |
| [`reopen_mock_security_plan`](#reopen_mock_security_plan) | `mock:read` + `mock:plan:draft` | 已实现 | 按现有门禁重开已部署方案以继续编辑。 |
| [`add_mock_plan_mission`](#add_mock_plan_mission) | `mock:read` + `mock:plan:draft` | 已实现 | 在可编辑方案新增任务。 |
| [`rename_mock_plan_mission`](#rename_mock_plan_mission) | `mock:read` + `mock:plan:draft` | 已实现 | 修改指定任务名称。 |
| [`delete_mock_plan_mission`](#delete_mock_plan_mission) | `mock:read` + `mock:plan:delete` | 已实现 | 经确认删除指定草稿任务。 |
| [`set_mock_mission_assignment`](#set_mock_mission_assignment) | `mock:read` + `mock:plan:draft` | 已实现 | 设置、变更或清空任务中的 Mock 模拟成员。 |

### 地图航线与方案几何

Map 已支持航点编辑、保存、完成或放弃，以及方案整体移动预览和提交。每项任务在 Map 中应只有一个航线显示所有者。编辑期间可以手动平移、缩放和绕转，自动跟随与聚焦受编辑状态限制。 [方案几何](TASK-P5.3-PlanGeometryClosure.md) · [交互后续](UIInteractionFollowup.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`get_mock_mission_route`](#get_mock_mission_route) | `mock:read` | 已实现 | 读给定任务的 Mock 航线摘要、版本和状态。 |
| [`begin_mock_route_edit`](#begin_mock_route_edit) | `mock:read` + `mock:route:edit` | 已实现 | 为测试任务取得与配置会话绑定的航线编辑会话。 |
| [`save_mock_route_draft`](#save_mock_route_draft) | `mock:read` + `mock:route:edit` | 已实现 | 基于约定的合成地图夹具保存航线修改并保留编辑会话。 |
| [`end_mock_route_edit`](#end_mock_route_edit) | `mock:read` + `mock:route:edit` | 已实现 | 完成或放弃当前会话内的未保存修改。 |
| [`preview_mock_plan_translation`](#preview_mock_plan_translation) | `mock:read` + `mock:route:edit` | 待实现 | 预览方案整体平移在指定合成夹具中的结果与检查项。 |
| [`commit_mock_plan_translation`](#commit_mock_plan_translation) | `mock:read` + `mock:route:edit` | 待实现 | 提交经确认且版本仍有效的 Mock 平移预览。 |
| [`cancel_mock_plan_translation`](#cancel_mock_plan_translation) | `mock:read` + `mock:route:edit` | 待实现 | 取消当前 Mock 整体平移预览并结束其编辑会话。 |

工具面向业务状态和合成场景。相机手势、地图命中和航线显示所有权仍由 Map 处理。

### 检查复核与 Mock 执行

Command 已有配置检查、操作员复核、部署快照、立即或预约开始，以及执行详情和历史。部署和开始是独立业务动作。以下写工具只管理隔离模拟数据，分别沿用人工确认和状态校验。 [现有演示流程](Stage1/Stage1-Demo-Runbook.md) · [部署与配置占用](PLAN-Configuration-Workflow.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`validate_mock_plan`](#validate_mock_plan) | `mock:read` + `mock:plan:validate` | 已实现 | 运行配置检查并返回通过、阻塞与当前内容修订。 |
| [`record_mock_plan_review`](#record_mock_plan_review) | `mock:read` + `mock:plan:review` | 已实现 | 记录操作员针对指定修订确认的模拟方案复核。 |
| [`list_mock_plan_deployments`](#list_mock_plan_deployments) | `mock:read` | 已实现 | 读取隔离工作区的模拟部署版本及快照摘要。 |
| [`publish_mock_plan_snapshot`](#publish_mock_plan_snapshot) | `mock:read` + `mock:plan:deploy` | 已实现 | 经操作员确认冻结经检查与复核的 Mock 配置快照。 |
| [`list_mock_executions`](#list_mock_executions) | `mock:read` | 已实现 | 按方案、任务或状态查预约、活动与历史模拟执行。 |
| [`get_mock_execution`](#get_mock_execution) | `mock:read` | 已实现 | 查询模拟执行进度、成员、耗时、状态和结果原因。 |
| [`request_mock_execution`](#request_mock_execution) | `mock:read` + `mock:execution:start` | 待实现 | 提交操作员已确认的立即或预约模拟任务，并返回模拟执行编号。 |
| [`update_mock_execution_schedule`](#update_mock_execution_schedule) | `mock:read` + `mock:execution:schedule` | 待实现 | 重排待开始的模拟任务。 |
| [`start_scheduled_mock_execution`](#start_scheduled_mock_execution) | `mock:read` + `mock:execution:start` | 待实现 | 经操作员确认，立即开始指定的待启动模拟任务。 |
| [`cancel_mock_execution_schedule`](#cancel_mock_execution_schedule) | `mock:read` + `mock:execution:schedule` | 待实现 | 取消尚未开始的模拟预约并检查资源释放。 |
| [`set_mock_execution_state`](#set_mock_execution_state) | `mock:read` + `mock:execution:control` | 待实现 | 对指定 Mock 执行执行暂停、恢复、模拟返航或经确认终止，按对象类型和当前状态判定可用动作。 |

### 模拟设备与集群演示

当前存在设备列表与状态投影、批量新增模拟机，以及历史 Stage 1 Mock 的单机和集群移动、执行中单机接管。集群预览、提交和执行组控制是不同动作。 [交互后续](UIInteractionFollowup.md) · [集群交互](MAP-Fleet-Control-UX.md) · [单机接管演示](Stage1/Stage1-Demo-Runbook.md#执行中单机手动接管)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`list_mock_devices`](#list_mock_devices) | `mock:read` | 已实现 | 列出隔离工作区的测试设备与模拟机。 |
| [`get_mock_device_status`](#get_mock_device_status) | `mock:read` | 已实现 | 查看来源、观测时间、模拟位置、可用状态、当前任务与占用。 |
| [`create_mock_simulated_devices`](#create_mock_simulated_devices) | `mock:read` + `mock:fleet:manage` | 已实现 | 按已配置的合成夹具批量新增模拟机。 |
| [`update_mock_device_metadata`](#update_mock_device_metadata) | `mock:read` + `mock:fleet:manage` | 已实现 | 修改隔离测试注册表的展示信息。 |
| [`remove_mock_test_device`](#remove_mock_test_device) | `mock:read` + `mock:fleet:manage` | 已实现 | 在无任务或控制权占用且经确认时移除测试注册表条目。 |
| [`preview_mock_fleet_move`](#preview_mock_fleet_move) | `mock:read` + `mock:fleet:preview` | 待实现 | 预览单机或集群在指定合成场景中的移动结果。 |
| [`confirm_mock_fleet_move`](#confirm_mock_fleet_move) | `mock:read` + `mock:fleet:simulate` | 待实现 | 在隔离模拟器中提交经确认且未过期的移动预览。 |
| [`preview_mock_single_takeover`](#preview_mock_single_takeover) | `mock:read` + `mock:fleet:preview` | 待实现 | 预览单架模拟机从已有模拟任务切出的影响。 |
| [`confirm_mock_single_takeover`](#confirm_mock_single_takeover) | `mock:read` + `mock:fleet:simulate` | 待实现 | 在隔离模拟器确认已预览的接管，并确认其余演示成员的状态。 |
| [`cancel_mock_fleet_preview`](#cancel_mock_fleet_preview) | `mock:read` + `mock:fleet:preview` | 待实现 | 撤销尚未提交的当前移动或接管预览。 |

暂停、恢复或终止已创建的演示移动使用 `set_mock_execution_state`。设备展示信息只涉及测试数据，模拟状态须携带来源和时间。

### 任务预演与结果复核

新增模拟机队支持在物理机队为空时完成任务预演，并持久化进度和回放采样。新版预演与历史 Stage 1 演示运行各有记录。 [启动器预演说明](../Launcher/README.md#mission-rehearsals) · [2026-11-20 验收记录](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`list_mock_rehearsals`](#list_mock_rehearsals) | `mock:read` | 已实现 | 列出任务的预演批次及复核状态。 |
| [`get_mock_rehearsal`](#get_mock_rehearsal) | `mock:read` | 已实现 | 读取成员进度、结果、输入修订以及可回放的记录引用。 |
| [`start_mock_rehearsal`](#start_mock_rehearsal) | `mock:read` + `mock:rehearsal:run` | 已实现 | 启动或重新运行仅含模拟机的测试预演。 |
| [`record_mock_rehearsal_review`](#record_mock_rehearsal_review) | `mock:read` + `mock:rehearsal:review` | 已实现 | 记录操作员对已完成且输入版本仍有效的预演复核。 |

新增模拟机、任务分配和预演回放使用前述独立工具。复核结果仅作用于隔离预演。

### 态势上下文告警日志与视频

Command 展示设备、告警和业务日志，已处理告警标记属于界面会话。Map 与 Video 同步选机和视频目标，播放状态单独确认。Video 的多卡片布局共用一个主播放器。 [界面和数据审计](Stage1/Stage1-Feature-Audit.md) · [选机视频联动](EditReturnVideo.md) · [视频验收](TASK-V2-VideoMultiFeedUI.md)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`get_mock_workspace_health`](#get_mock_workspace_health) | `mock:read` | 已实现 | 查看隔离工作区就绪、协作客户端连接及数据新鲜度摘要。 |
| [`get_mock_workspace_context`](#get_mock_workspace_context) | `mock:read` | 已实现 | 读当前授权会话的选中方案、任务、设备及视频目标。 |
| [`set_mock_workspace_selection`](#set_mock_workspace_selection) | `mock:read` + `mock:context:select` | 已实现 | 在授权的演示会话选择或清空当前方案、任务及设备集合。 |
| [`get_mock_workspace_preferences`](#get_mock_workspace_preferences) | `mock:read` | 已实现 | 读取演示会话可同步的语言偏好。 |
| [`set_mock_workspace_language`](#set_mock_workspace_language) | `mock:read` + `mock:context:preferences` | 已实现 | 切换授权演示会话的共享语言。 |
| [`list_mock_session_alerts`](#list_mock_session_alerts) | `mock:read` | 待实现 | 查询运行中的 Mock 工作区告警。 |
| [`acknowledge_mock_session_alert`](#acknowledge_mock_session_alert) | `mock:read` + `mock:alerts:ack` | 待实现 | 为当前授权会话标记一条模拟告警已处理。 |
| [`list_mock_operation_events`](#list_mock_operation_events) | `mock:read` | 已实现 | 按分类、对象或时间查询脱敏业务日志与事件。 |
| [`list_mock_video_sources`](#list_mock_video_sources) | `mock:read` | 已实现 | 查询允许访问的样例视频源和可用情况。 |
| [`get_mock_video_view`](#get_mock_video_view) | `mock:read` | 已实现 | 读共享视频目标与样例源状态，分别报告已选目标和已确认播放状态。 |
| [`set_mock_video_target`](#set_mock_video_target) | `mock:read` + `mock:video:select` | 已实现 | 显式切换至受控的 Mock 视频目标。 |

告警确认仍需约定会话持久化边界。选机与视频切换的契约应保留 Map 显式选机的现有联动语义。

### 驾驶员交接与测试工作区

Mock Pilot 存在显式交接申请、Command 人工授权及收回、Pilot 启用和退出。历史 DEMO 启动归位属于测试环境操作。以下工具覆盖交接流程的观察及申请，操作者的授权和 Pilot 操作保持各自的权限。 [驾驶员 Mock](../Backend/README.md#104-驾驶员控制权-mock) · [Mock 启动归位](UIInteractionFollowup.md#启动归位与地图可见性2026-10-20)

| 工具 | 所需权限 | 实现 | 覆盖能力 |
| --- | --- | --- | --- |
| [`list_mock_pilot_handoff_requests`](#list_mock_pilot_handoff_requests) | `mock:read` | 待实现 | 列出隔离工作区内的 Mock 交接申请和确认状态。 |
| [`get_mock_pilot_handoff_status`](#get_mock_pilot_handoff_status) | `mock:read` | 待实现 | 查模拟设备的交接、授权和会话是否已结束，仅返回授权范围内的状态。 |
| [`request_mock_pilot_handoff`](#request_mock_pilot_handoff) | `mock:read` + `mock:pilot:request` | 待实现 | 把指定 Mock 设备的交接申请送到人工处理入口。 |
| [`request_mock_pilot_handoff_release`](#request_mock_pilot_handoff_release) | `mock:read` + `mock:pilot:request` | 待实现 | 申请由操作员处理 Mock 控制权收回，返回申请及确认状态。 |
| [`request_mock_workspace_reset`](#request_mock_workspace_reset) | `mock:read` + `mock:sandbox:reset` | 待实现 | 仅测试管理员可向自己的隔离工作区发起人工确认的重置。 |

### 客户端与环境管理边界

| 当前系统能力 | MCP 接入归属 |
| --- | --- |
| Map 2D/3D、平移缩放与绕转、定位、框选手势、航线渲染 | 用选择、方案和航线工具提供业务结果，视角与交互由 Map 维护。 |
| Video 布局、主副卡交换、播放器重试和当前帧渲染 | 用样例源、视频目标和状态工具提供业务数据，播放器交互由 Video 维护。 |
| Pilot 启用、退出、连续操作与人工授权 | 用交接申请和状态工具协助人工处理，操作与授权由原有客户端及身份管理。 |
| Launcher 进程启停、依赖检查、地图定位与常用地点、日志文件及配置文件 | 工作区健康和业务日志对模型提供脱敏摘要，进程、文件和地图环境由运行人员维护。 |
| 真实设备、真实图传、真机分配、飞控链路、历史目标与攻击预演 | 属于本目录之外的功能面。Mock 服务使用合成数据和独立工作区。 |

### 实现与联调验收

1. 实现时先用方案、设备和事件查询跑通授权与数据回读，再接草稿编辑、航线会话与复核。执行、预演、视频和交接工具使用对应的状态与确认用例验收。
2. 每个写工具交付前明确作用对象、权限、版本冲突、重复请求、部分失败及重试语义。配置会话继续以 Backend 为权威，回放会话与业务执行独立，部署快照与开始执行分开。
3. 各工具在隔离工作区核对工具发现、成功与失败响应、跨身份访问拒绝和操作前后快照。协议或组件自动化与原生 UI 验收分别保存证据。

## 通用数据约定

- `arguments` 是 JSON 对象，拒绝未声明的字段。场景、方案和回放 ID 区分大小写，长度为 1 至 64，只接受 ASCII 字母、数字、下划线和连字符。草稿工具的 `request_id` 另有格式要求。
- 成功响应的 `structuredContent` 包含 `contract_version`（字符串 `"0.2"`）、`simulation`（布尔值 `true`）和 `observed_at`（服务端读取或确认结果的 RFC 3339 UTC 时间）。响应示例中的字段均存在，可空字段单独说明。
- 用 `tools/list` 返回的 `inputSchema` 和 `outputSchema` 校验数据。服务可在 `content` 的文本块中附相同的 JSON，兼容旧客户端。所有 ID、时间和返回值均为联调样例。

## 场景与状态工具

### `list_demo_scenarios`

传入空对象 `{}`。`scenarios` 返回该身份可访问的场景，每项包含场景 ID 和展示名称。没有可访问场景时返回空数组。

```json
{}
```

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:00:00Z",
  "scenarios": [{"scenario_id": "demo-a", "title": "Demo A"}]
}
```

### `get_sim_status`

`scenario_id` 为必填字符串，取自场景列表。返回方案、任务计数、设备计数和当前身份的回放。各计数为非负整数。`plan.state` 取 `draft` 或 `active`，没有方案时 `plan` 为 `null`。当前身份没有可用回放时 `replay` 为 `null`。`replay.state` 取 `playing` 或 `paused`。`position_ms` 是回放起点起算的非负毫秒整数，`version` 是用于写入校验的非负整数。

```json
{"scenario_id": "demo-a"}
```

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:00:00Z",
  "scenario_id": "demo-a",
  "plan": {"plan_id": "plan-a", "title": "Demo A", "state": "active"},
  "tasks": {"total": 2, "completed": 1},
  "devices": {"total": 2, "online": 1},
  "replay": {
    "replay_id": "rpl-demo-a-01", "state": "playing",
    "position_ms": 40000, "version": 3
  }
}
```

## 告警与回放工具

### `list_sim_alerts`

`scenario_id` 为必填字符串。`severity` 可选，取 `info`、`warning` 或 `critical`，省略时查全部级别。`limit` 可选，取 1 至 50 的整数，默认 20。录制告警按 `occurred_at` 从新到旧排序，同一时间按 `alert_id` 升序。`occurred_at` 用 RFC 3339 UTC。无匹配时返回空数组。

```json
{"scenario_id": "demo-a", "severity": "warning", "limit": 10}
```

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:00:00Z",
  "scenario_id": "demo-a",
  "alerts": [{
    "alert_id": "alert-a-01", "severity": "warning",
    "code": "DEVICE_OFFLINE", "occurred_at": "2026-11-23T04:59:50Z"
  }]
}
```

### `set_replay_state`

`replay_id` 和 `state` 为必填字符串，状态只接受 `playing` 或 `paused`。`expected_version` 为必填非负整数，取最近查询返回的回放版本。服务先校验会话归属和版本，状态变化时递增版本。暂停后进度保持稳定，恢复后从该位置继续。

```json
{"replay_id": "rpl-demo-a-01", "state": "paused", "expected_version": 3}
```

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:00:01Z",
  "scenario_id": "demo-a",
  "replay": {
    "replay_id": "rpl-demo-a-01", "state": "paused",
    "position_ms": 41000, "version": 4
  }
}
```

#### 工作区回放扩展

工作区回放会话和请求回执保存在工作区的 SQLite 文件。每项记录必须有已完成的回放采样，记录列表只显示调用身份自己的回放会话。

`open_mock_record_replay` 返回的 `replay_id` 也可传给本工具。参数与回放版本校验沿用上面的契约，返回用 `record_id` 表示工作区记录来源。固定录制场景仍返回 `scenario_id`。版本冲突时用 `list_mock_replay_records` 读取当前身份的回放版本再决定是否重试。

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "record_id": "record-a",
  "replay": {"replay_id": "replay-a", "state": "playing", "position_ms": 12000, "version": 3}
}
```

验收时先打开自己的工作区回放并定位，再切到播放。回读记录列表确认只推进自己的回放，原业务执行状态不变。固定场景的旧示例应继续通过。

## 创建安保方案草稿

### `create_security_plan_draft`

收到用户明确的创建请求后调用。工具把空任务的草稿落盘到当前身份关联的隔离 Mock 工作区。本文的录制演示场景用于查状态和回放，草稿用返回的 `plan_id` 在 Mock 指挥端打开。权限为 `mock:read` 和 `mock:plan:draft`，与回放权限独立。

| 参数 | 类型 | 必填 | 约束 |
| --- | --- | --- | --- |
| `request_id` | 字符串 | 是 | 客户端为一次用户创建动作生成的小写 UUID v4。同一次动作的重试沿用此值。 |
| `name` | 字符串 | 是 | 去除首尾 ASCII 空白后为 1 至 160 个 UTF-8 字节。 |
| `description` | 字符串 | 否 | 最多 1,000 个 UTF-8 字节，省略按空字符串处理。 |

调用参数 `arguments`

```json
{
  "request_id": "74396890-8610-4fbe-84e7-9326c251bf91",
  "name": "Weekend event demo",
  "description": "Draft for team review."
}
```

成功返回 `structuredContent`

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "74396890-8610-4fbe-84e7-9326c251bf91",
  "plan_id": "plan-42",
  "name": "Weekend event demo",
  "state": "draft"
}
```

`state: "draft"` 对应后端创建时的 `DRAFT` 状态。这是创建回执。客户端拿到成功回执后再汇报方案编号，操作员凭该编号在指挥端接手。

### 重复请求与超时

服务按 **Mock 工作区、已认证身份、`request_id`** 保存规范化参数和创建回执，记录随测试工作区保留，服务重启后仍有效。草稿与请求键关联需要由后端原子持久化，或由后端提供按键恢复结果的等效机制，覆盖草稿落盘后响应丢失的情况。

- 同一键、相同参数重试时返回原创建回执和同一个 `plan_id`。并发重试最多落盘一份草稿。
- 同一键换名称或说明时返回 `REQUEST_ID_CONFLICT`。如果用户明确发起一次新的创建，客户端生成新的键。
- 调用超时、配置冲突或交接待确认时，客户端保留原键和参数。按错误类型等待或重试。JSON-RPC 消息 ID 由 SDK 管理。

### 配置占用与交接

1. MCP 用自己的专用实例身份申请 Backend 的配置占用。已有活动的配置会话时返回 `CONFIGURATION_IN_PROGRESS`，保留对方会话。操作员退出配置或租约失效后，客户端可用原键重试。
2. Backend 落盘草稿并将 MCP 持有的占用关联到该草稿。MCP 按所有者、方案和 `acquired_version` 核对会话，结束自己本次取得的配置占用。确认该占用已释放或失效后返回成功，操作员即可在指挥端接手。
3. 草稿已落盘、占用释放待确认时，返回 `DRAFT_HANDOFF_PENDING`。重试先查原草稿和占用，再完成交接。若操作员已取得新占用，保留该会话。租约失效由 Backend 判定。

草稿写入沿用现有配置占用、后端确认与指挥端接手规则。去重和交接恢复属于 MCP 契约的实现要求。 [现有配置占用规则](PLAN-Configuration-Workflow.md)

首批实现在 Backend 的同一事务内检查配置占用、持久化草稿与回执并完成交接。活动配置被拒绝时保留原请求键和参数。成功回执返回时 MCP 的配置占用已结束，因此该原子路径无需返回中间交接状态。

业务错误示例 `structuredContent`。外层工具结果为 `isError: true`。

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:01Z",
  "error": {
    "code": "CONFIGURATION_IN_PROGRESS",
    "message": "Wait for the active configuration session to end."
  }
}
```

## 64 项补充工具契约

本节与前述五项组成 v0.2 文档契约。工作区、调用身份及测试管理员角色由服务端认证上下文确定。所有示例使用可独立复位的合成数据。工具只连接隔离测试数据与软件模拟器。路线、平移、设备创建和移动的夹具 ID 取自测试管理员提供的当前工作区夹具清单。工具按清单校验引用，拒绝额外参数。

### 参数与回执约定

- 除标为可选或条件所需的字段，参数均必填。ID 使用通用数据约定。名称去除首尾 ASCII 空白后为 1 至 160 个 UTF-8 字节。说明、备注和原因最多 1,000 个 UTF-8 字节，原因去除空白后不能为空。时间为 RFC 3339 UTC。版本为非负安全整数，`expected_*` 校验指定对象或会话。
- 分页 `limit` 为可选整数，范围 1 至 50，默认 20。`cursor` 是可选不透明字符串。列表默认按更新时间倒序，事件与告警按发生时间倒序，同一时间按 ID 升序。游标绑定身份、工作区和过滤条件，末页 `next_cursor` 为 `null`，没有记录时返回 `items: []`。可组合筛选取交集。
- 返回示例为完整 MCP `structuredContent` 对象，示例所列字段始终返回。时间位置用非负整数毫秒，`progress_pct` 为 0 至 100 的数值。缺失的观测、链接和结果原因为 `null`。`checks` 是检查项数组，每项含字符串 `code`、`message` 和 `severity`（`blocking` 或 `warning`）。
- 补充写工具均由客户端生成必填 `request_id`（小写 UUID v4），成功回执回显它。服务通过权限及对象归属校验后，按工作区、身份、工具与请求 ID 去重，保存规范化业务参数和已确认结果。同参复用原回执，改参返回 `REQUEST_ID_CONFLICT`。回执与写入须原子关联或可按键恢复，记录随测试工作区保留且重启有效。超时或处理中沿用原键及原参数。修改业务参数前核实原请求结果，再以新键提交不同的动作。
- 需要 `approval_id` 的操作由授权操作员确认后由服务端签发句柄，绑定工具、工作区、目标、规范化业务参数和对象版本或预览。示例 `approval-a` 是测试夹具标识。确认句柄和请求 ID 不参与业务参数指纹，已完成的相同请求重试复用原回执。过期确认的未提交请求可在重新确认同一业务参数后沿用原键。
- 配置、航线编辑以 Backend 结果为准。句柄绑定身份及取得占用的版本，服务管理活动会话续期，空闲 120 秒后停止续期，实际失效由 Backend 判定。预览有效期 60 秒，提交时重查占用、版本和夹具条件。结束编辑或取消预览只影响该句柄对应的会话。

### 错误语义

缺少认证或权限时，跨机器联调使用 HTTP 401 或 403。对象不在授权范围时返回 `NOT_FOUND`。业务错误的外层 MCP 工具结果为 `isError: true`，`structuredContent.error` 至少含字符串 `code` 和 `message`。可按授权返回 `field` 或 `current_version`。

| 错误码 | 客户端处理 |
| --- | --- |
| `INVALID_ARGUMENT` / `CURSOR_INVALID` | 修正参数，失效游标从首批刷新。 |
| `NOT_FOUND` / `SESSION_EXPIRED` / `REPLAY_NOT_FOUND` | 重新发现授权对象或恢复本人会话。 |
| `VERSION_CONFLICT` / `REVISION_CONFLICT` | 读取当前对象或回放版本，再决定是否继续。 |
| `REQUEST_ID_CONFLICT` / `REQUEST_IN_PROGRESS` | 改参复用键时核对原请求。处理中保留原键并退避查询或重试。 |
| `CONFIGURATION_IN_PROGRESS` / `CONFIGURATION_EXPIRED` / `EDIT_IN_PROGRESS` | 等待活动占用结束、重新取得失效会话，或先完成现有编辑。 |
| `APPROVAL_REQUIRED` / `APPROVAL_INVALID` | 获取操作员对当前操作及版本的确认。 |
| `INVALID_STATE` / `OBJECT_IN_USE` / `LIMIT_EXCEEDED` | 读取状态和业务占用，并检查工作区额度。 |
| `FIXTURE_NOT_FOUND` / `PREVIEW_EXPIRED` / `PREVIEW_STALE` / `PREVIEW_BLOCKED` | 核对合成夹具、重新预览或处理阻塞项。 |
| `CHECK_FAILED` / `REVIEW_REQUIRED` / `REHEARSAL_STALE` | 补齐检查与复核，或为当前输入重新做预演。 |
| `MOCK_UNAVAILABLE` / 超时 | 查询可有限重试。写入保留原键及参数，并检查权威结果。 |

以下是过期对象版本的业务错误负载示例。

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "error": {"code": "VERSION_CONFLICT", "message": "Read the current object version.", "current_version": 5}
}
```

每项验收还要用较低权限和另一工作区的身份验证拒绝。写操作以隔离快照、日志和回执核对提交次数。协议、组件自动化与原生 UI 分别取证。

### 录制场景与回放契约

固定录制场景沿用原接口。工作区回放同一身份在同一记录下只有一个有效会话。列表中的 `execution_id` 与 `rehearsal_id` 只填一种来源，另一项为 `null`。工作区回放的版本由播放状态变更或手动定位推进，播放时间自然推进时沿用原版本。新开的工作区回放也可调用 `set_replay_state`，详见该工具的补充。

#### `list_mock_replay_records`

**权限**　`mock:read`。

**参数**　`execution_id`、`rehearsal_id`、`record_id`（可选 ID，可组合）；`limit`、`cursor`（分页）。

```json
{"rehearsal_id":"reh-a","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"record_id":"record-a","execution_id":null,"rehearsal_id":"reh-a","duration_ms":60000,"replay":{"replay_id":"replay-a","state":"paused","position_ms":12000,"version":2}}],
  "next_cursor": null
}
```

**错误处理**　`CURSOR_INVALID`：用相同过滤条件从首批重新查询。

**验收**　筛选 `reh-a` 得到 `record-a`；无本人回放时 `replay` 为 `null`，另一身份的会话不会出现在结果。

#### `open_mock_record_replay`

**权限**　`mock:read` + `mock:replay`。

**参数**　`record_id`（必填 ID）；`request_id`（通用写入）。

```json
{"record_id":"record-a","request_id":"00000000-0000-4000-8000-000000000002"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000002",
  "record_id": "record-a",
  "duration_ms": 60000,
  "replay": {"replay_id":"replay-a","state":"paused","position_ms":0,"version":1}
}
```

**错误处理**　`NOT_FOUND`：重新列出可见记录。`INVALID_STATE`：记录尚未完成采样，等待可回放。

**验收**　打开后位置为零且处于暂停；同一身份再次打开同一记录返回仍有效的会话，回放启动前后业务执行状态一致。

#### `seek_mock_replay`

**权限**　`mock:read` + `mock:replay`。

**参数**　`replay_id`（必填 ID）、`position_ms`（必填非负整数，最大为记录时长）、`expected_version`（必填版本）；`request_id`。

```json
{"replay_id":"replay-a","position_ms":12000,"expected_version":1,"request_id":"00000000-0000-4000-8000-000000000003"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000003",
  "record_id": "record-a",
  "replay": {"replay_id":"replay-a","state":"paused","position_ms":12000,"version":2}
}
```

**错误处理**　`REVISION_CONFLICT`：用记录列表获取自己的最新会话后再决定。`REPLAY_NOT_FOUND`：重新打开记录。越界返回 `INVALID_ARGUMENT`。

**验收**　定位到 12,000 ms 后版本递增，播放状态保持，随后提交旧版本时游标仍在原处。

#### `close_mock_replay`

**权限**　`mock:read` + `mock:replay`。

**参数**　`replay_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"replay_id":"replay-a","expected_version":2,"request_id":"00000000-0000-4000-8000-000000000004"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000004",
  "record_id": "record-a",
  "replay_id": "replay-a",
  "closed": true
}
```

**错误处理**　`REVISION_CONFLICT`：先读当前会话。`REPLAY_NOT_FOUND`：重新查记录并确认会话是否已关闭。

**验收**　关闭后记录仍可查询，列表中的 `replay` 为 `null`；同键重试返回原回执。

### 安保方案与任务配置契约

方案 `state` 取 `draft`、`ready` 或 `deployed`。`plan_version` 校验方案状态与写入，`content_revision` 关联检查、复核和部署。内容变化使旧检查与复核失效。`configuration.state` 为 `idle`、`owned` 或 `busy`，仅 `owned` 返回本人会话句柄及到期时间。

#### `list_mock_security_plans`

**权限**　`mock:read`。

**参数**　`state`（可选，`draft`、`ready`、`deployed`）；`limit`、`cursor`（分页）。

```json
{"state":"draft","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"plan_id":"plan-42","name":"Weekend event demo","state":"draft","plan_version":1,"content_revision":1,"mission_count":0}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正筛选枚举。`CURSOR_INVALID`：按相同条件重查。

**验收**　新建的 `plan-42` 出现在草稿列表，部署筛选排除该草稿；空列表返回 `items: []`。

#### `get_mock_security_plan`

**权限**　`mock:read`。

**参数**　`plan_id`（必填 ID）。

```json
{"plan_id":"plan-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "plan_id": "plan-a",
  "name": "Demo plan",
  "description": "Mock plan",
  "state": "draft",
  "plan_version": 4,
  "content_revision": 2,
  "missions": [{"mission_id":"mission-a","name":"Demo task","device_ids":["sim-a"],"route_version":1}],
  "configuration": {"state":"owned","configuration_id":"cfg-a","lease_expires_at":"2026-11-23T05:04:00Z"}
}
```

**错误处理**　`NOT_FOUND`：重新列出当前工作区的方案。

**验收**　方案修订和任务与 Backend 快照一致；无占用时 `configuration.state` 为 `idle`，其他人占用时为 `busy` 且两个会话字段为 `null`。

#### `begin_mock_plan_edit`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`（必填 ID）、`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","expected_plan_version":4,"request_id":"00000000-0000-4000-8000-000000000007"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000007",
  "plan_id": "plan-a",
  "configuration_id": "cfg-a",
  "plan_version": 4,
  "content_revision": 2,
  "lease_expires_at": "2026-11-23T05:04:00Z"
}
```

**错误处理**　`CONFIGURATION_IN_PROGRESS`：等待现有持有人结束。`VERSION_CONFLICT`：重读方案。`INVALID_STATE`：部署方案先走重开。

**验收**　同键返回同一会话；另一个身份申请被拒，原会话仍有效，方案内容修订保持原值。

#### `end_mock_plan_edit`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`configuration_id`（必填 ID）；`request_id`。

```json
{"configuration_id":"cfg-a","request_id":"00000000-0000-4000-8000-000000000008"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000008",
  "plan_id": "plan-a",
  "configuration_id": "cfg-a",
  "released": true
}
```

**错误处理**　`EDIT_IN_PROGRESS`：先完成或放弃关联编辑。`CONFIGURATION_EXPIRED`：重查占用；旧句柄不触碰新会话。

**验收**　有未结束的航线编辑时拒绝；结束后释放自己的占用，以旧句柄延迟请求不会释放另一身份取得的新占用。

#### `update_mock_security_plan_draft`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）；`name`（可选名称）、`description`（可选说明，空串清空），两者至少一个；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","expected_plan_version":4,"name":"Edited demo plan","request_id":"00000000-0000-4000-8000-000000000009"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000009",
  "plan_id": "plan-a",
  "name": "Edited demo plan",
  "description": "Mock plan",
  "state": "draft",
  "plan_version": 5,
  "content_revision": 3
}
```

**错误处理**　`VERSION_CONFLICT`：重新读取草稿。`CONFIGURATION_EXPIRED`：按会话规则重新取得配置。`INVALID_STATE`：只读快照需先重开。

**验收**　仅修改名称时保留说明，变更修订使上版检查与复核失效，旧版本无写入。

#### `copy_mock_security_plan_draft`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`source_plan_id`（必填 ID）、`expected_source_plan_version`（必填版本）；`name`（可选名称，默认由服务生成合法副本名）；`request_id`。

```json
{"source_plan_id":"plan-a","expected_source_plan_version":4,"name":"Demo copy","request_id":"00000000-0000-4000-8000-000000000010"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000010",
  "source_plan_id": "plan-a",
  "plan_id": "plan-copy",
  "name": "Demo copy",
  "state": "draft",
  "plan_version": 1,
  "content_revision": 1,
  "mission_count": 1
}
```

**错误处理**　`VERSION_CONFLICT`：重读源方案。`CONFIGURATION_IN_PROGRESS`：待冲突占用结束后沿用原键。

**验收**　同键只产生一份副本，副本的任务和航线 ID 与源方案不同。服务如为复制取得配置占用，返回成功时应已完成交接。编辑副本只改变副本自己的内容。

#### `delete_mock_security_plan_draft`

**权限**　`mock:read` + `mock:plan:delete`。

**参数**　`plan_id`（必填 ID）、`expected_plan_version`（必填版本）、`approval_id`（必填确认 ID）；`request_id`。

```json
{"plan_id":"plan-42","expected_plan_version":1,"approval_id":"approval-a","request_id":"00000000-0000-4000-8000-000000000011"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000011",
  "plan_id": "plan-42",
  "deleted": true
}
```

**错误处理**　`INVALID_STATE`：目标须是允许删除的草稿。`OBJECT_IN_USE`：先结束相关占用。`APPROVAL_REQUIRED` 或 `APPROVAL_INVALID`：重新确认目标与版本。

**验收**　批准后列表和读取均不再返回目标草稿，其他方案与所有执行历史保持不变；旧版本或无确认均拒绝。

#### `reopen_mock_security_plan`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`（必填 ID）、`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","expected_plan_version":8,"request_id":"00000000-0000-4000-8000-000000000012"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000012",
  "plan_id": "plan-a",
  "state": "draft",
  "plan_version": 9,
  "content_revision": 3,
  "previous_deployment_id": "dep-a"
}
```

**错误处理**　`OBJECT_IN_USE`：先检查关联活动执行与预约。`VERSION_CONFLICT`：重读方案。`INVALID_STATE`：目标当前不处于可重开状态。

**验收**　存在预约时拒绝；可重开时回到草稿并保留旧部署快照，后续编辑用 `begin_mock_plan_edit`。

#### `add_mock_plan_mission`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）、`name`（必填名称）；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","expected_plan_version":4,"name":"Second demo task","request_id":"00000000-0000-4000-8000-000000000013"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000013",
  "plan_id": "plan-a",
  "mission_id": "mission-b",
  "name": "Second demo task",
  "device_ids": [],
  "route_version": 0,
  "plan_version": 5,
  "content_revision": 3
}
```

**错误处理**　`VERSION_CONFLICT`：重读方案。`CONFIGURATION_EXPIRED`：重新取得配置。名称无效返回 `INVALID_ARGUMENT`。

**验收**　只新增一个空分配、无已保存航线的任务；丢失回执后同键重试得到同一个任务 ID。

#### `rename_mock_plan_mission`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`、`mission_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）、`name`（必填名称）；`request_id`。

```json
{"plan_id":"plan-a","mission_id":"mission-a","configuration_id":"cfg-a","expected_plan_version":4,"name":"Renamed demo task","request_id":"00000000-0000-4000-8000-000000000014"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000014",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "name": "Renamed demo task",
  "plan_version": 5,
  "content_revision": 3
}
```

**错误处理**　`NOT_FOUND`：核实任务归属。`VERSION_CONFLICT`：重读方案和任务。`CONFIGURATION_EXPIRED`：重新取得配置。

**验收**　改名前后任务 ID、设备分配及航线相同，内容修订递增；用其他方案的任务 ID 时无写入。

#### `delete_mock_plan_mission`

**权限**　`mock:read` + `mock:plan:delete`。

**参数**　`plan_id`、`mission_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）、`approval_id`（必填确认 ID）；`request_id`。

```json
{"plan_id":"plan-a","mission_id":"mission-a","configuration_id":"cfg-a","expected_plan_version":4,"approval_id":"approval-a","request_id":"00000000-0000-4000-8000-000000000015"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000015",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "deleted": true,
  "plan_version": 5,
  "content_revision": 3
}
```

**错误处理**　`EDIT_IN_PROGRESS`：先退出目标任务的航线编辑。`OBJECT_IN_USE`：核对当前占用。`APPROVAL_INVALID`：按当前任务版本重新确认。

**验收**　只有目标草稿任务从列表移除，其他任务及已冻结部署记录保留；另一任务的确认回执无法删除本任务。

#### `set_mock_mission_assignment`

**权限**　`mock:read` + `mock:plan:draft`。

**参数**　`plan_id`、`mission_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）；`device_ids`（必填唯一 ID 数组，0 至 20 项，空数组清空）；`request_id`。

```json
{"plan_id":"plan-a","mission_id":"mission-a","configuration_id":"cfg-a","expected_plan_version":4,"device_ids":["sim-a","sim-b"],"request_id":"00000000-0000-4000-8000-000000000016"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000016",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "device_ids": ["sim-a","sim-b"],
  "plan_version": 5,
  "content_revision": 3
}
```

**错误处理**　`NOT_FOUND`：用模拟设备列表复查归属。`OBJECT_IN_USE`：查询占用并调整分配。`VERSION_CONFLICT`：重读当前任务。

**验收**　分配两个模拟成员后列表准确；传空数组清空。跨工作区或混入不可分配设备时整次拒绝。

### 地图航线与方案几何契约

`route_version: 0` 代表尚无保存记录，存在保存记录的空路线按版本区分。预览状态为 `ready` 或 `blocked`，有效提交与取消都会关闭预览。提交只改变 Mock 草稿，Map 按权威状态保持各任务唯一航线显示所有者。

#### `get_mock_mission_route`

**权限**　`mock:read`。

**参数**　`plan_id`、`mission_id`（必填 ID）。

```json
{"plan_id":"plan-a","mission_id":"mission-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "route_version": 1,
  "route": {"route_id":"route-a","map_fixture_id":"map-a","waypoint_fixture_ids":["point-a","point-b"],"closed_loop":false,"state":"saved"}
}
```

**错误处理**　`NOT_FOUND`：确认任务归属；已知任务无已保存航线时返回 `route: null` 和 `route_version: 0`。早期保存的航线如果没有合成地图夹具引用，返回 `FIXTURE_NOT_FOUND`。

**验收**　已有航线的夹具顺序与快照一致；没有航线的任务可区分于未知任务。

#### `begin_mock_route_edit`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`plan_id`、`mission_id`、`configuration_id`（必填 ID）、`expected_route_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","mission_id":"mission-a","configuration_id":"cfg-a","expected_route_version":1,"request_id":"00000000-0000-4000-8000-000000000018"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000018",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "route_edit_id": "edit-a",
  "route_version": 1,
  "lease_expires_at": "2026-11-23T05:04:00Z"
}
```

**错误处理**　`CONFIGURATION_EXPIRED`：恢复父配置。`EDIT_IN_PROGRESS`：先结束存在的冲突会话。`VERSION_CONFLICT`：读取已保存航线。

**验收**　进入编辑不改变保存版本；同一任务不会同时取得两个编辑会话。原生 Map 单独验证编辑期间手动运镜和单一显示所有者。

#### `save_mock_route_draft`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`route_edit_id`（必填 ID）、`expected_route_version`（必填版本）、`map_fixture_id`（必填 ID）、`waypoint_fixture_ids`（必填有序 ID 数组，0 至 100 项，空数组清空草稿）、`closed_loop`（可选布尔，默认 `false`）；`request_id`。

```json
{"route_edit_id":"edit-a","expected_route_version":1,"map_fixture_id":"map-a","waypoint_fixture_ids":["point-a","point-b"],"closed_loop":false,"request_id":"00000000-0000-4000-8000-000000000019"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000019",
  "mission_id": "mission-a",
  "route_edit_id": "edit-a",
  "route_version": 2,
  "content_revision": 3,
  "state": "saved",
  "waypoint_count": 2,
  "edit_open": true
}
```

**错误处理**　`FIXTURE_NOT_FOUND`：核对授权夹具及其点 ID。`VERSION_CONFLICT`：重读已保存版本。`CONFIGURATION_EXPIRED`：恢复配置和编辑会话。

**验收**　保存后同一编辑会话仍有效，点序和快照一致；外部点 ID 被拒绝。空数组保存后完成检查仍适用。

#### `end_mock_route_edit`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`route_edit_id`（必填 ID）；`disposition`（必填，`finish` 或 `discard_unsaved`）；`expected_route_version`（`finish` 必填，`discard_unsaved` 省略）；`request_id`。

```json
{"route_edit_id":"edit-a","disposition":"finish","expected_route_version":2,"request_id":"00000000-0000-4000-8000-000000000020"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000020",
  "mission_id": "mission-a",
  "route_edit_id": "edit-a",
  "route_version": 2,
  "edit_open": false,
  "disposition": "finish"
}
```

**错误处理**　`CHECK_FAILED`：完成前修正当前航线。`VERSION_CONFLICT`：读取最新保存状态。`CONFIGURATION_EXPIRED`：重查占用；旧会话不结束后来建立的会话。

**验收**　完成后释放航线编辑，父配置仍有效。放弃未保存修改保留最后一次保存结果；旧句柄不影响新编辑。

#### `preview_mock_plan_translation`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`plan_id`、`configuration_id`、`translation_fixture_id`（必填 ID）、`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","translation_fixture_id":"shift-a","expected_plan_version":5,"request_id":"00000000-0000-4000-8000-000000000021"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000021",
  "plan_id": "plan-a",
  "preview_id": "plan-preview-a",
  "base_plan_version": 5,
  "state": "ready",
  "affected_mission_ids": ["mission-a"],
  "checks": [],
  "expires_at": "2026-11-23T05:03:00Z"
}
```

**错误处理**　`FIXTURE_NOT_FOUND`：选用工作区夹具。`EDIT_IN_PROGRESS`：先结束冲突航线编辑。`VERSION_CONFLICT`：刷新方案。预览计算发现业务阻塞时返回 `state: blocked` 与检查项。

**验收**　预览给出影响任务但保存版本和执行数不变；阻塞预览不可提交。

#### `commit_mock_plan_translation`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`preview_id`、`approval_id`（必填 ID）；`request_id`。

```json
{"preview_id":"plan-preview-a","approval_id":"approval-a","request_id":"00000000-0000-4000-8000-000000000022"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000022",
  "plan_id": "plan-a",
  "preview_id": "plan-preview-a",
  "plan_version": 6,
  "content_revision": 4,
  "updated_mission_ids": ["mission-a"],
  "state": "committed"
}
```

**错误处理**　`PREVIEW_EXPIRED` 或 `PREVIEW_STALE`：重做预览并重新确认。`PREVIEW_BLOCKED`：先解决检查项。`APPROVAL_INVALID`：重新绑定当前预览确认。

**验收**　只更新预览指定的草稿航线，提交后预览会话关闭，旧部署快照不变；预览之后变更方案内容时拒绝。

#### `cancel_mock_plan_translation`

**权限**　`mock:read` + `mock:route:edit`。

**参数**　`preview_id`（必填 ID）；`request_id`。

```json
{"preview_id":"plan-preview-a","request_id":"00000000-0000-4000-8000-000000000023"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000023",
  "plan_id": "plan-a",
  "preview_id": "plan-preview-a",
  "state": "canceled"
}
```

**错误处理**　`NOT_FOUND`：重新核对自己的预览。`INVALID_STATE`：已提交的预览不能以取消撤回。`PREVIEW_EXPIRED`：刷新当前编辑状态。

**验收**　取消释放本预览占用，方案和执行记录保持。已提交预览不会被取消请求逆转。

### 检查复核与 Mock 执行契约

执行 `kind` 为 `mission`、`fleet_move` 或 `single_takeover`。`state` 为 `scheduled`、`starting`、`executing`、`paused`、`returning`、`completed`、`failed`、`aborted` 或 `canceled`。执行 `version` 随状态、排程或成员变化递增，进度采样使用同一版本。无关联方案、任务或部署的执行使用 `null`，`scheduled_for` 仅在待开始时有值。无结果原因时 `reason_code` 为 `null`。预约使用工作区健康接口中的模拟时钟。

#### `validate_mock_plan`

**权限**　`mock:read` + `mock:plan:validate`。

**参数**　`plan_id`、`configuration_id`（必填 ID）、`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","expected_plan_version":5,"request_id":"00000000-0000-4000-8000-000000000024"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000024",
  "plan_id": "plan-a",
  "validation_id": "check-a",
  "plan_version": 6,
  "content_revision": 3,
  "state": "ready",
  "passed": true,
  "checks": []
}
```

**错误处理**　`VERSION_CONFLICT`：重读方案。`CONFIGURATION_EXPIRED`：恢复配置。完成检查但不通过时返回 `passed: false`、`state: draft` 和阻塞项。

**验收**　合格夹具返回对应修订的通过记录；缺航线夹具返回检查失败且无执行；以旧修订复核或部署时拒绝。

#### `record_mock_plan_review`

**权限**　`mock:read` + `mock:plan:review`。

**参数**　`plan_id`、`configuration_id`、`validation_id`、`approval_id`（必填 ID）；`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","validation_id":"check-a","approval_id":"approval-a","expected_plan_version":6,"request_id":"00000000-0000-4000-8000-000000000025"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000025",
  "plan_id": "plan-a",
  "review_id": "review-a",
  "validation_id": "check-a",
  "plan_version": 7,
  "content_revision": 3,
  "state": "ready"
}
```

**错误处理**　`CHECK_FAILED`：当前修订缺通过的检查。`VERSION_CONFLICT`：刷新方案。`APPROVAL_INVALID`：取得绑定当前修订的确认。

**验收**　只记录操作者确认的修订；同键保存一次，之后修改草稿使旧复核不能部署。

#### `list_mock_plan_deployments`

**权限**　`mock:read`。

**参数**　`plan_id`（必填 ID）；`limit`、`cursor`（分页）。

```json
{"plan_id":"plan-a","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "plan_id": "plan-a",
  "items": [{"deployment_id":"dep-a","content_revision":3,"created_at":"2026-11-23T05:01:00Z","mission_count":1}],
  "next_cursor": null
}
```

**错误处理**　`NOT_FOUND`：刷新方案列表。`CURSOR_INVALID`：从首批重查。

**验收**　部署后列出新快照；重开、改稿、再部署后，旧快照仍保留其原修订。

#### `publish_mock_plan_snapshot`

**权限**　`mock:read` + `mock:plan:deploy`。

**参数**　`plan_id`、`configuration_id`、`review_id`、`approval_id`（必填 ID）、`expected_plan_version`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","configuration_id":"cfg-a","review_id":"review-a","approval_id":"approval-a","expected_plan_version":7,"request_id":"00000000-0000-4000-8000-000000000027"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000027",
  "plan_id": "plan-a",
  "deployment_id": "dep-a",
  "plan_version": 8,
  "content_revision": 3,
  "state": "deployed",
  "configuration_released": true
}
```

**错误处理**　`REVIEW_REQUIRED`：重新检查与复核。`VERSION_CONFLICT`：重读方案。`APPROVAL_REQUIRED` 或 `APPROVAL_INVALID`：重新确认。

**验收**　冻结快照并释放本配置占用，部署前后模拟执行数一致；重复提交只留一份部署记录。

#### `list_mock_executions`

**权限**　`mock:read`。

**参数**　`plan_id`、`mission_id`（可选 ID），`state`（可选执行状态）；`limit`、`cursor`（分页）。

```json
{"plan_id":"plan-a","state":"scheduled","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"execution_id":"exec-a","kind":"mission","plan_id":"plan-a","mission_id":"mission-a","state":"scheduled","version":1,"scheduled_for":"2026-11-23T05:10:00Z"}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正状态过滤。`CURSOR_INVALID`：相同过滤从首批查。

**验收**　预约过滤只含预约，不跨工作区；没有对应状态时返回空数组。

#### `get_mock_execution`

**权限**　`mock:read`。

**参数**　`execution_id`（必填 ID）。

```json
{"execution_id":"exec-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "execution_id": "exec-a",
  "kind": "mission",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "deployment_id": "dep-a",
  "state": "paused",
  "version": 4,
  "scheduled_for": null,
  "progress_pct": 40,
  "elapsed_ms": 18000,
  "members": [{"device_id":"sim-a","state":"paused"}],
  "reason_code": null
}
```

**错误处理**　`NOT_FOUND`：重新列出执行。`MOCK_UNAVAILABLE`：稍后读权威快照。

**验收**　同一执行暂停期间进度稳定；终态含可用的结果原因且成员与 Backend 快照一致。

#### `request_mock_execution`

**权限**　`mock:read` + `mock:execution:start`。

**参数**　`deployment_id`、`mission_id`、`approval_id`（必填 ID）；`mode`（必填，`now` 或 `scheduled`）；`scheduled_for`（时间，预约必填且晚于模拟时钟，立即开始须省略）；`request_id`。

```json
{"deployment_id":"dep-a","mission_id":"mission-a","approval_id":"approval-a","mode":"scheduled","scheduled_for":"2026-11-23T05:10:00Z","request_id":"00000000-0000-4000-8000-000000000030"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000030",
  "execution_id": "exec-a",
  "deployment_id": "dep-a",
  "mission_id": "mission-a",
  "state": "scheduled",
  "version": 1,
  "scheduled_for": "2026-11-23T05:10:00Z"
}
```

**错误处理**　`INVALID_STATE`：重新检查部署与任务。`OBJECT_IN_USE`：检查模拟设备占用。`APPROVAL_INVALID`：重新确认方式与时间。

**验收**　预约返回执行 ID，模拟时钟未到时不进入运行；立即模式省略时间并单独确认；两种模式均只改变隔离模拟记录。

#### `update_mock_execution_schedule`

**权限**　`mock:read` + `mock:execution:schedule`。

**参数**　`execution_id`（必填 ID）、`expected_version`（必填版本）、`scheduled_for`（必填时间，晚于模拟时钟）；`request_id`。

```json
{"execution_id":"exec-a","expected_version":1,"scheduled_for":"2026-11-23T05:20:00Z","request_id":"00000000-0000-4000-8000-000000000031"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000031",
  "execution_id": "exec-a",
  "state": "scheduled",
  "version": 2,
  "scheduled_for": "2026-11-23T05:20:00Z"
}
```

**错误处理**　`VERSION_CONFLICT`：重读执行。`INVALID_STATE`：仅待开始预约可重排。时间无效时返回 `INVALID_ARGUMENT`。

**验收**　仅预约时间变化，执行 ID 不变；使用该权限只能重排，立即开始须另有对应权限。

#### `start_scheduled_mock_execution`

**权限**　`mock:read` + `mock:execution:start`。

**参数**　`execution_id`、`approval_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"execution_id":"exec-a","approval_id":"approval-a","expected_version":2,"request_id":"00000000-0000-4000-8000-000000000032"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000032",
  "execution_id": "exec-a",
  "state": "starting",
  "version": 3,
  "scheduled_for": null
}
```

**错误处理**　`INVALID_STATE`：重查预约是否已开始或取消。`VERSION_CONFLICT`：刷新后重新确认。`APPROVAL_INVALID`：获取立即开始的确认。

**验收**　沿用预约 ID，不另建执行；相同排程权限无开始权限时拒绝。状态仍以随后查询为准。

#### `cancel_mock_execution_schedule`

**权限**　`mock:read` + `mock:execution:schedule`。

**参数**　`execution_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"execution_id":"exec-a","expected_version":2,"request_id":"00000000-0000-4000-8000-000000000033"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000033",
  "execution_id": "exec-a",
  "state": "canceled",
  "version": 3
}
```

**错误处理**　`INVALID_STATE`：只限尚未开始的预约。`VERSION_CONFLICT`：重读执行。

**验收**　取消后历史保留并释放该预约的 Mock 占用；已开始任务保持现状。

#### `set_mock_execution_state`

**权限**　`mock:read` + `mock:execution:control`。

**参数**　`execution_id`（必填 ID）、`expected_version`（必填版本）、`action`（必填，`pause`、`resume`、`return`、`abort`）；`approval_id`（`abort` 必填，其余省略）；`request_id`。

```json
{"execution_id":"exec-a","expected_version":3,"action":"pause","request_id":"00000000-0000-4000-8000-000000000034"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000034",
  "execution_id": "exec-a",
  "state": "paused",
  "version": 4
}
```

**错误处理**　`INVALID_STATE`：核对执行类型及允许的状态转换。`VERSION_CONFLICT`：重读状态。`APPROVAL_REQUIRED` 或 `APPROVAL_INVALID`：终止需人工确认。

**验收**　暂停后进度停止，恢复后继续；模拟返航与终止遵照目标执行类型，已终止不能恢复，其他组状态不变。

### 模拟设备与集群演示契约

`telemetry_source` 为 `synthetic`、`recorded` 或 `unavailable`。查询保留可核对的观测时间。模拟接管与集群移动使用合成夹具 ID，提交后的记录通过执行查询及状态工具读取。

#### `list_mock_devices`

**权限**　`mock:read`。

**参数**　`kind`（可选，`simulated` 或 `test_fixture`）；`availability`（可选，`available`、`busy` 或 `offline`）；`limit`、`cursor`（分页）。

```json
{"kind":"simulated","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"device_id":"sim-a","name":"Simulator A","kind":"simulated","availability":"available","version":1}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正筛选值。`CURSOR_INVALID`：从首批重新查询。

**验收**　只显示授权的模拟设备和测试条目，物理注册表条目不出现在快照。

#### `get_mock_device_status`

**权限**　`mock:read`。

**参数**　`device_id`（必填 ID）。

```json
{"device_id":"sim-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "device_id": "sim-a",
  "name": "Simulator A",
  "label": "",
  "kind": "simulated",
  "availability": "busy",
  "version": 1,
  "telemetry_source": "synthetic",
  "telemetry_observed_at": "2026-11-23T05:01:59Z",
  "position_fixture_id": "point-a",
  "mission_id": "mission-a",
  "execution_id": "exec-a",
  "pilot_handoff_active": false
}
```

**错误处理**　`NOT_FOUND`：重新列出工作区设备。缺少来源观测时，观测时间及位置为 `null`。

**验收**　读取合成来源和观测时间；没有样本时保留空值且不推断在线，忙碌状态与执行快照一致。

#### `create_mock_simulated_devices`

**权限**　`mock:read` + `mock:fleet:manage`。

**参数**　`device_fixture_id`（必填 ID）、`count`（必填整数，1 至 20）；`request_id`。

```json
{"device_fixture_id":"device-setup-a","count":2,"request_id":"00000000-0000-4000-8000-000000000037"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000037",
  "devices": [{"device_id":"sim-a","name":"Simulator A","kind":"simulated","version":1},{"device_id":"sim-b","name":"Simulator B","kind":"simulated","version":1}],
  "created_count": 2
}
```

**错误处理**　`FIXTURE_NOT_FOUND`：选用测试管理员提供的设备夹具。`INVALID_ARGUMENT`：修正数量。`LIMIT_EXCEEDED`：检查工作区配额后另行发起。

**验收**　数量为 2 时仅生成两条模拟记录；同键重试返回同一组 ID，配额不足时整批不写入。

#### `update_mock_device_metadata`

**权限**　`mock:read` + `mock:fleet:manage`。

**参数**　`device_id`（必填 ID）、`expected_version`（必填版本）；`name`（可选名称）、`label`（可选字符串，最多 160 UTF-8 字节，空串清空），至少一项；`request_id`。

```json
{"device_id":"sim-a","expected_version":1,"name":"Demo simulator A","request_id":"00000000-0000-4000-8000-000000000038"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000038",
  "device_id": "sim-a",
  "name": "Demo simulator A",
  "label": "",
  "version": 2
}
```

**错误处理**　`NOT_FOUND`：核查目标属于测试注册表。`VERSION_CONFLICT`：重读当前展示信息。`INVALID_ARGUMENT`：修正字段或长度。

**验收**　变更展示名称不改变设备身份、模拟观测或任务状态；额外连接配置字段被拒绝。

当前可编辑的对象为 `kind: simulated`。`test_fixture` 来自管理员的 Backend 测试配置，可查询状态。

#### `remove_mock_test_device`

**权限**　`mock:read` + `mock:fleet:manage`。

**参数**　`device_id`、`approval_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"device_id":"sim-a","approval_id":"approval-a","expected_version":2,"request_id":"00000000-0000-4000-8000-000000000039"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000039",
  "device_id": "sim-a",
  "removed": true
}
```

**错误处理**　`OBJECT_IN_USE`：核对任务引用、预约、活动执行和控制权。`APPROVAL_INVALID`：重新确认当前测试条目。`VERSION_CONFLICT`：重读测试条目。

**验收**　有未解除的业务占用时拒绝；删除后列表排除目标，历史执行仍能显示原记录。

#### `preview_mock_fleet_move`

**权限**　`mock:read` + `mock:fleet:preview`。

**参数**　`device_ids`（必填唯一 ID 数组，1 至 20 项）、`motion_fixture_id`（必填 ID）；`request_id`。

```json
{"device_ids":["sim-a","sim-b"],"motion_fixture_id":"motion-a","request_id":"00000000-0000-4000-8000-000000000040"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000040",
  "preview_id": "fleet-preview-a",
  "kind": "fleet_move",
  "state": "ready",
  "device_ids": ["sim-a","sim-b"],
  "checks": [],
  "expires_at": "2026-11-23T05:03:00Z"
}
```

**错误处理**　`FIXTURE_NOT_FOUND`：选用授权的合成移动夹具。`CONFIGURATION_IN_PROGRESS`：待当前配置结束。存在模拟业务阻塞时返回 `state: blocked` 和检查项。

**验收**　预览不改变设备状态；一台及多台夹具均可预览，阻塞行之后不能提交。

#### `confirm_mock_fleet_move`

**权限**　`mock:read` + `mock:fleet:simulate`。

**参数**　`preview_id`、`approval_id`（必填 ID）；`request_id`。

```json
{"preview_id":"fleet-preview-a","approval_id":"approval-a","request_id":"00000000-0000-4000-8000-000000000041"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000041",
  "preview_id": "fleet-preview-a",
  "execution_id": "move-a",
  "state": "starting",
  "version": 1,
  "device_ids": ["sim-a","sim-b"]
}
```

**错误处理**　`PREVIEW_EXPIRED` 或 `PREVIEW_STALE`：重做预览。`PREVIEW_BLOCKED`：查看检查项。`APPROVAL_INVALID`：重新确认有效预览。

**验收**　只创建一条符合预览范围的模拟移动记录；占用变化后旧预览提交失败且无新执行。

#### `preview_mock_single_takeover`

**权限**　`mock:read` + `mock:fleet:preview`。

**参数**　`execution_id`、`device_id`、`motion_fixture_id`（必填 ID）、`expected_execution_version`（必填版本）；`request_id`。

```json
{"execution_id":"exec-a","device_id":"sim-a","motion_fixture_id":"motion-a","expected_execution_version":4,"request_id":"00000000-0000-4000-8000-000000000042"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000042",
  "preview_id": "takeover-preview-a",
  "kind": "single_takeover",
  "state": "ready",
  "source_execution_id": "exec-a",
  "affected_device_id": "sim-a",
  "unaffected_device_ids": ["sim-b"],
  "checks": [],
  "expires_at": "2026-11-23T05:03:00Z"
}
```

**错误处理**　`INVALID_STATE`：重查模拟执行及成员资格。`VERSION_CONFLICT`：刷新原执行。`FIXTURE_NOT_FOUND`：重新选合成夹具。

**验收**　预览准确区分受影响设备与其他成员；预览及超时均不改变原模拟执行。

#### `confirm_mock_single_takeover`

**权限**　`mock:read` + `mock:fleet:simulate`。

**参数**　`preview_id`、`approval_id`（必填 ID）；`request_id`。

```json
{"preview_id":"takeover-preview-a","approval_id":"approval-a","request_id":"00000000-0000-4000-8000-000000000043"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000043",
  "preview_id": "takeover-preview-a",
  "source_execution_id": "exec-a",
  "execution_id": "takeover-a",
  "state": "starting",
  "version": 1,
  "device_id": "sim-a",
  "unaffected_device_ids": ["sim-b"]
}
```

**错误处理**　`PREVIEW_STALE` 或 `PREVIEW_EXPIRED`：重做预览。`PREVIEW_BLOCKED`：先处理检查项。`APPROVAL_INVALID`：重新确认本次预览。

**验收**　同一隔离事务仅切出目标模拟成员并创建独立记录；其他成员继续原任务，失败时原状态保持，结束后目标不会自动加入旧任务。

#### `cancel_mock_fleet_preview`

**权限**　`mock:read` + `mock:fleet:preview`。

**参数**　`preview_id`（必填 ID，移动或接管预览）；`request_id`。

```json
{"preview_id":"fleet-preview-a","request_id":"00000000-0000-4000-8000-000000000044"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000044",
  "preview_id": "fleet-preview-a",
  "kind": "fleet_move",
  "state": "canceled"
}
```

**错误处理**　`NOT_FOUND`：核对预览归属。`INVALID_STATE`：已提交预览应通过执行状态工具处理。`PREVIEW_EXPIRED`：结束本地预览交互。

**验收**　预览取消后不可提交且业务快照不变；已提交的模拟执行不受取消影响。

### 任务预演与结果复核契约

预演状态为 `starting`、`executing`、`completed`、`failed` 或 `aborted`，复核状态为 `pending`、`approved` 或 `stale`。未归档轨迹时 `record_id` 为 `null`，无结果原因时 `reason_code` 为 `null`。

#### `list_mock_rehearsals`

**权限**　`mock:read`。

**参数**　`plan_id`、`mission_id`（可选 ID）；`state`（可选预演状态）；`limit`、`cursor`（分页）。

```json
{"mission_id":"mission-a","state":"completed","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"rehearsal_id":"reh-a","plan_id":"plan-a","mission_id":"mission-a","state":"completed","version":4,"review_state":"pending","record_id":"record-a"}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正过滤。`CURSOR_INVALID`：从首批重查。

**验收**　列表按任务和状态过滤，运行中 `record_id` 为 `null`，结束并归档后可关联回放目录。

#### `get_mock_rehearsal`

**权限**　`mock:read`。

**参数**　`rehearsal_id`（必填 ID）。

```json
{"rehearsal_id":"reh-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "rehearsal_id": "reh-a",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "state": "completed",
  "version": 4,
  "input_content_revision": 3,
  "review_state": "pending",
  "record_id": "record-a",
  "members": [{"device_id":"sim-a","state":"completed","progress_pct":100}],
  "reason_code": null
}
```

**错误处理**　`NOT_FOUND`：重新发现可读预演。`MOCK_UNAVAILABLE`：稍后重读快照。

**验收**　完结记录返回各成员结果和归档记录 ID；任务变更后复核状态可显示 `stale`，历史采样保留。

#### `start_mock_rehearsal`

**权限**　`mock:read` + `mock:rehearsal:run`。

**参数**　`plan_id`、`mission_id`（必填 ID）、`expected_content_revision`（必填版本）；`request_id`。

```json
{"plan_id":"plan-a","mission_id":"mission-a","expected_content_revision":3,"request_id":"00000000-0000-4000-8000-000000000047"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000047",
  "rehearsal_id": "reh-a",
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "state": "starting",
  "version": 1,
  "input_content_revision": 3,
  "review_state": "pending"
}
```

**错误处理**　`CHECK_FAILED`：修正 Mock 任务输入或缺失夹具。`OBJECT_IN_USE`：等待模拟资源可用。`VERSION_CONFLICT`：重读任务。

**验收**　用纯软件夹具启动；同键只有一份预演，明确新发起的重新预演产生新 ID，旧结果与回放保留。

Backend 刚接受预演时回执为 `created`，版本为 0。后续状态和结果通过 `get_mock_rehearsal` 查询。

#### `record_mock_rehearsal_review`

**权限**　`mock:read` + `mock:rehearsal:review`。

**参数**　`rehearsal_id`、`approval_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"rehearsal_id":"reh-a","approval_id":"approval-a","expected_version":4,"request_id":"00000000-0000-4000-8000-000000000048"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000048",
  "rehearsal_id": "reh-a",
  "state": "completed",
  "version": 5,
  "review_state": "approved",
  "reviewed_at": "2026-11-23T05:02:00Z"
}
```

**错误处理**　`REHEARSAL_STALE`：按新任务版本重跑。`INVALID_STATE`：等待所有模拟成员完成。`APPROVAL_INVALID`：重新确认对应记录。

**验收**　全部成员完成且输入修订一致才记录复核；修改任务后拒绝旧结果复核。响应和日志均属模拟工作区。

### 态势上下文告警日志与视频契约

上下文、语言、视频和告警各有独立版本。视频的 `playback_state` 为 `loading`、`playing`、`no_source`、`failed`、`idle` 或 `unreported`。健康 `state` 为 `ready`、`degraded` 或 `offline`，客户端连接状态为 `connected` 或 `disconnected`。业务事件与视频回执仅提供脱敏信息。

#### `get_mock_workspace_health`

**权限**　`mock:read`。

**参数**　空对象 `{}`。

```json
{}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "workspace_id": "ws-a",
  "version": 1,
  "state": "ready",
  "backend_reachable": true,
  "last_backend_observed_at": "2026-11-23T05:01:59Z",
  "simulation_time": "2026-11-23T05:02:00Z",
  "clients": [{"role":"command","connection_state":"connected"},{"role":"map","connection_state":"connected"}],
  "reset_request": null
}
```

**错误处理**　`MOCK_UNAVAILABLE`：服务无法确认当前授权工作区时稍后重查。可确认断连时返回 `state: offline`，无法取得的观测字段为 `null`。

**验收**　断开测试 Backend 后报告断连或不可用；恢复后返回新观测时间。成功快照只显示授权客户端角色。

#### `get_mock_workspace_context`

**权限**　`mock:read`。

**参数**　`session_id`（必填 ID）。

```json
{"session_id":"session-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "session_id": "session-a",
  "version": 3,
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "device_ids": ["sim-a"],
  "primary_device_id": "sim-a",
  "video_target_device_id": null
}
```

**错误处理**　`NOT_FOUND`：核对授权演示会话。`SESSION_EXPIRED`：重新进入演示会话。

**验收**　快照反映当前选中对象，清空后保留空数组和空 ID。另一会话的选择不可见。

#### `set_mock_workspace_selection`

**权限**　`mock:read` + `mock:context:select`。

**参数**　`session_id`（必填 ID）、`expected_version`（必填版本）；`plan_id`、`mission_id`、`primary_device_id`（必填，可为 `null`）；`device_ids`（必填唯一 ID 数组，0 至 20 项）；`request_id`。

```json
{"session_id":"session-a","expected_version":3,"plan_id":"plan-a","mission_id":"mission-a","device_ids":["sim-a","sim-b"],"primary_device_id":"sim-a","request_id":"00000000-0000-4000-8000-000000000051"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000051",
  "session_id": "session-a",
  "version": 4,
  "plan_id": "plan-a",
  "mission_id": "mission-a",
  "device_ids": ["sim-a","sim-b"],
  "primary_device_id": "sim-a",
  "video_target_device_id": null
}
```

**错误处理**　`INVALID_ARGUMENT`：核对任务归属和主选设备是否在集合。`VERSION_CONFLICT`：读当前选择再更新。`SESSION_EXPIRED`：恢复演示会话。

**验收**　全量替换选择。空选择时传空数组与 `null`。该工具既不启动模拟任务也不切换视频；原生 Map 的显式选机联动单独验证。

#### `get_mock_workspace_preferences`

**权限**　`mock:read`。

**参数**　`session_id`（必填 ID）。

```json
{"session_id":"session-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "session_id": "session-a",
  "version": 2,
  "language": "zh-Hans"
}
```

**错误处理**　`NOT_FOUND` 或 `SESSION_EXPIRED`：核对并恢复演示会话。

**验收**　读取当前共享语言，同一工作区的另一会话仍保留自己的设置。

#### `set_mock_workspace_language`

**权限**　`mock:read` + `mock:context:preferences`。

**参数**　`session_id`（必填 ID）、`expected_version`（必填版本）、`language`（必填，`zh-Hans` 或 `en`）；`request_id`。

```json
{"session_id":"session-a","expected_version":2,"language":"en","request_id":"00000000-0000-4000-8000-000000000053"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000053",
  "session_id": "session-a",
  "version": 3,
  "language": "en"
}
```

**错误处理**　`INVALID_ARGUMENT`：修正语言值。`VERSION_CONFLICT`：重新读取偏好。`SESSION_EXPIRED`：重新进入演示会话。

**验收**　设置后偏好回读一致，方案版本不变。客户端间的显示语言同步另做原生 UI 验收。

#### `list_mock_session_alerts`

**权限**　`mock:read`。

**参数**　`session_id`（必填 ID）；`severity`（可选，`info`、`warning` 或 `critical`）、`handled`（可选布尔）；`limit`、`cursor`（分页）。

```json
{"session_id":"session-a","severity":"warning","handled":false,"limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "session_id": "session-a",
  "items": [{"alert_id":"alert-session-a","version":1,"severity":"warning","code":"DEVICE_OFFLINE","device_id":"sim-a","occurred_at":"2026-11-23T05:01:35Z","handled":false}],
  "next_cursor": null
}
```

**错误处理**　`SESSION_EXPIRED`：重新进入演示会话。`CURSOR_INVALID`：从首批查。

**验收**　未处理过滤返回目标告警，处理后从该过滤结果消失；录制告警目录保持原值。

#### `acknowledge_mock_session_alert`

**权限**　`mock:read` + `mock:alerts:ack`。

**参数**　`session_id`、`alert_id`（必填 ID）、`expected_version`（必填版本）；`request_id`。

```json
{"session_id":"session-a","alert_id":"alert-session-a","expected_version":1,"request_id":"00000000-0000-4000-8000-000000000055"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000055",
  "session_id": "session-a",
  "alert_id": "alert-session-a",
  "version": 2,
  "handled": true
}
```

**错误处理**　`NOT_FOUND`：重新查询授权会话的告警。`VERSION_CONFLICT`：重新查询标记。`SESSION_EXPIRED`：恢复会话。

**验收**　只在当前会话将标记置为已处理。另一会话及历史录制保留原事件，同键重试回执相同。

#### `list_mock_operation_events`

**权限**　`mock:read`。

**参数**　`category`（可选，`plan`、`execution`、`rehearsal`、`fleet`、`video`、`handoff` 或 `system`），`object_id`（可选 ID），`from`、`to`（可选时间，前闭后开，二者同传时 `from` 小于 `to`）；`limit`、`cursor`（分页）。

```json
{"category":"plan","object_id":"plan-a","from":"2026-11-23T05:00:00Z","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"event_id":"event-a","category":"plan","object_id":"plan-a","code":"PLAN_CREATED","occurred_at":"2026-11-23T05:00:10Z","message":"Mock plan created"}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正时间范围或类别。`CURSOR_INVALID`：以同样条件重新查询。

**验收**　给定窗口只返回对应对象的脱敏业务事件；访问密钥和主机日志不会进入结果。

#### `list_mock_video_sources`

**权限**　`mock:read`。

**参数**　`device_id`（可选 ID）；`availability`（可选，`available` 或 `unavailable`）；`limit`、`cursor`（分页）。

```json
{"device_id":"sim-a","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"source_id":"video-fixture-a","device_id":"sim-a","title":"Demo fixture A","availability":"available"}],
  "next_cursor": null
}
```

**错误处理**　`NOT_FOUND`：重查模拟设备归属。`CURSOR_INVALID`：从首批重查。

**验收**　有源设备返回受控样例源 ID；无源设备返回空数组，可用条目不携带媒体地址。

#### `get_mock_video_view`

**权限**　`mock:read`。

**参数**　`session_id`（必填 ID）。

```json
{"session_id":"session-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "session_id": "session-a",
  "version": 2,
  "target_device_id": "sim-a",
  "source_id": "video-fixture-a",
  "playback_state": "loading",
  "playback_observed_at": "2026-11-23T05:01:59Z"
}
```

**错误处理**　`SESSION_EXPIRED`：恢复演示会话。缺少播放器观测时返回 `unreported` 和 `playback_observed_at: null`。

**验收**　同一目标在加载、已播放或无源时得到不同观测；目标存在不会把无观测误报为已播放。

#### `set_mock_video_target`

**权限**　`mock:read` + `mock:video:select`。

**参数**　`session_id`（必填 ID）、`expected_version`（必填版本）、`device_id`（必填 ID 或 `null`，空值清除）；`source_id`（可选 ID，设备不为空时可指定样例源，省略使用被选设备的默认源）；`request_id`。

```json
{"session_id":"session-a","expected_version":2,"device_id":"sim-b","request_id":"00000000-0000-4000-8000-000000000059"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000059",
  "session_id": "session-a",
  "version": 3,
  "target_device_id": "sim-b",
  "source_id": null,
  "playback_state": "no_source",
  "playback_observed_at": null
}
```

**错误处理**　`NOT_FOUND`：刷新设备和样例源。`INVALID_ARGUMENT`：指定源须属于该设备，清除时省略源 ID。`VERSION_CONFLICT`：读取当前视频视图。

**验收**　确认目标后共享视图回读同一设备；无源仍报告目标和无源状态。切换不会改任务或设备分配。

### 驾驶员交接与测试工作区契约

人工申请 `state` 为 `pending_operator`、`approved`、`rejected`、`expired` 或 `completed`。驾驶员 `authority_state` 为 `unassigned` 或 `assigned`，`session_state` 为 `inactive` 或 `active`。无历史申请时 `latest_request` 为 `null`。健康接口的 `reset_request` 有值时包含 `reset_request_id` 和申请状态。人工处理后以读取结果为准。

#### `list_mock_pilot_handoff_requests`

**权限**　`mock:read`。

**参数**　`device_id`（可选 ID）、`kind`（可选，`handoff` 或 `release`）、`state`（可选申请状态）；`limit`、`cursor`（分页）。

```json
{"device_id":"sim-a","kind":"handoff","state":"pending_operator","limit":20}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "items": [{"handoff_request_id":"handoff-a","kind":"handoff","device_id":"sim-a","state":"pending_operator","created_at":"2026-11-23T05:01:00Z"}],
  "next_cursor": null
}
```

**错误处理**　`INVALID_ARGUMENT`：修正筛选值。`CURSOR_INVALID`：从首批重新查询。

**验收**　按设备、类型和状态筛选；未授权工作区的申请不出现，人工处理后的状态可回读。

#### `get_mock_pilot_handoff_status`

**权限**　`mock:read`。

**参数**　`device_id`（必填 ID）。

```json
{"device_id":"sim-a"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "device_id": "sim-a",
  "version": 4,
  "authority_state": "assigned",
  "session_state": "inactive",
  "latest_request": {"handoff_request_id":"handoff-a","kind":"handoff","state":"approved"}
}
```

**错误处理**　`NOT_FOUND`：重新核对当前测试设备。权限不足时不披露交接对象。

**验收**　人工分配后为 `assigned`，Pilot 会话尚未启用仍为 `inactive`；交接成功和操控状态分别报告。

#### `request_mock_pilot_handoff`

**权限**　`mock:read` + `mock:pilot:request`。

**参数**　`device_id`（必填 ID）、`expected_version`（必填版本，来自交接状态）、`note`（可选说明，默认空串）；`request_id`。

```json
{"device_id":"sim-a","expected_version":3,"note":"Demo handoff review","request_id":"00000000-0000-4000-8000-000000000062"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000062",
  "handoff_request_id": "handoff-a",
  "device_id": "sim-a",
  "kind": "handoff",
  "state": "pending_operator",
  "version": 4
}
```

**错误处理**　`INVALID_STATE`：核对当前模拟设备的交接资格。`OBJECT_IN_USE`：存在待人工处理的同设备申请时查询其状态。`VERSION_CONFLICT`：刷新交接状态。

**验收**　写入一条待操作员处理的申请。重复请求返回同一申请；申请本身不会授予 Pilot 控制权。

#### `request_mock_pilot_handoff_release`

**权限**　`mock:read` + `mock:pilot:request`。

**参数**　`device_id`（必填 ID）、`expected_version`（必填版本，来自交接状态）、`note`（可选说明，默认空串）；`request_id`。

```json
{"device_id":"sim-a","expected_version":4,"note":"Demo release review","request_id":"00000000-0000-4000-8000-000000000063"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000063",
  "handoff_request_id": "release-a",
  "device_id": "sim-a",
  "kind": "release",
  "state": "pending_operator",
  "version": 5
}
```

**错误处理**　`INVALID_STATE`：没有可收回的授权时读交接状态。`OBJECT_IN_USE`：检查已有申请。`VERSION_CONFLICT`：重读交接状态。

**验收**　申请回执待操作员处理，当前授权保持；只有现有人工流程确认后，状态查询才显示授权及会话已退出。

#### `request_mock_workspace_reset`

**权限**　`mock:read` + `mock:sandbox:reset`。

**参数**　`workspace_id`（必填 ID，须等于本人专用测试工作区）、`expected_workspace_version`（必填版本，来自健康状态）、`reason`（必填非空说明）；`request_id`。

```json
{"workspace_id":"ws-a","expected_workspace_version":1,"reason":"Restore the demo fixture","request_id":"00000000-0000-4000-8000-000000000064"}
```

**成功返回**

```json
{
  "contract_version": "0.2",
  "simulation": true,
  "observed_at": "2026-11-23T05:02:00Z",
  "request_id": "00000000-0000-4000-8000-000000000064",
  "workspace_id": "ws-a",
  "reset_request_id": "reset-a",
  "workspace_version": 2,
  "state": "pending_operator"
}
```

**错误处理**　`INVALID_STATE`：组织共享或含活动业务占用的工作区不能受理。`OBJECT_IN_USE`：已有待确认重置。`VERSION_CONFLICT`：重读健康状态。HTTP 403：需要测试管理员角色及重置权限。

**验收**　专用工作区只生成一条待确认请求，业务数据保持；确认后才恢复初始夹具。重置前的重复键和旧工作区版本不会再触发一次重置。

## 联调步骤

1. **准备客户端。** 服务交付前可用本地桩返回本文样例，先验证工具选择、参数生成与结果解析。
2. **连接 Mock 服务。** 本地用 `stdio` 启动交付的服务进程。跨机器使用团队提供的内网 HTTPS Streamable HTTP 地址和独立测试身份。双方确认 SDK、协议版本及 OAuth 授权。
3. **先直调，再接模型。** 通过 MCP SDK 发 `tools/list` 核对权限和 schema，再用 `tools/call` 传工具名与相应的 `arguments`。使用 MCP `2026-07-28` 时由 SDK 发送逐请求元数据。若采用 `2025-11-25`，须双方确认支持，由 SDK 完成初始化握手。
4. **呈现已确认结果。** 先处理传输、协议和工具错误，再读取 `structuredContent`。状态和结果用模拟数据标记。模型只在后端确认后报告写入成功。

### 错误与恢复

| 结果 | 客户端处理 |
| --- | --- |
| HTTP 401 / 403 | 401 时重新认证。403 时核对调用身份的工具权限。 |
| JSON-RPC 错误 | 检查协议版本、请求格式和工具名称。 |
| `INVALID_ARGUMENT` | 按 schema 修正缺失字段、类型、长度或枚举值。 |
| `SCENARIO_NOT_FOUND` / `REPLAY_NOT_FOUND` | 重新发现场景或查询当前身份的回放。 |
| `REVISION_CONFLICT` | 重新查询回放版本和状态，再决定是否重试。 |
| `REQUEST_ID_CONFLICT` | 重试原创建时使用原参数。为用户明确发起的新创建动作生成新键。 |
| `CONFIGURATION_IN_PROGRESS` | 告知用户已有配置会话。待会话结束后，用原键和参数重试。 |
| `DRAFT_HANDOFF_PENDING` | 草稿交接待确认。用原键和参数重试以恢复交接。 |
| `MOCK_UNAVAILABLE` / 调用超时 | 查询可有限重试。回放写入先查询状态。草稿写入沿用原键和参数。 |

业务错误的工具结果含 `isError: true`，从 `structuredContent.error.code` 读取错误码。工具输出 schema 同时定义成功与业务错误负载。

## 联调验收

从一致的 Mock 快照开始。先用 SDK 直调，再用 LLM 端到端验证。仅启用原有五项的兼容夹具中，只读身份发现 3 个查询工具，回放和草稿权限分别看到对应写工具。全量夹具按本页权限表核对 `tools/list`。

### 读取与回放验收

| 用例 | 通过标准 |
| --- | --- |
| 场景和状态 | 列表含 `demo-a`。状态返回 `plan-a`、已完成任务 1/2、在线设备 1/2。 |
| 告警 | `warning` 返回 `alert-a-01`，`critical` 返回空数组。`limit=0` 返回 `INVALID_ARGUMENT`。 |
| 暂停与恢复 | 用版本 3 暂停，查询返回 `paused` 和版本 4，两次读到的进度相同。用版本 4 恢复后变为 `playing`、版本 5，进度继续。 |
| 冲突与隔离 | 提交旧版本收到 `REVISION_CONFLICT`。只读身份直接请求回放写入，HTTP 返回 403。跨身份的回放 ID 被拒绝。 |
| LLM 端到端 | 输入「暂停演示 A 的回放，告诉我这个场景有哪些告警」。等暂停确认后报告 1 条模拟设备离线告警。 |

### 草稿创建验收

| 用例 | 通过标准 |
| --- | --- |
| 成功与交接 | 授权身份创建后，Backend 只新增 1 份 `DRAFT`，任务为空，MCP 的占用已结束，执行记录数不变。原生指挥端能用同一个 `plan_id` 打开草稿。 |
| 同键重试与恢复 | 同键同参连续提交、并发提交，或在创建落盘后模拟响应丢失并重启服务再重试，均返回同一 `plan_id`，草稿总数只增加 1。 |
| 改参及权限 | 同键改参返回 `REQUEST_ID_CONFLICT`。仅只读或仅有回放写权限的身份看不到草稿工具，HTTP 直调返回 403。空白名称返回 `INVALID_ARGUMENT`。 |
| 活动配置占用 | 指挥端持有占用时调用，返回 `CONFIGURATION_IN_PROGRESS`，方案总数和原占用不变。会话结束后原键重试成功。 |
| 释放失败与恢复 | 模拟落盘后释放未确认，收到 `DRAFT_HANDOFF_PENDING`。原键重试后草稿总数仍只增加 1。旧会话已失效且操作员获得新占用时，确认新会话仍有效。 |
| LLM 端到端 | 输入「创建一份周末活动安保演示草稿」。模型以名称调用，收到成功回执后给出 `plan_id` 和草稿状态。 |

指挥系统侧提供 Mock 入口、测试授权、数据版本和初始状态。LLM 侧记录 SDK 与协议版本、用户提示、工具名、参数、结构化结果和最终回复。协议/组件自动化附脱敏调用记录和 Backend 快照。原生 UI 证据另附截图及操作步骤。

## 协议参考

- [MCP 工具与错误规范](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- [MCP 传输方式](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports)
- [MCP HTTP 授权](https://modelcontextprotocol.io/specification/2026-07-28/basic/authorization)
- [MCP 版本兼容](https://modelcontextprotocol.io/specification/2026-07-28/basic/versioning)
