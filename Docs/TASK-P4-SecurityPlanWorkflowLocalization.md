# TASK-P4 — Security Plan Workflow / Localization / Explicit Video Target

## Final Result

**CONDITIONAL PASS**，收口日期 2026-09-19。

P4 业务工作流、Route Draft 保护、独立 Video Target、最终两项 UI 显示修复、三端语言切换及原生 A–N 均有实际运行证据。最终 UE 定向测试 5/5，P4 HTTP/WS 21/21；Backend 58/60，失败仍仅为两个历史 planner 测试。

不声明完全无条件 PASS：保留历史引擎/地图问题、少量 legacy 缺省遥测文案/日期区域格式，以及“任一网络接收者广播失败即全体回滚”的不可保证边界。同步 publisher 异常的服务端回滚已实现和测试，不能据此声称分布式原子广播。真实飞行和真实相机均未验收。

## Git / Base / Final HEAD

- Branch: `feat/triple-screen-p4-workflow-localization`
- Worktree: `C:/Users/wy331/Documents/UE5DroneControl-p4`
- Base HEAD: `f3c56eebffb690c70f7a7699ee8ed78cda8df931`
- 最终代码与测试提交: `c2a5ae1a764ba0f811acfb992c39904df29238db`
- 后续提交仅归档 Evidence 和本报告。包含报告自身的提交不能在其文件内嵌入自己的哈希；最终交付 HEAD 由本分支 `git rev-parse HEAD` 解析，并在最终交付回复给出。
- 未创建新分支、未 reset/stash/drop、未 merge、未 push；此前修改和失败证据保留。

## Architecture

Backend 保持权威状态、权限、版本、验证和持久化。三客户端继续使用原 HTTP/WS 连接及 context heartbeat，没有添加并行同步通道。

| Domain | 权威状态 | 客户端副本 / 行为 |
|---|---|---|
| Plan / Mission / Path / Deployment | SecurityPlanStore | Command 业务工作流，Map 空间编辑，Video 无写权限 |
| OperationalContext | active UAV/Plan/Mission 等 | 共享选择；与视频目标分离 |
| UIPreferences | language + ui_preferences_version | UILanguageSubsystem 应用 Backend 确认的 en/zh-Hans |
| VideoView | video_target_uav_id + video_view_version | Video 明确切源；Global Active UAV 不隐式切源 |

`USecurityPlanWorkspaceWidget` 承载 Command 管理和审阅，`UMapMissionRouteWidget` 承载 Map 编辑会话。已有 PlayerController/PathActor 实际几何编辑复用；新增测试访问声明只用于检查真实 actor/undo 状态。

## Security Plan Workflow / Command UX

创建方案 → 添加任务 → 分配 UAV → 明确请求 Map 编辑 → 配置检查 → 人工审阅 → 二次确认部署 → 已部署只读 → 复制草稿。任务列表显示 UAV、航点数与下一步入口。审阅显示每个 Mission 的配置状态、航点数、循环/速度/等待摘要、问题与内容版本，始终保留“只检查业务配置、不启动执行”的说明。

Command 原生创建三个 Mission，并实际遇到重复 UAV 分配拒绝。原生部署 plan-1 revision 12 生成 deployment-12。重启后原生 Copy as Draft 生成 plan-13，修改名称为 `P4 Native Copy Revised`。

## Role Matrix

| 操作 | Command | Map | Video |
|---|---|---|---|
| Plan/Mission 管理、UAV assignment | 允许 | 拒绝 | 拒绝 |
| 配置检查 / 选择 | 允许 | 允许 | Plan mutation 拒绝 |
| Review / Deploy / Copy | 允许 | 拒绝 | 拒绝 |
| Begin/dirty/save/discard Route | 拒绝 | 允许且绑定 owner/session | 拒绝 |
| 请求 Map 编辑 | 允许，Map 必须在线 | 不承担业务请求 | 拒绝 |
| 明确设置视频目标、语言偏好 | 允许 | 允许 | 允许 |

真实 HTTP/WS 21/21 覆盖角色拒绝、重复 assignment、离线 Map、session/lease、review/deploy 和独立视图。错误使用 code/params，客户端不解析英文 message 翻译。

## Content Revision

