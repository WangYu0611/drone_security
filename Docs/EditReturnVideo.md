# 重新编辑方案与 Map 视频目标同步

重新编辑已有方案后，可以结束配置并返回方案列表；保存或放弃未保存修改的处理仍由界面明确确认。轨迹校验失败时，方案及发生冲突的任务显示为草稿，保留冲突无人机和最小间距信息。复核与部署继续受到 Backend 校验约束，退出配置不会删除方案。修正路线后可再次部署同一方案，原部署快照保留；部署本身不代表开始执行。

按照本次用户明确要求，在 **Map 中显式点击无人机** 后，Video 主目标同步为实际点击的无人机。此行为覆盖此前 Map 选机不自动切视频的约定。Command 的普通选择、任务或方案选择、通用 context 更新仍保持独立。Shift / Ctrl 点击按实际命中的无人机切视频，即使它不是最终主选，或刚从多选集合移除。

## 有序确认协议

Map 先向 `/api/context` 发出选择请求，在正常 `patch` 外携带该客户端递增的正整数序号和实际点击对象：

```json
{
  "instance_id": "<online Map instance>",
  "map_selection_serial": 12,
  "map_video_target_uav_id": "UAV-02",
  "patch": {
    "active_uav_id": "UAV-01",
    "selected_uav_ids": ["UAV-01", "UAV-02"]
  }
}
```

收到选择确认后，再向 `/api/video-view` 发送：

```json
{
  "instance_id": "<same Map instance>",
  "video_target_uav_id": "UAV-02",
  "expected_context_version": 23,
  "map_selection_serial": 12
}
```

`expected_context_version` 使用选择响应的版本。Backend 在同一请求锁内验证当前 context 版本，以及该 Map 客户端最近一次选择的序号、确认版本和实际点击对象。旧选择请求、迟到的视频请求或目标不匹配返回 HTTP 409 / `VIDEO_SELECTION_STALE`，保留当前选择及视频目标。选择结果未变化时 context 版本可以不增加，选择序号仍严格递增，因此同版本的过期点击也不能覆盖新目标。成功的点击意图只消费一次；重复同一 guard 返回过期错误，避免覆盖其后用户显式设置的视频目标。再次点击同一无人机使用新序号，可正常重新同步。

视频更新通过 `VideoViewChanged` 广播。Video 重连后先订阅，再读取当前 context 与 video-view 快照恢复目标。无 guard 的普通 context 和已有显式 OpenVideo 请求维持各自语义。

## 部署期间的心跳与并发保护

首次原生重部署检查发现：配置持有者的心跳可在复核与部署之间推进全局版本，导致原有流程返回 `VERSION_CONFLICT`。该次失败证据保留，不能把此前协议通过记录解释为这次原生重部署通过。

部署确认流程冻结 `expected_content_revision` 和 `configuration_acquired_version`，并让每一步的 `expected_plan_version` 来自上一步确认。仅 `validate` / `review` / `deploy` 支持这组三字段：同一客户端仍持有同一方案的有效租约，内容版本、租约取得版本和方案版本全部匹配时，允许心跳引起的较旧全局 `expected_version`。未来版本、早于本租约的版本、缺失或不匹配的 guard 继续拒绝；全局版本恰好匹配时也检查全部 guard。无 guard 的请求与其他动作仍使用原有严格版本规则，不自动重试确认。

## 独立协议回归

正常构建 Backend 后，在仓库根目录运行。脚本需要 Python 3.10+，以及提供全局 `fetch`、`crypto.randomUUID` 和 `WebSocket` 的 Node.js；本次使用 Node.js `v24.13.0`：

```powershell
python Scripts/EditReturnVideo/verify.py
```

脚本默认独占 `22994/22995`，拒绝生产与原生验证端口 `19880/19881/22992/22993`，使用新的 Mock 数据目录和它自己启动的 Backend。每次在 `Evidence/EditReturnVideo/protocol-<run>/` 保存全部请求响应、WS 消息、关键快照、结果、二进制 SHA-256 和自有进程清理记录；失败目录不会覆盖。测试结束只清理本次持有进程句柄的 Backend。

13 项组合检查覆盖（原 11 项保留，新增 2 项心跳组合检查）：

- 双任务方案首次部署、重新编辑后碰撞任务显示草稿，以及无碰撞任务保留就绪状态。
- 无法复核或部署的草稿可结束配置、清除方案选择；修正后用同一方案、任务和路线 ID 再部署，保留原部署快照。
- 普通 Command / Map context 选择不切视频；带确认 guard 的 Map 显式点击切换视频并广播。
- 旧 context 版本、同版本的旧序号，以及迟到的 context 请求均不能覆盖较新目标。
- 多选主选不变但实际点击对象变化、Ctrl 取消选中、目标篡改、已消费请求重试被拒与同机重新点击。
- 同一 Video 身份重连水合并继续接收后续切换；选择和视频同步不修改部署或创建执行。
- 在校验、复核和部署每一步前主动发送真实 WS 配置持有者心跳，确认它只推进全局版本，三项 guard 仍可完成同方案部署。
- 每一步分别验证错误内容、租约和方案版本在全局过期或准确时都被拒绝，并检查无 guard、缺项、未来全局版本与早于租约的版本继续失败。

