# TASK-P2 — Command Center V2

验收日期：2026-09-14。结论：**CONDITIONAL PASS**。P2 六项功能和真实三进程 A–H 场景通过；Git 作者身份阻塞已解除，现有改动已分阶段提交并通过 clean 核验。另有本次新观察到的显存容量风险，不归类为历史 Known Issue。

## Git / Recon

| 项目 | 结果 |
| --- | --- |
| P1 基线分支 | `feat/triple-screen-p1-shared-context` |
| Base HEAD | `23ef4422a6b48802525bda42ed70a24984be43b9` |
| P2 Branch | `feat/triple-screen-p2-command-center-v2` |
| 工作目录 | `C:/Users/wy331/Documents/UE5DroneControl-p2`，独立 worktree |
| Final HEAD | 本报告的收尾提交 `docs(p2): close git delivery and retain acceptance limitations`；精确 hash 用 `git log -1 --format=%H -- Docs/TASK-P2-CommandCenterV2.md` 查询，并在最终交付回复记录（避免提交正文包含自身 hash 的循环依赖） |
| commits | 4 个功能/测试证据提交 + 1 个文档收尾提交，见下表 |
| workspace | 功能/测试证据提交后已验证 `nothing to commit, working tree clean`；本报告收尾提交后再次核验 |
| 合并 | 未 merge，未修改 main/integration/P1 分支代码，未 squash P1 |

原工作区的 `logs/backend.log` 修改和未跟踪 `Backend/data/operational_context.json` 原样保留；未 reset/stash/覆盖。作者已由用户明确指定为 `WangYu0611 <331508364@qq.com>`，仓库 local 配置和 `git var GIT_AUTHOR_IDENT` 均验证正确；未修改全局 Git 配置。仅整理已有改动及文档，没有重新开发 P2、重跑运行时验收或启动 P3。

Recon 确认 P1 的 UOperationalContextSubsystem、三角色、Backend `/api/context`、OperationalContextChanged 均存在。Command 入口为 DroneOpsPlayerController → CommandScreenManager → CommandShellWidget，复用 DroneListWidget、CommandAlarmPanel、CommandAlertStore。Main Map 为 MapShellWidget / CommandMapInteractionService 和 Cesium 2D/3D，含已有离线栅格平面。原 Video source 经 Registry 选择并默认跟随全局；本次将媒体视图的本地 source 与共享选择分开。现有 M1/P1 fixture 包括 MapVisualQA、CommandVisualQA、OperationalContextTest，以及 `Backend/tests/p1_context_integration.mjs`。

## Build

| 构建 | 结果 | 证据 |
| --- | --- | --- |
| UE 5.8 Win64 Development Editor | PASS，最终 `Result: Succeeded` | `ue-final-build.log` |
| Backend Release | PASS，当前 P2 worktree 构建 | `backend-build.log` |

首次 UBA executor 停滞，终止本任务构建进程后，使用 `-NoUBA -MaxParallelActions=2` 正常完成。停滞不算测试通过。A–H/I/J 使用已成功构建的业务版本；随后仅修正筛选文字对比度，以及底图报错时保持 attribution 可见。最终二进制重新构建、CommandLayoutSmoke/EventLogProjection 复测及真实窗口复核。两版 DLL SHA256 分别保存在 `business-acceptance-binary-sha256.json` 和 `binary-sha256.json`，不把早期截图冒充最终二进制截图。

## P2 功能

| 功能 | 状态 | 实现与边界 |
| --- | --- | --- |
| Command V2 | PASS | 统一 Header、左侧 UAV/Alert、中央独立 2D Map、底部 30% Event Log；保留已有详情/动作入口 |
| 2D Tactical Map | PASS | XYZ 底图、Registry GPS 投影、Active/Alert 标记、点击经过 Backend ACK 应用；本地 pan/zoom |
| Event Log | PASS | EventStore、UTC 时间/category/message、自动滚动、分类筛选、Clear Display |
| Video Independent Source | PASS | 默认 Follow OFF；普通 aircraft/slot click 仅切本地 source 并退出 follow；Global Target 单独显示 |
| Follow Target | PASS | ON 立即跟随当前 Active UAV，后续全局变化继续跟随；OFF 保持本地 source |
| Set As Active UAV | PASS | 显式按钮提交 P1 请求，Backend 更新后 Command/Map 联动 |
| Log 折叠 / Search | DEFERRED | 建议的可选项，当前首版未实现 |

