# 方案配置工作流与验收

本次工作将方案配置作为明确的业务状态：Command 提供方案信息、任务和配置检查；需要路线修改时交接给 Map。Backend 的 `begin_configuration` / `end_configuration` 负责配置占用，客户端只依据已确认快照改变模式。选择方案、查看告警和 Command 选机不应隐式开始飞行或更换 Video Target。按 2026-10-23 的新要求，Map 主动点击无人机或在设备列表选机时，会在选择确认后同步切换 Video 主目标；纯框选保持当前视频。以下历史验收中的视频独立性记录应按此更新范围理解，当前修复与验收见 [编辑返回与 Map 视频联动](EditReturnVideo.md)。

此文档记录操作边界、验收方法和实际验证证据。自动化结果与原生操作分开记录；最终构建重新运行了本次两项配置测试、航线取消竞争回归及本地化测试，原生完整流程和最终界面复核分别标明构建身份。

## 需要保留的交互边界

- 新建方案先填写信息，取消不留下空方案；提交后的标题、状态、任务列表以 Backend 确认为准。
- 配置中，Map 显示配置用途和路线操作，隐藏新建集群移动入口，避免将路线配置误操作为集群飞行。
- 查看告警属于查看流程，不应抢占配置或自动切换视频；退出查看后保留可识别的工作上下文。
- 有未保存内容时离开配置，应分别提供保存后离开、放弃后离开和继续编辑。保存失败必须保留草稿与占用；取消离开不改变 Backend 状态。
- Command 将路线编辑交给 Map 后，双方显示一致的方案和任务；已锁定的相机、Route V2 单一显示所有者规则保持有效。
- 配置模式不能遮住其他已经执行的编队控制。暂停和终止必须明确显示所控制的执行组，并等待 Backend 确认。
- 完成部署是配置结果，不是飞行开始。成功完成配置后释放占用；失败应留下可恢复的状态。
- 重连与重启先获得权威快照再恢复操作，显示占用者和可用动作，避免使用本地旧模式写入。

配置占用以 `owner_instance_id`、`plan_id` 和有限期限识别，心跳续期。新建和复制成功后，占用绑定新方案；完成部署、确认结束配置或期限届满后释放。`acquired_version` 记录本次占用的取得版本，用来阻止前一次会话的延迟退出请求释放后来重新取得的占用。

心跳会推进全局版本，因此“放弃路线”和“结束配置”不能仅因一次续期将用户困在编辑状态。Backend 允许旧版本的 `discard_route_edit` 释放同一存活路线会话，但必须同时匹配会话 ID、所有者、方案和任务；过期会话、MOVE 会话及未来版本仍被拒绝。旧版本的 `end_configuration` 必须匹配当前所有者与方案，且请求版本不能早于 `acquired_version`；尚有路线编辑时仍拒绝退出。保存路线等内容修改继续使用原有乐观版本校验。

## 自动化工具

`Scripts/PlanConfiguration/verify.py` 默认运行两个测试：`DroneOps.PlanConfiguration.Workspace` 使用 Command 角色，`DroneOps.PlanConfiguration.MapControls` 使用 Map 角色。每个测试分别创建新 Mock 数据目录和 Editor 进程，默认保留真实渲染视口；测试自身负责准备业务数据。

```powershell
python Scripts/PlanConfiguration/verify.py
python Scripts/PlanConfiguration/verify.py --role Command DroneOps.PlanConfiguration.Workspace
python Scripts/PlanConfiguration/verify.py --role Map DroneOps.PlanConfiguration.MapControls
python Scripts/PlanConfiguration/verify.py --role Map --expected DroneOps.P54.RouteEditCameraLock=2 DroneOps.P54.RouteEditCameraLock
```

默认端口为 `22582/22583`。可以通过 `--http-port` / `--ws-port` 改用其他空闲端口。工具拒绝复用端口上的既有 Backend，要求正常 Launcher 解析到规范的 `Backend/build/Release/DroneBackend.exe`，不自动替换成阶段专用或旧版本构建。

运行记录写入新的 `Evidence/PlanConfiguration/verification-<run>`，原始运行数据和进程日志保留在对应 `Saved/PlanConfiguration-verify-<run>-<index>`。通过条件同时要求进程成功退出、报告和日志中的发现/完成/通过数量一致、实际测试名匹配、没有失败或未运行项，并确认所持有进程已清理。保留警告数量，保留每次失败，不用进程退出码单独宣称通过。`--null-rhi` 仅供明确的组件测试，不能代替视口或原生操作证据。

