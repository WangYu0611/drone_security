# Drone Security Mock MCP 接入指南

接口契约 v0.2 · 2026-11-23

目标是让 LLM 通过 MCP 逐项使用指挥系统的业务功能。每个工具独立约定参数、权限和验收。v0.2 覆盖隔离 Mock 环境的场景与状态查询、告警查询、演示回放和安保方案草稿创建。

**当前进度**　项目已有 Stage 1 Mock 业务流程。本文定义待实现的业务 MCP 契约，联调地址和账号待服务部署后提供。服务交付前，模型侧可以用本文的固定样例测试工具选择、参数生成和结果解析。仓库中的 Unreal Editor MCP 属于编辑器工具。项目现状见 [演示手册](Stage1/Stage1-Demo-Runbook.md) 和 [编辑器 MCP 记录](TASK-A3-Startup-Visual.md#unreal-mcp-evidence-and-current-boundary)。

## 当前系统功能盘点

范围以 2026-11-23 的 `main` 为准，核对了 Command、Map、Video、Pilot 和 Backend。历史 Stage 1 Mock、现行模拟机预演和 Mock Pilot 测试都属于本目录的业务来源。当前 Launcher 默认实时入口，模拟能力是否可用取决于测试配置。这里记录代码、说明和已有验收的证据，MCP 运行及原生界面仍需按各自用例重新验证。 [历史功能审计](Stage1/Stage1-Feature-Audit.md) · [当前运行说明](First-Run-Windows.md) · [近期预演验收](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

## 工具与权限

目录共 **69 项**。**5 项已写入 v0.2 契约**，本页已有参数和示例。**64 项待补契约**，下列名称及权限是拟定接入项，需补 schema、状态变化、错误、权限验证和验收后才能接入。新增目录沿用 v0.2 的 `contract_version`。现阶段业务 MCP 待实现和部署。

权限按工具和工作区判定。每个工具均需 `mock:read`，写工具还需表中所列权限。服务在发现和调用时校验身份及对象归属。业务写入以隔离 Mock Backend 确认或工具所属会话的结果为准。复核、部署与开始各自校验操作员确认。模拟写入仅面向测试数据和软件仿真，无硬件出口。

### 录制场景与回放

固定录制场景用于客户端联调。业务执行和任务预演完成后产生的回放记录来自隔离 Mock 工作区。Map 已具备轨迹回放控件，近期验收记录覆盖了采样和显示，相关原生鼠标交互仍待验收。 [Stage 1 演示手册](Stage1/Stage1-Demo-Runbook.md) · [任务预演验收](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `list_demo_scenarios` | `mock:read` | 已写入 | 列出固定录制场景和 `scenario_id`。 |
| `get_sim_status` | `mock:read` | 已写入 | 读取录制场景中的方案、任务、设备及当前身份的回放摘要。 |
| `list_sim_alerts` | `mock:read` | 已写入 | 查询固定场景录制告警。 |
| `set_replay_state` | `mock:read` + `mock:replay` | 已写入 | 播放或暂停已分配的录制回放。 |
| `list_mock_replay_records` | `mock:read` | 待补 | 列出工作区内可回放的模拟执行与预演记录。 |
| `open_mock_record_replay` | `mock:read` + `mock:replay` | 待补 | 从工作区回放记录打开独立回放会话。 |
| `seek_mock_replay` | `mock:read` + `mock:replay` | 待补 | 定位到回放时间点。 |
| `close_mock_replay` | `mock:read` + `mock:replay` | 待补 | 关闭自己的回放会话。 |

若工作区回放要复用 `set_replay_state`，仍需补会话来源和版本校验约定。播放、暂停和定位均作用于回放进度。

### 安保方案与任务配置

Command 已支持方案与任务配置、草稿复制和删除、已部署方案重开。Backend 持有方案、任务、内容修订和配置占用。 [方案配置流程](PLAN-Configuration-Workflow.md) · [返回编辑与重部署](EditReturnVideo.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `list_mock_security_plans` | `mock:read` | 待补 | 列出工作区内的方案及草稿、就绪、已部署状态。 |
| `get_mock_security_plan` | `mock:read` | 待补 | 按方案 ID 读详情、任务列表、修订与当前配置占用摘要。 |
| `create_security_plan_draft` | `mock:read` + `mock:plan:draft` | 已写入 | 持久化空任务草稿并交接给指挥端。 |
| `begin_mock_plan_edit` | `mock:read` + `mock:plan:draft` | 待补 | 申请指定 Mock 方案的配置占用。 |
| `end_mock_plan_edit` | `mock:read` + `mock:plan:draft` | 待补 | 结束自己取得的配置占用，报告仍待保存或退出的编辑会话。 |
| `update_mock_security_plan_draft` | `mock:read` + `mock:plan:draft` | 待补 | 修改现有草稿的名称和说明。 |
| `copy_mock_security_plan_draft` | `mock:read` + `mock:plan:draft` | 待补 | 从可读方案复制新草稿及独立的任务和航线。 |
| `delete_mock_security_plan_draft` | `mock:read` + `mock:plan:delete` | 待补 | 经确认删除满足状态及占用条件的指定草稿。 |
| `reopen_mock_security_plan` | `mock:read` + `mock:plan:draft` | 待补 | 按现有门禁重开已部署方案以继续编辑。 |
| `add_mock_plan_mission` | `mock:read` + `mock:plan:draft` | 待补 | 在可编辑方案新增任务。 |
| `rename_mock_plan_mission` | `mock:read` + `mock:plan:draft` | 待补 | 修改指定任务名称。 |
| `delete_mock_plan_mission` | `mock:read` + `mock:plan:delete` | 待补 | 经确认删除指定草稿任务。 |
| `set_mock_mission_assignment` | `mock:read` + `mock:plan:draft` | 待补 | 设置、变更或清空任务中的 Mock 模拟成员。 |

### 地图航线与方案几何

Map 已支持航点编辑、保存、完成或放弃，以及方案整体移动预览和提交。每项任务在 Map 中应只有一个航线显示所有者。编辑期间可以手动平移、缩放和绕转，自动跟随与聚焦受编辑状态限制。 [方案几何](TASK-P5.3-PlanGeometryClosure.md) · [交互后续](UIInteractionFollowup.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `get_mock_mission_route` | `mock:read` | 待补 | 读给定任务的 Mock 航线摘要、版本和状态。 |
| `begin_mock_route_edit` | `mock:read` + `mock:route:edit` | 待补 | 为测试任务取得与配置会话绑定的航线编辑会话。 |
| `save_mock_route_draft` | `mock:read` + `mock:route:edit` | 待补 | 基于约定的合成地图夹具保存航线修改并保留编辑会话。 |
| `end_mock_route_edit` | `mock:read` + `mock:route:edit` | 待补 | 完成或放弃当前会话内的未保存修改。 |
| `preview_mock_plan_translation` | `mock:read` + `mock:route:edit` | 待补 | 预览方案整体平移在指定合成夹具中的结果与检查项。 |
| `commit_mock_plan_translation` | `mock:read` + `mock:route:edit` | 待补 | 提交经确认且版本仍有效的 Mock 平移预览。 |
| `cancel_mock_plan_translation` | `mock:read` + `mock:route:edit` | 待补 | 取消当前 Mock 整体平移预览并结束其编辑会话。 |

工具面向业务状态和合成场景。相机手势、地图命中和航线显示所有权仍由 Map 处理。

### 检查复核与 Mock 执行

Command 已有配置检查、操作员复核、部署快照、立即或预约开始，以及执行详情和历史。部署和开始是独立业务动作。以下写工具只管理隔离模拟数据，分别沿用人工确认和状态校验。 [现有演示流程](Stage1/Stage1-Demo-Runbook.md) · [部署与配置占用](PLAN-Configuration-Workflow.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `validate_mock_plan` | `mock:read` + `mock:plan:validate` | 待补 | 运行配置检查并返回通过、阻塞与当前内容修订。 |
| `record_mock_plan_review` | `mock:read` + `mock:plan:review` | 待补 | 记录操作员针对指定修订确认的模拟方案复核。 |
| `list_mock_plan_deployments` | `mock:read` | 待补 | 读取隔离工作区的模拟部署版本及快照摘要。 |
| `publish_mock_plan_snapshot` | `mock:read` + `mock:plan:deploy` | 待补 | 经操作员确认冻结经检查与复核的 Mock 配置快照。 |
| `list_mock_executions` | `mock:read` | 待补 | 按方案、任务或状态查预约、活动与历史模拟执行。 |
| `get_mock_execution` | `mock:read` | 待补 | 查询模拟执行进度、成员、耗时、状态和结果原因。 |
| `request_mock_execution` | `mock:read` + `mock:execution:start` | 待补 | 提交操作员已确认的立即或预约模拟任务，并返回模拟执行编号。 |
| `update_mock_execution_schedule` | `mock:read` + `mock:execution:schedule` | 待补 | 重排待开始的模拟任务。 |
| `start_scheduled_mock_execution` | `mock:read` + `mock:execution:start` | 待补 | 经操作员确认，立即开始指定的待启动模拟任务。 |
| `cancel_mock_execution_schedule` | `mock:read` + `mock:execution:schedule` | 待补 | 取消尚未开始的模拟预约并检查资源释放。 |
| `set_mock_execution_state` | `mock:read` + `mock:execution:control` | 待补 | 对指定 Mock 执行执行暂停、恢复、模拟返航或经确认终止，按对象类型和当前状态判定可用动作。 |

### 模拟设备与集群演示

当前存在设备列表与状态投影、批量新增模拟机，以及历史 Stage 1 Mock 的单机和集群移动、执行中单机接管。集群预览、提交和执行组控制是不同动作。 [交互后续](UIInteractionFollowup.md) · [集群交互](MAP-Fleet-Control-UX.md) · [单机接管演示](Stage1/Stage1-Demo-Runbook.md#执行中单机手动接管)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `list_mock_devices` | `mock:read` | 待补 | 列出隔离工作区的测试设备与模拟机。 |
| `get_mock_device_status` | `mock:read` | 待补 | 查看来源、观测时间、模拟位置、可用状态、当前任务与占用。 |
| `create_mock_simulated_devices` | `mock:read` + `mock:fleet:manage` | 待补 | 按已配置的合成夹具批量新增模拟机。 |
| `update_mock_device_metadata` | `mock:read` + `mock:fleet:manage` | 待补 | 修改隔离测试注册表的展示信息。 |
| `remove_mock_test_device` | `mock:read` + `mock:fleet:manage` | 待补 | 在无任务或控制权占用且经确认时移除测试注册表条目。 |
| `preview_mock_fleet_move` | `mock:read` + `mock:fleet:preview` | 待补 | 预览单机或集群在指定合成场景中的移动结果。 |
| `confirm_mock_fleet_move` | `mock:read` + `mock:fleet:simulate` | 待补 | 在隔离模拟器中提交经确认且未过期的移动预览。 |
| `preview_mock_single_takeover` | `mock:read` + `mock:fleet:preview` | 待补 | 预览单架模拟机从已有模拟任务切出的影响。 |
| `confirm_mock_single_takeover` | `mock:read` + `mock:fleet:simulate` | 待补 | 在隔离模拟器确认已预览的接管，并确认其余演示成员的状态。 |
| `cancel_mock_fleet_preview` | `mock:read` + `mock:fleet:preview` | 待补 | 撤销尚未提交的当前移动或接管预览。 |

暂停、恢复或终止已创建的演示移动使用 `set_mock_execution_state`。设备展示信息只涉及测试数据，模拟状态须携带来源和时间。

### 任务预演与结果复核

新增模拟机队支持在物理机队为空时完成任务预演，并持久化进度和回放采样。新版预演与历史 Stage 1 演示运行各有记录。 [启动器预演说明](../Launcher/README.md#mission-rehearsals) · [2026-11-20 验收记录](../Evidence/SimulationRehearsal/2026-11-20/validation.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `list_mock_rehearsals` | `mock:read` | 待补 | 列出任务的预演批次及复核状态。 |
| `get_mock_rehearsal` | `mock:read` | 待补 | 读取成员进度、结果、输入修订以及可回放的记录引用。 |
| `start_mock_rehearsal` | `mock:read` + `mock:rehearsal:run` | 待补 | 启动或重新运行仅含模拟机的测试预演。 |
| `record_mock_rehearsal_review` | `mock:read` + `mock:rehearsal:review` | 待补 | 记录操作员对已完成且输入版本仍有效的预演复核。 |

新增模拟机、任务分配和预演回放使用前述独立工具。复核结果仅作用于隔离预演。

### 态势上下文告警日志与视频

Command 展示设备、告警和业务日志，已处理告警标记属于界面会话。Map 与 Video 同步选机和视频目标，播放状态单独确认。Video 的多卡片布局共用一个主播放器。 [界面和数据审计](Stage1/Stage1-Feature-Audit.md) · [选机视频联动](EditReturnVideo.md) · [视频验收](TASK-V2-VideoMultiFeedUI.md)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `get_mock_workspace_health` | `mock:read` | 待补 | 查看隔离工作区就绪、协作客户端连接及数据新鲜度摘要。 |
| `get_mock_workspace_context` | `mock:read` | 待补 | 读当前授权会话的选中方案、任务、设备及视频目标。 |
| `set_mock_workspace_selection` | `mock:read` + `mock:context:select` | 待补 | 在授权的演示会话选择或清空当前方案、任务及设备集合。 |
| `get_mock_workspace_preferences` | `mock:read` | 待补 | 读取演示会话可同步的语言偏好。 |
| `set_mock_workspace_language` | `mock:read` + `mock:context:preferences` | 待补 | 切换授权演示会话的共享语言。 |
| `list_mock_session_alerts` | `mock:read` | 待补 | 查询运行中的 Mock 工作区告警。 |
| `acknowledge_mock_session_alert` | `mock:read` + `mock:alerts:ack` | 待补 | 为当前授权会话标记一条模拟告警已处理。 |
| `list_mock_operation_events` | `mock:read` | 待补 | 按分类、对象或时间查询脱敏业务日志与事件。 |
| `list_mock_video_sources` | `mock:read` | 待补 | 查询允许访问的样例视频源和可用情况。 |
| `get_mock_video_view` | `mock:read` | 待补 | 读共享视频目标与样例源状态，分别报告已选目标和已确认播放状态。 |
| `set_mock_video_target` | `mock:read` + `mock:video:select` | 待补 | 显式切换至受控的 Mock 视频目标。 |

告警确认仍需约定会话持久化边界。选机与视频切换的契约应保留 Map 显式选机的现有联动语义。

### 驾驶员交接与测试工作区

Mock Pilot 存在显式交接申请、Command 人工授权及收回、Pilot 启用和退出。历史 DEMO 启动归位属于测试环境操作。以下工具覆盖交接流程的观察及申请，操作者的授权和 Pilot 操作保持各自的权限。 [驾驶员 Mock](../Backend/README.md#104-驾驶员控制权-mock) · [Mock 启动归位](UIInteractionFollowup.md#启动归位与地图可见性2026-10-20)

| 工具 | 所需权限 | 契约 | 覆盖能力 |
| --- | --- | --- | --- |
| `list_mock_pilot_handoff_requests` | `mock:read` | 待补 | 列出隔离工作区内的 Mock 交接申请和确认状态。 |
| `get_mock_pilot_handoff_status` | `mock:read` | 待补 | 查模拟设备的交接、授权和会话是否已结束，仅返回授权范围内的状态。 |
| `request_mock_pilot_handoff` | `mock:read` + `mock:pilot:request` | 待补 | 把指定 Mock 设备的交接申请送到人工处理入口。 |
| `request_mock_pilot_handoff_release` | `mock:read` + `mock:pilot:request` | 待补 | 申请由操作员处理 Mock 控制权收回，返回申请及确认状态。 |
| `request_mock_workspace_reset` | `mock:read` + `mock:sandbox:reset` | 待补 | 仅测试管理员可向自己的隔离工作区发起人工确认的重置。 |

### 客户端与环境管理边界

| 当前系统能力 | MCP 接入归属 |
| --- | --- |
| Map 2D/3D、平移缩放与绕转、定位、框选手势、航线渲染 | 用选择、方案和航线工具提供业务结果，视角与交互由 Map 维护。 |
| Video 布局、主副卡交换、播放器重试和当前帧渲染 | 用样例源、视频目标和状态工具提供业务数据，播放器交互由 Video 维护。 |
| Pilot 启用、退出、连续操作与人工授权 | 用交接申请和状态工具协助人工处理，操作与授权由原有客户端及身份管理。 |
| Launcher 进程启停、依赖检查、地图定位与常用地点、日志文件及配置文件 | 工作区健康和业务日志对模型提供脱敏摘要，进程、文件和地图环境由运行人员维护。 |
| 真实设备、真实图传、真机分配、飞控链路、历史目标与攻击预演 | 属于本目录之外的功能面。Mock 服务使用合成数据和独立工作区。 |

### 待补契约的交付与验收

1. 先补方案及任务详情、设备状态和事件等读取契约，再补草稿编辑、航线会话与复核。执行、预演、视频和交接工具按各自的确认、状态转换与回放规则完成验收。
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

从一致的 Mock 快照开始。只读身份发现 3 个查询工具，另备有回放权限和有草稿权限的独立测试身份，分别发现对应的写工具。先用 SDK 直调，再用 LLM 端到端验证。

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