### 数据与地图

- CommandCenterPanel 读取现有 Context/Sync/Presence，Client count 按 ONLINE 角色去重，没有第二套 presence 服务。
- CommandTacticalMap 直接读取 Registry descriptor/telemetry，只投影有效 GPS，无 GPS 数量明确显示。不建立第二份 UAV 数据模型，不生成生产用假坐标。
- Command 仍不加载 Cesium actor、3D Tiles 或 Main Map Shell。嵌入完整场景会破坏 Command 的 M1 生命周期；现有在线配置是 Ion asset，并非可直接消费的 XYZ。原生 UMG 栅格视图复用 GPS、Registry 和 P1 选择入口。
- 支持既有 `[CommandMap] BasemapUrl` 合同及 `[CommandTacticalMap]` 覆盖。DefaultGame.ini 提供带引号的 `https://tile.openstreetmap.org/{z}/{x}/{y}.png`，可换为适当的 XYZ 服务。署名持续可见。仅请求当前视口，最多 12 个并发请求、256 张内存缓存、7 天磁盘缓存、失败 30 秒退避；不是离线地图交付。
- 最后实机复核发生部分瓦片请求失败，UI 显示重试，已有底图和 UAV 继续显示。外部底图完整性/网络 SLA 未验收，存在网络依赖。
- Main Map Camera、zoom、bearing 不受 Command 强制同步。未改 Main Map 实现或路线业务坐标。

### Operational Event 入口

UOperationalEventStore 是 GameInstanceSubsystem，接收已接受的 Context 变化、P1 versioned Presence、Backend connectivity、AlertStore 事件。结构包含 timestamp/category/level/source/event_type/target_id/message，以及本地 sequence。支持 SYSTEM / OPERATION / ALERT / MISSION / PLAN 和 INFO / WARNING / CRITICAL；MISSION/PLAN 仅记录已有 context ID 并预留类型，无虚构编辑/执行业务。

日志是最多 500 条的**进程会话投影**，不是 Backend 持久审计数据库。初始历史 OFFLINE 客户端不会误记为刚刚断线。重启 Command 恢复 Backend Context 并投影当前选择，不恢复上一进程完整日志或 AlertStore 历史。Clear Display 只推进 UI 水位，不清 EventStore/Backend。生产 UI 不显示 Unreal Debug Log；截图中的 QA 提示和引擎警告来自真实测试窗口。

### Video 语义

Registry 在所有角色继续保持共享 Active UAV 副本；VideoShellWidget 独立持有 LocalSourceId 和 Follow flag。旧 FollowGlobalSelection 配置不再决定媒体视图默认行为。Follow 默认 OFF；只有明确 SET AS ACTIVE UAV 向 Backend 提交选择。P1DisableSync 的既有本地 Registry 合同保留用于回归。P1 transport、HTTP snapshot、WS subscription、version gate 和 Backend authority 没有重写。

## Regression

选定最终验收结果去重统计，同一测试在不同角色分别计数；见 `automation-summary.json`。21 个 UE 用例均有 Test Completed，20 成功，1 个已知失败。修复前试跑和重跑不重复累加。

| 组别 | 数量 | 成功 / 失败 | 说明 |
| --- | ---: | --- | --- |
| P1 UE | 4 | 4 / 0 | 三角色 VersionAndRoleSync；Map MultiSelectionAcks |
| M1 / Command / Map role | 7 | 7 / 0 | 三角色 StartupRole、AlertStore、CommandRegression、A2MapMode、Map RoleLifecycle |
| Video V2 lifecycle | 2 | 2 / 0 | Command 与 Video 的 RoleLifecycle |
| P2 新增 | 6 | 6 / 0 | Layout、MapSelection、EventLog、Independent、Follow、SetActive |
| Standalone | 2 | 1 / 1 | RoleLifecycle 成功；CesiumWorld 高度断言 1080 vs 1082 |
| P1 HTTP/WS 协议 | 11 | 11 / 0 | 选择、no-op、atomic alert、校验、mode/IDs、late join、presence、reconnect、无事件循环 |
| Backend 单测 | 47 | 45 / 2 | 历史 Crossing、ConflictHeightSeparation 失败 |

