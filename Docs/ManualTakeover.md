# 执行中无人机的手动接管移动

已部署任务的无人机进入执行、返航或暂停状态后，可以在 Command 战术地图或 Map 地图中单独选中该无人机，选择移动目标并确认接管。接管会结束该机当前的任务执行，从 Backend 当前确认的位置创建一条独立移动执行；到达目标后不会自动恢复原任务。接管成功时，同一任务中的其他无人机保留原执行。按 2026-10-23 更新的交互要求，Map 主动点击无人机或通过设备列表选机时，会在选择确认后同步切换 Video 主目标；Command 选机、任务分配和接管命令本身不额外切换视频。Map 纯框选和点击空地也不切换视频。

部署本身只激活配置。尚未开始、预约中、已结束的执行，以及普通手动移动执行，不属于此功能的接管对象。接管仅允许 Command 和 Map 客户端操作；Video 只接收状态。配置模式期间不能开始接管移动。

## Backend 协议

向 `/api/security-plans` 发送 `execution_takeover_move`，身份须为已在线订阅的客户端：

```json
{
  "instance_id": "<online Command or Map instance>",
  "action": "execution_takeover_move",
  "request_id": "<unique request id>",
  "execution_id": "<selected UAV's deployed mission execution>",
  "uav_id": "UAV-01",
  "control_version": 4,
  "target": {"latitude": 39.981, "longitude": 116.3465, "altitude": 40},
  "target_area_radius_m": 10000
}
```

`target_area_radius_m` 可省略。目标使用现有 WGS84 / 椭球高约定，移动范围沿用当前 10 km 本地限制。一次只接管一架无人机，不接受 `assigned_uav_id` 或 `assigned_uav_ids` 替代单机身份。

Backend 在同一事务内核验角色、配置锁、执行身份和控制版本、位置、目标范围与轨迹安全，再提交原执行终止与新移动。原执行成为 `ABORTED`，`completion_reason` 为 `MANUAL_TAKEOVER`；响应 `execution_id` 指向新 `GROUP_MOVE` 执行，新执行记录 `source_execution_id` 并具有独立 `group_id`。部署快照和原任务路线保持不变。旧任务组的暂停、恢复和终止不控制新移动。

重复发送完全相同的 `request_id` 和请求内容返回同一移动；改变内容复用该 ID 会得到 `REQUEST_ID_REUSED`。过期控制版本返回 `EXECUTION_STALE`，轨迹碰撞返回 `TRAJECTORY_CONFLICT`，这些失败都不会先终止原任务。收到失败后应按 Backend 最新状态重新检查并让用户明确提交，不能自动改目标或强制解除占用。

## Real 适配器的失败边界

Real 适配器先对被接管无人机发送 hold，再允许新移动执行进入运行。hold 被拒绝或抛出异常时，原执行和新执行都标记为 `FAILED`，返回 `REAL_HOLD_REJECTED`，不发送新移动命令；重复同一请求仍返回失败。现有 Real 整组失败保护可能随后停止该原任务组的其他成员，因此“其他成员继续”适用于接管正常成功的情况。

若 hold 已生效但新状态持久化失败，Backend 将内存中的原执行和新执行都置为 `FAILED`，返回 `REAL_TAKEOVER_PERSIST_FAILED`，阻止旧控制器继续派发路线或新移动启动，并要求人工检查。此时不能把内存中的失败保护当成持久化成功。Real 适配器的 Fake / 回调替身回归仅验证控制器行为，不构成真实无人机或物理 hold 生效的证明。

## 隔离验证

构建正常 Backend 后，在仓库根目录运行。脚本需要 Python 3.10+，以及提供全局 `fetch`、`crypto.randomUUID` 和 `WebSocket` 的 Node.js；本次使用 Node.js `v24.13.0`：

```powershell
python Scripts/ManualTakeover/verify.py
```

脚本使用全新 Mock 数据和独占端口（默认 `22782/22783`，可通过 `--http-port` / `--ws-port` 改写），只停止它持有句柄的 Backend。每次输出到独立 `Evidence/ManualTakeover/protocol-<run>/`，保存请求响应、WS 时序、关键快照、16 项检查结果、二进制 SHA-256 和进程清理记录；失败证据保留。HTTP/WS + Mock 验证不等于原生鼠标操作或真实无人机验证。

协议覆盖四机已部署任务中单机接管、其他成员继续、重复请求、错误身份与过期版本、配置锁、目标碰撞、Command/Map/Video 权限、旧任务组控制隔离、目标到达和终止后不恢复。