两个既有回归需要专门夹具：`DroneOps.P4.Workflow.MapLiveDraftGuard` 自动运行 `p4_ue_route_fixture.mjs`；`DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo` 自动运行 `p4_workflow_integration.mjs`，准备已部署方案。夹具只作用于该测试拥有的独立 Backend，日志和 JSON 写入本次报告目录，不覆盖历史 P4 证据。

新增 Workspace 测试覆盖配置占用确认、新建表单取消不留空方案、有效创建、脏内容的继续/放弃/保存后退出、查看告警保留配置与输入，以及 Video Target/执行状态隔离。MapControls 测试使用真实 HTTP/WS 和 Mock 执行，覆盖进入配置后取消已预备的框选模式、隐藏集群移动入口、保留既有组暂停/终止控制、终止确认及取消、结束配置后恢复入口。另有 JSON 组件用例检查混合执行组、RETURNING、独立组目标和已结束任务；这些都不等同于原生鼠标验收。

## 原生三客户端工具

`Scripts/PlanConfiguration/qa_host.py` 使用在线 `/Game/Level/CesiumWorld`，默认端口 `22580/22581`，每次创建独立 `Saved/PlanConfiguration-QA-<run>`。它只启动自己持有的 Backend，随后通过该目录的 `command.json` 启停自己的客户端。`--start-all` 可在 Backend 就绪后直接启动 Command / Map / Video。

```powershell
python Scripts/PlanConfiguration/qa_host.py
```

首条 `MOCK_BACKEND_READY` 输出包含本次目录和控制文件路径。每个命令使用新的 `request_id`，最好先写临时文件再原子替换 `command.json`。可用命令如下：

```json
{"request_id":"open-clients-1","action":"start_all"}
{"request_id":"open-map-1","action":"start","role":"Map"}
{"request_id":"close-map-1","action":"stop_role","role":"Map"}
{"request_id":"reconnect-map-1","action":"restart","role":"Map"}
{"request_id":"restart-backend-1","action":"restart","role":"Backend"}
{"request_id":"capture-state-1","action":"snapshot"}
{"request_id":"finish-1","action":"stop"}
```

`status.json` / `ownership.json` 记录 QA 窗口对应的 PID、可执行文件及角色；原生操作必须以这些 PID 识别窗口，不能仅凭与正常用户会话相同的标题。`snapshot` 只读取共享状态。业务写入通过待验证的原生界面完成；控制文件不提供方案创建、编辑或执行命令。

原生夹具以仓库的四架明确标记的 Mock UAV 为基础，默认初始椭球高为 80 米，可用 `--altitude-ellipsoid` 指定。它不是离地高度，也不复制或覆盖用户当前会话数据；验证地图地表时仍应记录实际加载状态和高度基准。关闭时先请求 Mock 任务持久化暂停，再清理持有的进程树；清理失败会明确记录，不能标记完成。

## 原生场景清单

| 场景 | 鼠标/键盘操作 | 必须取得的证据 |
| --- | --- | --- |
| 新建方案 | 打开表单，空值提交、取消；重新填写有效名称后提交 | 取消前后方案数量相同；有效提交只新增一个方案；确认后才进入配置 |
| 告警查看 | 配置中打开并查看告警，再返回 | 无意外配置释放或模式切换；未调用飞行；Video Target 未变化 |
| 集群入口隔离 | 进入配置后查看 Map 工具条；结束配置再查看 | 配置时不能新建集群移动；退出并收到快照后恢复正常入口 |
| 脏内容离开 | 修改方案字段，分别尝试取消离开、保存离开、放弃离开 | 取消保留输入；保存成功才释放；放弃不污染已保存内容；失败可继续编辑 |
| 路线交接 | Command 选任务并进入路线编辑，在 Map 修改、保存后返回 | 方案/任务一致，只有一个路线显示所有者；相机锁行为不变 |
| 活跃组控制 | 一个编队执行时配置另一方案，再在 Map 暂停、恢复或终止原执行组 | 控件仍可达，组 ID/成员明确；配置不错误解除原任务占用；终止需要既有确认 |
| 部署与释放 | 完成配置检查和部署 | 配置占用释放，部署没有产生执行；成功提示与 Backend 状态一致 |
| 重连恢复 | 仅重启 QA 的 Map/Command，必要时再重启 QA Backend | 新进程按权威快照恢复模式/占用；脏内容处理有明确结果；不重复提交任务 |