CommandMapSelection 和 VideoSetActiveTarget 使用独立真实 Backend；前者断言 ACK 前 Registry 不发生乐观修改。部分隔离用例连接预期不可用端点，报告中 HTTP 连接警告不等于测试失败；没有把 0 discovered 当成 PASS。

开发中 Shell 调整一度丢失 Mission.ArrayId 展示，由 M1.CommandRegression 发现，已恢复详情区 TaskScroll 并重跑成功。另一轮生命周期用例受并行预览引发的 Cesium SQLite lock 干扰；停止预览后顺序重跑 CommandRegression/RoleLifecycle 均成功。这些是本次已处理的问题，未归类为历史 Known Issue。

### 历史问题重新核验

1. **Standalone 1082 vs 1080：仍存在**，本轮失败输出保留于 standalone-reg-events.log / results JSON。
2. **Backend planner legacy：仍存在**，SegmentDistanceTest.Crossing、AssemblyPlannerTest.ConflictHeightSeparation，详见 GTest XML/log。
3. **低高度 3D route occlusion：仍存在 / Deferred**。本轮 A2 截图 regression-map-2d.png 中路线可见，regression-map-3d-occlusion.png 中受遮挡；A2MapMode 几何回归通过。未提高或移动业务坐标掩盖问题。

### 新观察：显存容量风险

A–H 后长时间三进程验收中，Main Map 显示 `Video memory has been exhausted (562.746 MiB over budget)`。硬件 RTX 3060 Laptop 6GB；现场 nvidia-smi 记录 5851 / 6144 MiB，只有本任务三个 UE game 进程，无并行自动化 Editor。没有崩溃，后续选择操作成功，但未做帧率/长稳性能认证。I-map-set-active-result.jpg、gpu-memory.txt、final-live-processes.json 保留证据。

这是**本次新观察到的风险**，不是确认已有历史问题，也没有证据将其归因于 P2 某一改动。功能通过不代表该硬件满足生产长稳容量要求。

## Physical Acceptance

Backend 来自本 worktree Release，独立 HTTP 19080 / WS 19081 和 Saved/P2QA 数据。真实启动三个 `UnrealEditor.exe ... -game -ClientRole=...` 进程，参数保存在 three-processes.json。按 1920×1080 参数启动；主机缩放使原生截图约 1538×878（末次 1538×874），不是 1920×1080 逐像素截图认证。没有生成、拼接或篡改截图。

P2.QA 是显式 opt-in fixture：延时加载 UAV 1/2/3 synthetic GPS/alert，隔离普通无人机发送，保留真实 P1 Context 通道。本地已有 6 个 descriptor，3 个无 GPS，不绘制假位置。无真实无人机、PX4、摄像头、WebRTC 视频或飞行验收。独立原生窗口逐个采集，通过 Backend 版本和进程记录关联，未拼成假三联屏图片。

| 场景 | 结果 | 真实操作 / 观察 | 证据前缀 |
| --- | --- | --- | --- |
| A | PASS | 三端 ONLINE、Clients 3/3，v18，Global 3、Video 1 / OFF | A-* |
| B | PASS | Command UAV-02 marker → v19 / Active 2，Map 高亮 2，Video source 1 保持 | B-* |
| C | PASS | Main Map UAV-03 **实体点击** → v20 / Active 3，Command 标记 3，Video source 1 保持 | C-* |
| D | PASS | Video 普通 source 2→1，Global 3 / v20 不变 | D-* |
| E | PASS | Follow ON 立即显示当前 3；Command marker 2 → v21，Video 跟随到 2 | E-* |
| F | PASS | Follow OFF；Command marker 3 → v22，Video 仍为 2 | F-* |
| G | PASS | Backend debug alert，Log 出现 Alert；Command 选择 Alert，一次 v22→v23 同时更新 UAV-01 / ActiveAlert / ALERT_RESPONSE | G-* |
| H | PASS | 关闭并重启 Command，恢复 v23 / UAV-01 / ActiveAlert / ALERT_RESPONSE；Video 保持 source 2 | H-* |
| I 补充 | PASS | Video SET AS ACTIVE UAV → v24 / UAV-02，Command/Map 同步，Follow OFF | I-* |
| J 补充 | PASS | ALERT 分类仅显示 Alert；Clear 清 UI，Backend 仍 v24 / UAV-02 / 同一 Alert | J-* |

