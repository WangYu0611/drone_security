# Drone Security Mock MCP 接入指南

接口契约 v0.2 · 2026-11-23

目标是让 LLM 通过 MCP 逐项使用指挥系统的业务功能。每个工具独立约定参数、权限和验收。v0.2 覆盖隔离 Mock 环境的场景与状态查询、告警查询、演示回放和安保方案草稿创建。

**当前进度**　项目已有 Stage 1 Mock 业务流程。本文定义待实现的业务 MCP 契约，联调地址和账号待服务部署后提供。服务交付前，模型侧可以用本文的固定样例测试工具选择、参数生成和结果解析。仓库中的 Unreal Editor MCP 属于编辑器工具。项目现状见 [演示手册](Stage1/Stage1-Demo-Runbook.md) 和 [编辑器 MCP 记录](TASK-A3-Startup-Visual.md#unreal-mcp-evidence-and-current-boundary)。

## 工具与权限

| 工具 | 用途 | 所需权限 |
| --- | --- | --- |
| `list_demo_scenarios` | 查询可用的录制演示场景 | `mock:read` |
| `get_sim_status` | 查询模拟方案、任务、设备和当前身份的回放状态 | `mock:read` |
| `list_sim_alerts` | 查询演示场景的录制告警 | `mock:read` |
| `set_replay_state` | 播放或暂停当前身份有权访问的回放 | `mock:read` + `mock:replay` |
| `create_security_plan_draft` | 在当前身份对应的 Mock 工作区新建安保方案草稿 | `mock:read` + `mock:plan:draft` |

服务按身份和权限返回可见工具，并在每次调用时重新校验。Mock Backend 是模拟业务状态的权威来源。回放写入限于授权回放会话，草稿写入限于隔离 Mock 工作区。部署与开始任务由操作员在指挥端分别确认。

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