`content_revision` 只随业务内容变化递增。Plan/Mission 内容、assignment、route 保存会使旧 validation/review 失效。Selection、语言、Video Target、会话 heartbeat 不增加内容版本。Store transaction version 用于并发控制，不等于内容 revision。

Legacy 数据迁移到 content revision 1，旧 READY 不作为已审阅保证。复制生成新 Plan/Mission/Route ID，DRAFT revision 1、review/deployment 清空，保留 source_plan_id/source_deployment_id。

## Validation

配置检查要求 Mission、有效且不重复的 UAV assignment、有效航线/航点及参数。输出结构化 issues、各项计数和 validated_content_revision。READY 表示业务配置通过；不代表地形、空域、续航或飞行安全已评估。

原生 H 先检查缺失路线的失败场景，再补齐到 3 missions / 3 assigned / 3 routes。Deploy 再检查当前配置及版本，不接受陈旧结果。

## Review

Review 记录 content_revision、reviewed_at、reviewed_by。内容修改清除旧 Review；现有 dirty/edit session 阻止 Review/Deploy。原生 revision 10→11 验证了失效，再次 Validation/Review 后恢复部署入口。之后第三个 North 航点保存为 revision 12，再次检查/审阅后才部署。

UE `CommandReviewReadonlyCopyAndCombo` 也通过真实 HTTP 与真实 Command widget 验证 review invalidation 和 Deploy 隐藏。

## Deployment Record / Deployment Snapshot

Deployment 仅冻结业务配置，包含原方案、所有 Mission、所有 Path、content revision、review/deploy 操作者与时间。Plan 与三个 Mission 均转为 DEPLOYED；Backend 拒绝原地修改，Command 字段只读。

最终原生记录 `deployment-12`。复制为 plan-13，Mission IDs 为 mission-14/16/18，Route IDs 为 route-15/17/19。复制时 revision 1，改名后 revision 2；review/deployment 不继承，source 指向 plan-1/deployment-12。

原 snapshot 前后 canonical JSON SHA-256 均为：

`b6620cb7190075bc0a9bf1c0e0b0338ac73ab6dab69ca33ca43d5bbe1836397f`

### Atomicity boundary

所有 business change 在临时 next document 中验证；写文件成功、同步 publisher 接受后才交换内存状态。持久化失败不发布；同步 publisher 抛异常时恢复原文件并保留原内存。对应故障注入测试通过。

已有 WS 层逐接收者发送失败会将连接置离线；已送达其他客户端的事件无法撤回。不能承诺任何网络部分投递失败都回滚所有客户端。文件系统同时导致 rollback persist 失败也不属于已验证成功路径。以 Backend 权威状态和重连 hydration 恢复一致性；此边界明确保留。

### No execution

P4 PlanRequest → SecurityPlanStore 路径没有 ApiCreateArray、AssemblyController、ExecutionEngine、PX4/MAVLink 调用依赖。最终 HTTP method/path audit：`POST /api/arrays = 0`；Assembly/Exec start 日志计数均 0。

历史原生 M 在新增 HTTP 请求审计前完成，因此它的 arrays=0 依据隔离代码可达性及无执行启动日志，不冒充当时已有每请求计数。最终协议 Deploy 则有直接 HTTP 审计。没有声称抓取真实 PX4/MAVLink 网络包。参见 [执行边界](../Evidence/TASK-P4/closure/deploy-execution-boundary.json)。

## Route Edit Sessions / Map UX

20 秒租约使用原 context heartbeat 续租。Begin ACK 后开启真实路径编辑；首次变化标 dirty。Save 使用 owner/session/version，成功 ACK 后退出。HTTP 冲突、持久化失败、断线/无确认保留 editor、waypoints、actor 和 undo。待切换 Mission 通过 Save/Discard/Cancel 处理，不静默丢弃；空 Mission selection 也有测试。

原生 F 用隔离 fixture 的 `.tmp` 路径制造真实写入失败，截图显示草稿/编辑器保留并继续加点，重试 ACK 后退出。UE MapLiveDraftGuard 进一步核对 actor 指针、undo stack、真实 HTTP 409 与重试保存。

K 语言验收后的离开操作曾因窗口坐标变化实际选择 Save，第三个 North 点因此保存；不将它标成原生 Discard 证据。真实 Discard ACK 行为由 UE 测试覆盖。