最终样式复核只启动 Command，所以最终截图 Clients 1/3；不替代 A/H 的真实三端在线证据。筛选文字和底图署名已实机确认可读；代码保留单独错误提示行，最后截图未再次出现瓦片错误。验收进程取证后停止，保存 PID 仅代表取证时刻，不代表现在在线。

## Evidence / 复现

完整索引：[Evidence/TASK-P2/README.md](../Evidence/TASK-P2/README.md)。目录保存原生截图、Context JSON、进程参数、UE 自动化结果、关键事件日志、Backend GTest 和构建日志。截图后缀与原生编码一致，JPEG 仅改正确后缀，没有重编码。完整本机试跑日志另保存在 Saved/P2QA。

launch-game.ps1 / run-automation.ps1 已保存到 Evidence；使用本机 UE5.8 / P2 worktree 绝对路径，迁移机器需调整。先启动独立 19080/19081 的 Backend，再顺序启动三角色。普通启动不带 P2.QA；仅验收使用 synthetic fixture。测试时停止物理预览，避免共享 Cesium 缓存冲突。生产服务未被重启或代替。

## Final

**CONDITIONAL PASS**：P2 功能、A–H 及 P1/M1/Video 相关验收通过。保留三个指定历史问题、底图网络依赖及新显存风险。Git 收尾为 **PASS**，作者身份阻塞已移除。整体仍为 **CONDITIONAL PASS**，原因是既有 1082/1080、Backend planner 测试失败、低高度遮挡、底图网络依赖及显存容量风险，不能因完成提交而改写成全部验收无条件通过。

完整 Security Plan/Route Editor、Mission assignment/Deploy/新执行动作、真 UAV/PX4/Jetson/摄像头/多路视频/AI/Recording/Payload、Launcher/自动摆窗、P5 unified header、auth 改造和跨客户端 Camera 同步全部 Deferred。

## Git closure — 2026-09-14

| Commit | Message |
| --- | --- |
| `49e7059` | feat(p2): add registry-backed 2D tactical map |
| `3757c50` | feat(p2): add operational event store and log projection |
| `a2f7aae` | feat(p2): integrate command center and independent video selection |
| `6cf7977` | test(p2): record command and video acceptance evidence |
| 本节所在提交 | docs(p2): close git delivery and retain acceptance limitations |

拆分按依赖调整：先提交 Tactical Map 与 Event Store/Log 组件，再提交 Shell、Context 和 Video 集成。Context 同时承载日志入口与 Video 共享选择语义，保持现有文件整体提交，未为拆分修改业务实现。

功能/测试证据完成 HEAD：`6cf79777d4304c3b74764b68ac4e5e243caf06fc`，此时工作区已 clean。之后只修改本报告及 Evidence README，收尾提交不改变已验收源码。最终 SHA 由上述查询得到，不把前一提交冒充最终 HEAD。

保护分支核验：main 保持 `f6ed7f97fabc21c202333528b0cb030dd4b1b2fe`，P1 保持 `23ef4422a6b48802525bda42ed70a24984be43b9`；P2 Base..HEAD 的 merge commit 数为 0。未 reset、stash、删除用户文件、push 或启动 P3。

本次复核现有证据：31 张清单截图 SHA256 全部匹配；最终 DLL SHA256 与 binary-sha256.json 匹配；所有证据 JSON 可解析；21 个 UE 用例均记录 Test Completed（20 Success / 1 历史 Fail），P1 协议日志 11 PASS，Backend 日志保留 45/47 和两个历史失败。此次是 Git 收尾及证据完整性复核，不是重新执行编译、测试或三进程验收。原有验收结果及其范围限制继续有效。

源码/配置/报告的 staged whitespace 检查通过；原始验收日志的尾随空格警告保留，不改写原始日志。Evidence/git-status.txt 保留为收尾前历史快照，不代表最终状态。