已执行结果：HTTP/WS + 隔离 Mock **16/16 通过**，进程退出码 0，自有 Backend 已停止且无遗留进程句柄，运行前后二进制 SHA-256 一致。见 [运行记录](../Evidence/ManualTakeover/protocol-1792718590193182400/run.json)、[检查结果](../Evidence/ManualTakeover/protocol-1792718590193182400/result.json) 和 [WS 时序](../Evidence/ManualTakeover/protocol-1792718590193182400/timeline.json)。本条记录不包含 UE 组件、原生鼠标或真实硬件验收。

原生接管验证可在测试人员拥有的全新隔离 Mock Backend 上准备两机长任务：

```powershell
$env:P1_HTTP = 'http://127.0.0.1:<owned-http-port>'
$env:P1_WS = 'ws://127.0.0.1:<owned-ws-port>/ws'
$env:MANUAL_TAKEOVER_EVIDENCE = '<fresh-evidence-directory>'
node Scripts/ManualTakeover/fixture.mjs
```

该准备脚本要求空的计划与执行仓库，使用所选 UAV 的 Mock 起点，创建并部署约 890 m、2 m/s 的两机航线，再开始任务。它输出执行 ID 与快照，关闭自身客户端后退出，Backend 继续运行。此准备过程属于 API 证据；其后的实际选机、选择目标、确认接管和移动显示，应单独记录为原生 UI 证据。协议脚本不提供真实硬件验收结论。

## 本次实现验收（2026-10-23）

最终 UE Editor 构建成功。完整证据索引见 [验收汇总](../Evidence/ManualTakeover/acceptance.json)。

| 验证层 | 结果 | 范围 |
| --- | --- | --- |
| Backend 新增回归 | 13 发现 / 13 完成 / 13 通过 | 原子接管、幂等、版本与权限、安全失败、Fake Real 失败边界 |
| Backend 全套 | 190 发现 / 190 完成 / 188 通过 / 2 失败 | 保留已知 `SegmentDistanceTest.Crossing` 与 `AssemblyPlannerTest.ConflictHeightSeparation` 基线失败 |
| HTTP / WS 隔离 Mock | 16 / 16 通过 | 实际服务接口、广播、成员隔离与目标到达 |
| 新增 UE 自动化 | 4 发现 / 4 完成 / 4 通过 | 可接管条件、预览身份与请求、路线显示归属、中英切换 |
| 既有 UE 回归 | 3 发现 / 3 完成 / 3 通过 | Presentation 使用 NullRHI；Command.SinglePointMove 与 Fleet.Workflow 使用真实 RHI，各保留 1 条已知启动/fixture warning |
| Map 原生鼠标 + Mock | 通过 | 选中执行中的 UAV-01 → 右键目标 → 中文后果提示 → 接管并移动 → 暂停 → 恢复 → 到达 |

原生截图及状态见 [原生验收](../Evidence/ManualTakeover/native-1792718959342115200/ui/acceptance.json)。接管后的立即快照和暂停快照均确认 UAV-02 继续原任务；整理最终证据时 UAV-02 已以 `ROUTE_FINISHED` 正常完成。UAV-01 新移动到达所选目标，旧执行仍为 `ABORTED / MANUAL_TAKEOVER`，没有自动恢复。最初最终状态断言误要求 UAV-02 在证据整理时仍处于执行中；保留该失败说明，最终校验允许原路线自然完成，并继续强制检查接管时和暂停时的成员连续执行证据。

原生任务的配置、部署及启动使用 API fixture；上述接管交互使用实际鼠标操作。Command 接管流程尚未做原生回放；真实无人机 **NOT NATIVE VERIFIED**，Fake Real 回归不代表物理控制验证。所有本次自有测试进程已清理，未关闭其他用户进程。

本地化修正后的 GatherText 各步骤及 GatherText 自身退出成功，生成中英资源；Editor 总退出码仍为 1，日志保留已有 GameFeatureData 启动错误。实际中英切换自动化和原生中文界面已通过，不能将该命令行整体报告为全绿。

## 发布前复核（2026-10-23）

以上 190 项 Backend、原生流程及初次协议记录保留各自构建身份。后续编辑返回、视频联动和按钮框选合入后，使用当前规范 Backend `51158bff` 重跑接管协议，16/16 通过，退出 0，自有进程清理完成且二进制未变。见 [发布前协议运行](../Evidence/ManualTakeover/protocol-1792736990601809200/run.json) 与 [16 项检查](../Evidence/ManualTakeover/protocol-1792736990601809200/result.json)。

当前 UE 模块 `1c342e8c` 另以 NullRHI 重跑接管组件回归，发现/完成/通过 4/4，退出 0、无 warning、进程清理完成；这次复核不包含渲染或原生鼠标回放。见 [运行身份](../Evidence/PlanConfiguration/verification-1792737024785819000/run.json) 与 [组件结果](../Evidence/PlanConfiguration/verification-1792737024785819000/summary.json)。发布范围与仍有失败的检查见 [发布说明](Delivery-20261023.md)。