## Video Target

视频只跟随独立 `video_target_uav_id`。明确 feed / View Video 操作更新目标；无目标时无默认首机回退。Set as Active UAV 是单独明确动作。旧 Follow Target 不再作为控制入口。

原生 L：明确 UAV-01 → Command 选择 UAV-02 → Video 仍 UAV-01 → 明确打开 UAV-02 后才切换。最终编号各处使用 `UAV-02` 格式。只有一个实际 browser/player；4/6 卡位是占位，不声称多路播放。无源时显示 NO SOURCE，没有伪造 PLAYING。

语言切换不重建 browser、不增加 generation；原生旧日志仅包含两次明确切源，重启最终日志仅包含一次 UAV-02 恢复。自动化还比较 browser 指针和 generation。

## Localization Architecture

正式 `DroneOps` Localization Target：NSLOCTEXT/FText 稳定 key，en 与 zh-Hans PO/archive/locres；`UILanguageSubsystem` 只应用已确认语言，失败/离线请求不本地伪切换、不排队回放。PIE 使用 game localization preview，并等待异步资源刷新。

最终修复 selected 按钮双语混排及 Video UAV 补零不一致；无目标提示也来自正式 catalog。六张截图见 [Evidence README](../Evidence/TASK-P4/README.md)。Backend language、Plan revision 和 video version 分离，已有真实未保存 3-waypoint Draft 的语言不变性证据。

2026-09-19 GatherTextStep4 明确生成两个 locres，GatherText commandlet exit 0；整个引擎进程 exit 1 仍因已有 GameFeatureData/AssetManager 启动错误。UE 使用 `WriteFileIfModified`+BinaryHash，相同资源保留原 mtime；没有伪造最新时间戳。哈希与完整日志在 [本地化结果](../Evidence/TASK-P4/closure/localization-results.json)。

**局部保留显示边界**：Command 缺少 TaskState 时，旧 fleet telemetry fallback 仍可能显示 `MISSION 任务 N/A`；系统日期区域格式仍可为中文。它们不属于本次续作指定的 selected/Video 修复，未扩展重构。故不声称每一个 legacy 控件已完全英文化。

## Event Localization

Plan 事件保存 event_type/code + params，UI 用稳定模板与用户原名重新格式化，不解析 Backend 英文 message。连接、选择等已改为结构化本地事件。外部告警正文和用户输入保持原文。中英文截图与自动化均验证 Plan 事件重渲染。

## Persistence / Restart

Plan/Deployment、OperationalContext、UIPreferences、VideoView 分开持久化。无效/旧版本 domain snapshot 被拒绝，重连通过原 WS+HTTP hydrate 最新状态；下拉框刷新使用 guard，不回写 selection。

9 月 15 日和最终 9 月 19 日 Backend JSON 与 M 后状态逐字段一致；N 另启动三个独立真实客户端，确认 Command DEPLOYED/三个 Mission/deployment-12、Map 原任务只读航线、Video UAV-02、中文、active UAV/Plan/Mission，并保存 presence/process/screenshot。然后才复制并改名新 Draft，不以 JSON 检查替代原生恢复。

## HTTP/WS

P3 baseline 13/13（复用）。最终 P4 21/21，包括角色矩阵、CRUD/assignment、Map request、dirty、409 保留、lease expiry、review/deploy、copy、新连接 hydration、language/video 独立性以及数组请求为零。

第一次 Copy selection 失败保留在 protocol/trial1；最终使用 closure/protocol 的完整请求/响应与 WS transcript。

## Automation

| 最终定向 UE 测试 | Result |
|---|---|
| Workflow.CommandLiveCRUD | Success |
| Workflow.CommandReviewReadonlyCopyAndCombo | Success |
| Workflow.MapLiveDraftGuard | Success |
| Localization.FTextAndDraftIsolation | Success |
| VideoTarget.ExplicitSelectionAndBrowserIsolation | Success |

5 Test Started / 5 Test Completed Success。P3 baseline 3/3 复用，不无理由重跑。最终 UE5.8 Development Editor 构建 exit 0。