每个原生场景应保存操作前、操作中和最终界面，以及只读 Backend 快照。组件测试、自动化视口、实际鼠标键盘、Mock 执行和真实设备证据分别记录。没有真实设备动作时，不作真机飞行结论。

## 实际验证结果

以下为 2026-10-18 的阶段性记录，发生在心跳竞争修复及最终构建之前。原始报告均位于 `Evidence/PlanConfiguration/`，测试代码及断言没有为通过而改弱。表格中的“警告”表示 UE 报告里带警告的测试数量，非日志行数。

| 测试 | 发现 / 完成 / 通过 / 失败 | 警告 | 运行目录 |
| --- | --- | --- | --- |
| PlanConfiguration.Workspace + MapControls | 2 / 2 / 2 / 0 | 2 | `verification-1792258135884655000` |
| P54.RouteEditCameraLock 两项 | 2 / 2 / 2 / 0 | 0 | `verification-1792258310870083400` |
| P4.Workflow.MapLiveDraftGuard 首次 | 1 / 1 / 0 / 1 | 0 | `verification-1792258310870083400` |
| MapLiveDraftGuard 无修改追踪重跑 | 1 / 1 / 1 / 0 | 1 | `verification-1792258610031847100` |
| MapLiveDraftGuard 心跳延迟诊断 | 1 / 1 / 0 / 1 | 0 | `verification-1792258727524693800` |
| CommandLiveCRUD + CommandReviewReadonlyCopyAndCombo | 2 / 2 / 2 / 0 | 2 | `verification-1792258799488106200` |

上述运行均确认只清理所持有进程，清理错误和剩余持有进程均为空。CommandReview 的既有 HTTP/WS 夹具另外通过 21 项协议检查。各次 `run.json` 保留规范 Backend 路径、Backend/UE 模块 SHA-256 和 UE 模块运行前后是否一致；例如新增两项测试的 UE 模块为 `1441c11746be9c8577bb498968c0b9636862e861ee88df97ee2dea89145b7f9d`，Backend 为 `b18cc1c2e1aa9bf0177be828971aa2907d1eb971a321486bea5078fdbca0a651`。

MapLiveDraftGuard 首次退出失败后保留了相机锁和 DIRTY 编辑会话。隔离回环代理只将 Discard 请求延迟至一次真实心跳后，重现了相同两个失败断言：请求 `expected_version=7` 收到 `HTTP 409 VERSION_CONFLICT`，权威版本为 8，编辑会话仍在。诊断明确记录了注入的延迟，原始请求/回复及诊断元数据保存在 `verification-1792258727524693800/http-trace.jsonl` 与同目录的 `http-trace-metadata.json`。这是竞争条件诊断，不能用无延迟重跑成功掩盖首次失败。

### 最终 UE 构建回归

以下运行使用同一最终 UE 模块 `60f5412b45ecee3ba11021f38049df5942f0c233eacb9dd8064f68bb598f7048` 和修复后的规范 Backend `5bae091ae8c677af33dbe5bae2e10a5d8abe6d279d57888908eb335e1defd067`。运行前后 UE 模块散列一致，每次进程退出 0，清理错误和剩余持有进程均为空。

| 测试 | 发现 / 完成 / 通过 / 失败 | 警告 | 运行目录 |
| --- | --- | --- | --- |
| PlanConfiguration.Workspace + MapControls | 2 / 2 / 2 / 0 | 2 | `verification-1792259274506787500` |
| P4.Workflow.MapLiveDraftGuard 正常运行 | 1 / 1 / 1 / 0 | 1 | `verification-1792259275501704500` |
| MapLiveDraftGuard 同一心跳延迟诊断重放 | 1 / 1 / 1 / 0 | 1 | `verification-1792259362820560200` |
| P4.Localization.FTextAndDraftIsolation | 1 / 1 / 1 / 0 | 1 | `verification-1792259632962494000` |

最终延迟重放保留了相同测试和断言：Discard 仍带 `expected_version=7`，等待心跳推进至 8 后提交，现在得到 `HTTP 200`、最终版本 9，路线编辑会话释放，相机解锁，待切换的空任务选择得以完成。该运行的 `http-trace.jsonl` 和 `http-trace-metadata.json` 保留注入时序与回复。故意提交的旧版本保存请求仍得到 `HTTP 409 VERSION_CONFLICT`，证明释放修复没有放松路线内容保存校验。

Localization 在最终 GatherText 结束后独立运行，验证保留的 FText 中英文切换及路线草稿隔离；它不覆盖所有新文案的原生排版。最终 GatherText 内部步骤退出 0，但 commandlet 进程因既有 GameFeatureData 启动错误退出 1，原始日志需保留，不能将本地化生成命令本身记为无错误通过。