前一轮 HTTP / WS + 隔离 Mock 回归：**11/11 通过**，退出码 0，自有 Backend 已停止且无遗留句柄，清理错误为空，运行前后二进制 SHA-256 一致。见 [运行记录](../Evidence/EditReturnVideo/protocol-1792725185560741000/run.json)、[检查结果](../Evidence/EditReturnVideo/protocol-1792725185560741000/result.json)、[请求响应](../Evidence/EditReturnVideo/protocol-1792725185560741000/traffic.json) 和 [WS 消息](../Evidence/EditReturnVideo/protocol-1792725185560741000/events.json)。

最终 Backend 构建后，含真实 WS 心跳插入的完整协议回归 **13/13 通过**，退出码 0；自有 Backend 已清理，`ownership_after_stop` 和 `cleanup_errors` 为空，运行前后 SHA-256 均为 `51158bffa028adc9004b2f6846ca325f5a10ff66784bde2bfd9bf51645e51e5b`。见 [本轮运行记录](../Evidence/EditReturnVideo/protocol-1792726708818955700/run.json)、[13 项结果](../Evidence/EditReturnVideo/protocol-1792726708818955700/result.json)、[三阶段心跳快照](../Evidence/EditReturnVideo/protocol-1792726708818955700/snapshots.json) 和 [完整请求响应](../Evidence/EditReturnVideo/protocol-1792726708818955700/traffic.json)。此前 11 项证据完整保留。

此结果验证 HTTP / WS 与隔离 Mock 状态，不证明原生按钮可达、鼠标命中、摄像头播放或真实硬件行为。原生界面与 UE 组件结果应单独记录。

## 该功能阶段的构建与原生验证（2026-10-23）

本节“最终”指编辑返回与视频联动阶段完成时的构建。之后按钮框选继续更新 UE 模块；当前发布构建身份及其单独复核见 [发布说明](Delivery-20261023.md)，不将本节 10 项 RHI 结果归到后续构建。

规范 Backend 和 UE Editor 均构建成功，最终二进制身份、各层结果和进程清理记录见 [验收汇总](../Evidence/EditReturnVideo/20261023-105421/acceptance.json)。

- 新增 Backend 组合及心跳边界回归：9 发现 / 9 完成 / 9 通过。
- Backend 全套：199 发现 / 199 完成 / 197 通过，仍保留 `SegmentDistanceTest.Crossing` 与 `AssemblyPlannerTest.ConflictHeightSeparation` 两项历史失败，无新增失败。
- HTTP / WS 隔离 Mock：13 / 13 通过，包含三个部署阶段的真实心跳穿插。
- 最终 UE RHI 自动化：10 发现 / 10 完成 / 10 通过，含 Workspace、MissionActions、DeploymentIntent、MapPresentation、SelectionVideoAck、原手动接管 4 项和 Fleet.Workflow；保留各运行的环境 warning 数量与原始日志。
- MissionActions 通过隔离 HTTP 代理刻意等待真实 owner 心跳后才转发部署：捕获的全局版本已过期，计划及内容版本、租约身份未变，仍得到 HTTP 200 并完成同方案部署。见 [注入时序](../Evidence/EditReturnVideo/20261023-105421/deploy-heartbeat-trace-1792726722612866300.jsonl)。
- 原生鼠标：用用户已保存方案的隔离副本复现并复验返回列表、实际部署阻塞提示、修正后同方案再次部署，以及 Map 设备菜单和地图标记选机后 Video 主目标切换。业务修改只作用于隔离副本，未为通过而放松安全检查。

原生环境没有配置摄像头源，验证的是主画面对应的无人机身份及该机无视频源状态，没有声称摄像头播放或真实无人机行为。本次自有 QA 进程已清理。正常用户客户端的后续启动与 QA 清理分别记录。

原生保存后退出、阻塞提示及地图标记切换是在中间构建 `A7CA3BAC` 上完成；这一轮实际重部署抓到心跳导致的 `VERSION_CONFLICT`，保留 [失败截图](../Evidence/EditReturnVideo/20261023-105421/08-native-redeploy-version-conflict.png) 与原始快照。最终构建 `3252B501` 另行完成同一 `plan-39 / mission-40` 的再次部署（新 `deployment-48`）、列表返回与设备菜单到 Video 主目标的复验；完整阶段边界见 [原生记录](../Evidence/EditReturnVideo/20261023-105421/native-acceptance.json)。最终构建的脏内容保存退出另有上述 Workspace RHI 回归，不把中间构建的整段原生流程冒充最终构建全量回放。

本地化 GatherText 各步骤及 GatherText 本身退出 0；Editor 进程因已有 GameFeatureData 启动错误退出 1，原始日志保留，未将该命令整体记为无错误通过。首次 Backend 测试构建中的 JSON/string 比较编译错误已修正，首次日志也保留，测试断言未削弱。