并发 Video trial 出现 Cesium SQLite `database is locked`，Result=Fail；没有产品 assertion failure。保留该 trial，结束本次三个原生实例后用相同测试/断言隔离重跑 Success。首轮网络错误、早期 C++ 编译和 Localization 失败也保留，不以进程 exit 0 替代测试结果。

## Backend Tests

Release 构建 PASS。最终 suite 60 executed，58 PASS，2 FAIL：

- `SegmentDistanceTest.Crossing` — historical retained。
- `AssemblyPlannerTest.ConflictHeightSeparation` — historical retained。

P4 revision/review/deploy/copy/session/independent domains，以及 persistence/publisher 故障测试全部通过。未修改历史测试期望或删除失败。

## Native Acceptance

A–N **14/14 PASS，限配置工作流和 synthetic fixture**。每项独立结果、步骤说明与真实截图链接见 [正式 A–N 表](../Evidence/TASK-P4/native-acceptance.md)。协议和自动化只作为补充，不替代原生输入。原始 M、N、K 的边界均在表内明确说明。

## Known Issues

- 无分布式全接收者 WS 广播回滚保证；不夸大同步 callback rollback。
- Legacy 缺省 task telemetry 文案和 OS date locale 尚非完全统一，见 Localization。
- GatherText commandlet 完成但引擎进程因已有资产管理配置诊断 exit 1。
- 并发 UE 可触发 Cesium SQLite cache lock；隔离测试通过，失败证据保留。
- 原生重启 Video 曾短暂显示 VSM/Nanite 队列警告，最终稳定截图恢复正常。
- Windows 系统菜单阻塞游戏 tick 曾使 Command 心跳超时；自动重连，离线语言操作被明确拒绝，恢复后显式重试成功。

## Retained Issues

未在 P4 专项修复/重验，继续保留：Standalone 1082 vs 1080；两项 Backend legacy planner failure；low-altitude 3D route occlusion；XYZ/map 网络依赖；6GB VRAM capacity 风险；P3 孤立 startup assertion；原生工具轴拖动限制。已有 Cesium enum 初始化和 GameFeatureData 诊断同样保留。不调整业务坐标掩盖遮挡。

## Deferred

真实相机/WebRTC 播放、真实 Jetson/PX4、飞行安全、地形/空域/续航验证、真正多路播放器、部署触发执行均未纳入本次业务 PASS。P4 Deploy 不代表飞机可执行。

## Evidence Index

根目录：`C:/Users/wy331/Documents/UE5DroneControl-p4/Evidence/TASK-P4`

- [分类索引](../Evidence/TASK-P4/README.md)：selected final result / diagnostic result / historical failing trial。
- [原生表](../Evidence/TASK-P4/native-acceptance.md) / [最终 UE 汇总](../Evidence/TASK-P4/closure/automation-results.json)。
- [原生 Copy IDs 与哈希](../Evidence/TASK-P4/closure/N-copy-results.json)。
- [二进制与资源哈希](../Evidence/TASK-P4/closure/final-binary-hashes.json)。
- [受保护 refs](../Evidence/TASK-P4/closure/protected-refs.json)。
- [日志脱敏备份记录](../Evidence/TASK-P4/closure/log-redaction-manifest.json)：只遮蔽 token；所有失败原文保留，本机原始日志未删除，历史 commit 未改写。

## Git checks

分阶段提交可追溯：baseline、Backend/domain、localization/共享副本、Command workflow、Map draft、Video target、tests、Evidence/docs。最后检查 git status clean、protected refs 一致、base..HEAD 无 merge commit。原 P1 工作区已有日志修改保留，P2/P3 工作区未改动。所有测试实例已停止后归档证据；没有重启或停止非本任务的进程。

## Final HEAD

代码/测试交付锚点：`c2a5ae1a764ba0f811acfb992c39904df29238db`。Evidence/docs 随后归档；完整最终提交使用：

```powershell
git -C C:/Users/wy331/Documents/UE5DroneControl-p4 rev-parse HEAD
git -C C:/Users/wy331/Documents/UE5DroneControl-p4 status --short
git -C C:/Users/wy331/Documents/UE5DroneControl-p4 log --oneline --decorate
```

最终回复给出该完整 HEAD 和实际 Git Gate 结果，不以代码锚点冒充文档最终提交。