### Backend 最终构建与协议验证

`backend/revision-lease-release/` 保存修复后的规范 Release 构建身份、完整单元结果和隔离 HTTP/WS 记录。Backend SHA-256 为 `5bae091ae8c677af33dbe5bae2e10a5d8abe6d279d57888908eb335e1defd067`；单元测试二进制为 `eb1aab3f1a922d785723f00e71f61d533c998aca2ebfa24fea1ba74a96d85f7d`。规范构建退出 0，原始构建日志和源文件散列见 `build-identity.json`，证据文件散列见 `manifest.json`。

- 全部单元测试：发现 154、完成 154、通过 152、失败 2、禁用 0。
- 本次新增回归：17/17 通过，其中修复心跳释放竞争的 6/6 通过；包括错误所有者/会话/任务、过期会话、MOVE 会话、未来版本和同一所有者重新取得占用后的旧退出请求。
- 两项保留的基线失败为 `SegmentDistanceTest.Crossing` 和 `AssemblyPlannerTest.ConflictHeightSeparation`。基线比较为 137 项完成、135 通过、同样两项失败，因此不宣称 Backend 全量测试通过。
- 隔离真实 HTTP/WS 协议验证：6/6 检查通过，覆盖 Command 独占、并发拒绝、创建绑定、配置中禁止新移动、上下文和告警查看边界、路线退出门禁及释放后恢复；所属 Backend 已停止，测试集群和端口占用清空。

这些是软件和 Mock 协议证据，不是原生鼠标或真实飞机证据。

### 原生三端操作与最终界面复核

实际鼠标/键盘操作使用独立 Mock 数据和在线 CesiumWorld；没有通过接口代写业务状态。`native/acceptance.json` 记录逐项范围、两个构建身份及限制，截图和只读快照分别保存在 `native/build-2-workflow/`、`native/build-3-recheck/`。

第一轮完整流程（UE 模块 `1441c117...`）完成了新建方案、告警往返保留草稿、UAV-01 分配、Map 绘制航线、保存草稿与完成航线、检查并部署。部署后快照确认配置占用释放、执行数为零。随后通过明确的 Mock 启动确认开始任务并暂停，配置第二份方案时仍可确认终止原执行组，第二份草稿没有丢失；保存后退出，最终两份方案保留，执行状态为 ABORTED。没有将部署描述为飞行。

最终构建（UE 模块 `60f5412b...`）重新检查中文表单和提示，确认重复状态/标题及新建表单中的旧方案摘要已移除；告警页面保留当前方案名、步骤和未保存提示。结束配置时显示保存、放弃、继续编辑选项。继续编辑后成功创建方案并分配 UAV-01，交接到 Map；地图显示“正在编辑航线，完成后返回指挥端”，框选和新建集群移动入口隐藏。添加一个未保存航点并确认放弃后，航点清除、2D/3D 相机控制解锁。最后结束配置，Command 无人机入口及 Map 框选/集群移动入口恢复；快照确认一份方案、无编辑会话、无配置占用、无执行。

原生完整部署/启动/暂停/终止流程是在第一轮构建完成的，最终构建未重复整条流程；最终两项 RHI 自动化覆盖工作区和已有执行组控制。重连、占用过期由 Backend/协议测试覆盖，未记为原生验收。告警页面此次没有实际告警到达，不宣称验收了真实告警处理。没有任何真机飞行结论。

两次 QA Host 均正常退出，清理错误和残留持有进程为空。Launcher 兼容性测试 16/16 通过；缺少 `plan_configuration` 能力的旧 Backend 会在启动流程中被拒绝。

### 公开证据与复核

`Scripts/PlanConfiguration/sanitize.py --apply` 在 `Saved/PlanConfiguration/raw-evidence/` 保留原始文件，将公开副本中的账户、机器和会话标识脱敏，保留测试断言、失败记录、Mock 请求和竞争诊断。`sanitization.json` 记录前后散列。`Scripts/PlanConfiguration/seal.py` 检查最终测试、原生快照、进程清理及本地化产物，生成总清单 `manifest.json`，绑定源文件、二进制及证据散列。

编译初次失败、相对路径 GatherText 配置失败、两次 MapDraftGuard 失败均保留；成功的重跑没有覆盖失败记录。全量 Backend 的两项既有几何失败和 GatherText 进程退出 1 也单独保留，不能把本次验收概括为所有测试全绿。
