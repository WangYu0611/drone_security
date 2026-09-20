# Stage 1 全量功能审计

审计日期：2026-09-20。结论：**CONDITIONAL PASS（静态审计与既有证据核对）**。Stage 1 已有方案 → 任务 → 航线 → 部署 → 显式开始 → Mock 执行 → 恢复的闭环；本轮未启动系统、未复跑运行验收、未接真机。下文“已验证”均指仓库保存的既有验证，不能理解为本轮重新实测。

扫描覆盖 4,323 个受版本管理文件的清单，以及 347 个自有源码/配置文件的专项检索；另只读查看 Saved/Stage1/data。没有仅从 P5.2 报告转述。C++、Backend、Launcher、Jetson、Python 旧包、Config、Content 资产清单、插件声明、历史日志均纳入。二进制 Blueprint 只核对资产存在和 C++ 加载引用，没有启动 Editor 导出全量 Blueprint 图；图内独有逻辑属于 UNKNOWN。

## 1. 证据等级和统计口径

CURRENT_ACTIVE 表示当前角色路径调用；CURRENT_AVAILABLE 表示入口/实现可定位但未证明当前流程使用；LEGACY_PRESERVED 表示旧能力仍存在；HISTORICAL_EVIDENCE_ONLY 表示仅历史材料；STAGE2_RESERVED 表示未交付扩展；DEPRECATED 表示被当前协议替代；UNKNOWN 表示证据不足。状态与“验证”分开。

总表一行是一个可独立讨论的能力组，不能作为按钮数量。ACTIVE_VERIFIED 表示当前入口及相关既有验收均有证据；ACTIVE_PARTIAL 表示依赖、视觉、覆盖或数据条件仍有限制。HISTORICAL_VERIFIED 只在明确的局部历史证据范围使用。没有将测试计划、实现代码或提交标题当成真机验收。

证据索引：

| ID | Source / Test / Evidence |
|---|---|
| S1 | `Launcher/runtime.py`、`windows.py`、`launcher.pyw`、`config.json`、`test_runtime.py` |
| S2 | `Source/UE5DroneControl/Command/CommandScreenManager.cpp`、`CommandShellWidget.cpp`、`CommandCenterPanel.cpp` |
| S3 | `Backend/storage/security_plan_store.h`、`Source/UE5DroneControl/Shared/SecurityPlanWorkspaceWidget.cpp`、`SecurityPlanPanel.cpp`、`DroneMissionService.cpp` |
| S4 | `Backend/storage/mock_execution.inl`、`Shared/ExecutionPresentation.h`、`Map/MapExecutionWidget.cpp`、`RealTimeDroneReceiver.cpp` |
| S5 | `Map/MapShellWidget.cpp`、`MapMissionRouteWidget.cpp`、`Command/CommandMapInteractionService.cpp`、`PathEditor/DroneWaypointActor.cpp`、`DronePathActor.cpp` |
| S6 | `DroneOps/Core/{ICoordinateService,SimpleCoordinateService,CesiumCoordinateService,GeographicTypes}.{h,cpp}`、`DroneOps/Control/DroneOpsGameMode.cpp`、`Backend/conversion/` |
| S7 | `Backend/http/http_server.cpp`、`DroneOps/Network/`、`DroneOps/Core/DroneRegistrySubsystem.{h,cpp}`、`DroneOpsTypes.h` |
| S8 | `Video/VideoShellWidget.{h,cpp}`、`UI/DroneVideoWindow{Widget,Manager}.cpp` |
| S9 | `Shared/OperationalContextSubsystem.cpp`、`Backend/storage/operational_context.h`、`shared_view_store.h` |
| S10 | `Command/CommandAlertStore.cpp`、`CommandAlarmPanel.cpp`、`OperationalLogWidget.cpp`、`Shared/OperationalEventStore.cpp` |
| S11 | `Shared/ProductText.cpp`、`UILanguageSubsystem.cpp`、`Config/Localization/DroneOps.ini`、`Content/Localization/` |
| S12 | `Backend/communication/`、`drone/`、`execution/`、`Jetson/jetson_bridge.py`、`jetson_video_stream.py` |
| E52 | `Evidence/TASK-P5.2/backend/results.xml`、`protocol/result.json`、`ue/final-counts.json`、`native/validation.json`、`native/uninterrupted-execution-22.json` |
| Eold | `Docs/TASK-P1-SharedOperationalContext.md`、`TASK-P2-CommandCenterV2.md`、`TASK-M1-MapClient.md`、`TASK-V2-VideoMultiFeedUI.md`、各自 Evidence/QA |
| EH | `Evidence/STAGE1-AUDIT/historical-telemetry.txt`、`Jetson/视频流调试对话整理.md` |

S2–S11 中省略前缀的 UE 路径均相对于 `Source/UE5DroneControl/`。精确源码行可用 `Evidence/STAGE1-AUDIT/*-scan.txt` 和 `http-source.txt` 定位。

## 2. 功能总表

“历史真机”列的“未证”表示本轮没有找到足以单项签收的历史证据，不否定用户确认的既往联通。Stage 2 列“复用”表示可复用基础，仍需重新验证。

| ID / 功能 | 模块 | 当前存在 | 当前使用 | 分类 | Stage 1 验证 | 历史真机验证 | Stage 2 | 证据 |
|---|---|---|---|---|---|---|---|---|
| F01 Backend Authority | Backend | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 | 不适用 | 复用 | S3/S4/S9 |
| F02 三独立客户端 | 启动 | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 A | 不适用 | 复用 | S1/S2 |
| F03 Launcher 单实例与就绪检查 | 启动 | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | Launcher 7 tests、E52 A | 不适用 | 复用 | S1 |
| F04 Ownership、安全关闭、重启 | 启动 | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 K/L/S/T/U | 不适用 | 复用 | S1 |
| F05 Editor Play 默认 Command、Role 覆盖 | 启动 | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | A3/角色自动化 | 不适用 | 复用 | S2、DefaultGame.ini |
| F06 Standalone 旧入口 | 启动 | 是 | 非默认 | LEGACY_PRESERVED | 本轮未复测 | 未证 | 需评估 | S2、DroneOpsGameMode |
| F07 Overview/Plans/Fleet/Alerts/Logs 五页 | Command | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P5.1/P5.2 | 不适用 | 复用 | S2 |
| F08 新建方案、自动 Task 01 | Command | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 B | 不适用 | 复用 | S3 |
| F09 列表、选择、打开、版本 | Command | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P4/P5.1 | 不适用 | 复用 | S3/S9 |
| F10 DRAFT/READY/DEPLOYED | Plan | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52/P4 | 不适用 | 复用 | S3 |
| F11 Copy as Draft 与独立航线副本 | Plan | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 R | 不适用 | 复用 | S3 |
| F12 草稿删除及删除限制 | Plan | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P4/P5.1 | 不适用 | 复用 | S3 |
| F13 Mission 关联、名称、UAV 分配 | Mission | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 C | 不适用 | 复用 | S3 |
| F14 配置检查、Review、Deploy | Deployment | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 E/F | 不适用 | 复用 | S3 |
| F15 不可变部署快照/已部署只读 | Deployment | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 9 immutable routes | 不适用 | 复用 | S3/S4 |
| F16 显式 Start 与确认 | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 G | 否，Mock | 需接适配 | S4 |
| F17 Executing/Completed/Waypoint/Progress/Duration | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 H/I/P | 否，Mock | 需接适配 | S4 |
| F18 Pause/Resume | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 J | 否，Mock | 需接适配 | S4 |
| F19 Return Home | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 N | 否，Mock 非飞控 RTL | 需接适配 | S4 |
| F20 Abort | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 O | 否，Mock | 需接适配 | S4 |
| F21 Failed 与失败原因 | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | Backend 缺 Mock 测试；非原生故障场景 | 否 | 复用结构 | S4/E52 |
| F22 History、UAV 活跃冲突 | Execution | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 Q/U | 不适用 | 复用 | S4 |
| F23 Registry、Fleet/选择/执行投影 | UAV | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52/Eold | 历史 telemetry；当前 Mock | 复用 | S7 |
| F24 Online/Offline、Mock/Real 身份解释 | UAV | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | Mock 不代表遥测 Online | 历史 Online/Lost | 必须映射 | S7/S4/EH |
| F25 P2 Command 2D tactical map | Command | 是 | 当前未构建 | AVAILABLE_NOT_USED / CURRENT_AVAILABLE | 仅 P2 历史验证 | 不适用 | 可复用 | CommandTacticalMap；当前 CenterPanel 无构建调用 |
| F26 Map Cesium/Georeference | Map | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | 运行证据；影像/插件限制 | 未证单项 | 复用 | S5/S6/E52 |
| F27 Map 2D/3D 切换 | Map | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | M1/A2；3D 低高度遮挡保留 | 未证 | 复用 | S5/Eold |
| F28 Camera/Zoom/Pan/Focus/自动 framing | Map | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | M1/E52 | 未证 | 复用 | S5 |
| F29 地图点击航点/编号/线/材质 | Route | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 D | 未证 | 复用 | S5；Content/Command/Materials/M_CommandPath.uasset |
| F30 Save 保持编辑/Finish 关闭/租约 | Route | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P5.1/E52 D | 不适用 | 复用 | S3/S5 |
| F31 编辑/只读/执行航线/点状态 | Route | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 H/I/R | 否，Mock | 复用 | S4/S5 |
| F32 WGS84 ↔ UE 正反转换 | Geographic | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | 单测/Map 保存与渲染；不等于测绘精度 | 未证单项 | 复用 | S6 |
| F33 GCJ-02 ↔ WGS84 栅格、WebMercator | Map | 是 | 按配置 | ACTIVE_PARTIAL / CURRENT_ACTIVE | 代码、旧地图证据；外部影像受限 | 未证 | 复用 | DroneOpsGameMode.cpp |
| F34 UE cm ↔ NED m、GPS anchor | Coordinate | 是 | 真机分支 | LEGACY_PRESERVED | 保存单测；非本轮真机 | 锚点/遥测历史日志 | 复用 | S6/S12/EH |
| F35 地理点派发/批量坐标/新鲜度检查 | Legacy Control | 是 | 非 Mock 主线 | LEGACY_PRESERVED | 解析/代码证据 | 未证完整派发 | 复用 | GeographicTypes、DroneOpsPlayerController |
| F36 原路径保存/加载/编队/序列预演 | Legacy UI | 是 | 非主线 | LEGACY_PRESERVED | 旧文档/代码 | 未证 | 复用评估 | PathEditor、UI/SequenceDispatchPanelWidget |
| F37 本地敌机/目标/侦察与攻击预演 | Legacy UI | 是 | 非主线 | LEGACY_PRESERVED | 代码；不代表真实识别或武器控制 | 未证 | 不纳入承诺 | HostileTargetManager、DroneTaskManager |
| F38 UDP Telemetry 和状态机 | Real UAV | 是 | 非 Mock 主线 | LEGACY_PRESERVED | 非本轮真机 | 历史接收/Online/Lost | 复用 | S12/EH |
| F39 JSON v1 move/hold、ACK、队列 | Real UAV | 是 | 非 Mock 主线 | LEGACY_PRESERVED | 协议/队列测试；非飞行 | 历史发送；端到端 ACK 未证 | 复用 | S12 |
| F40 ROS2 PX4 Offboard/Arm/Disarm | Real UAV | 是 | 未启动 | LEGACY_PRESERVED | 本轮未验证 | 用户确认联通；逐动作未证 | 复用 | Jetson/jetson_bridge.py |
| F41 旧航线集结/执行引擎 | Real UAV | 是 | Mock 不调用 | LEGACY_PRESERVED | 有单测及两项失败 | 未证完整飞行 | 需修订验收 | Backend/execution、S7 |
| F42 24/32 byte UE UDP 包 | Legacy Protocol | 是 | 当前桥不接受 | LEGACY_PRESERVED / DEPRECATED | 不兼容 JSON v1 | 未证 | 不可直接复用线上格式 | ue_px4_msgs/packets.py |
| F43 独立 Video Shell/显式目标/Retry/Empty | Video | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P4/E52/Video test | 本轮非真视频 | 复用 | S8/S9 |
| F44 4/6/Focus 与主副卡交换 | Video | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | 单播放器；其他卡片预留 | 未证多路真视频 | 多路需开发 | S8 |
| F45 Mock MP4 fixture | Video | 是 | 当前地址为空 | AVAILABLE_NOT_USED / CURRENT_AVAILABLE | 旧合成播放证据 | 不适用 | 可用于演示 | Scripts/video_demo_fixture.py、Docs/TASK-V1/V2 |
| F46 FollowTarget 开关 | Video | 方法保留 | 固定 false | LEGACY_PRESERVED / DEPRECATED | 当前无自动跟随功能 | 不适用 | 需产品决策 | VideoShellWidget.cpp:368 |
| F47 Jetson ROS 相机→GStreamer→RTSP→WebRTC | Real Video | 是 | 当前未接 | LEGACY_PRESERVED | 本轮未验证 | 仅相机取图局部历史证据 | 复用 | S12、tools/MediaMTX |
| F48 逐帧视频元数据/SfM 采集基础 | Video Metadata | 是 | 非主线 | LEGACY_PRESERVED | Backend 单测，非 SfM 成果 | 未证完整采集 | 复用 | video_metadata_store、jetson_video_stream |
| F49 Alert 会话 Store 与 UI | Alert | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P1/P2 | 非当前真机 | 复用 | S10 |
| F50 Operational Event 与 Log 投影 | Log | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P2/P4/E52 | 非当前真机 | 复用 | S10 |
| F51 Shared Context/晚加入/重连/版本过滤 | Context | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | P1/P4/E52 | 不适用 | 复用 | S9 |
| F52 UAV 选择不切 Video | Context | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 video-target-independent/P4 | 不适用 | 保持 | S8/S9 |
| F53 en/zh-Hans 和共享偏好 | Localization | 是 | 是 | ACTIVE_PARTIAL / CURRENT_ACTIVE | E52 M；旧 UI 仍有硬编码 | 不适用 | 补齐旧 UI | S11 |
| F54 Backend 文档持久化与重启恢复 | Persistence | 是 | 是 | ACTIVE_VERIFIED / CURRENT_ACTIVE | E52 S/T/U | 非真机重启恢复 | 复用 | S1/S3/S4/S9 |
| F55 Python GPS→ENU→NED 工具 | Legacy Coordinate | 是 | 无当前主线调用证据 | LEGACY_PRESERVED | 本轮未运行 | 未证 | 需评估近似精度 | ue_px4_msgs/utils.py |
| F56 已删除 LocalPreviewIsolationToggle UI | Legacy UI | 历史存在 | 否 | HISTORICAL_ONLY / HISTORICAL_EVIDENCE_ONLY | 不适用 | 不适用 | 非必需 | 8941c55 父提交、history.txt |
| F57 相机 YUYV→rgb8 局部成功记录 | Historical Video | 记录存在 | 非当前验收 | HISTORICAL_VERIFIED / HISTORICAL_EVIDENCE_ONLY | 否 | 历史对话确认，非 UE 播放 | 复用经验 | Jetson/视频流调试对话整理.md:11 |
| F58 Stage1 Plan→真实飞控统一适配 | Integration | 无统一桥 | 否 | STAGE2_RESERVED | 未实现 | 不适用 | 待设计 | S4 与 S12 为分离链路 |
| F59 多路真实视频/AI/LAN 多机部署 | Stage2 | 不完整 | 否 | STAGE2_RESERVED | 未验证 | 未证 | 预留方向 | P5.2 deferred scope |
| F60 glTFRuntime 实际依赖 | Plugin | 未找到声明 | 未证 | UNKNOWN | 未验证 | 未证 | 需原集成资料 | uproject/Build.cs/插件清单 |
| F61 Blueprint 图内独有逻辑 | Assets | uasset 存在 | 未完全判定 | UNKNOWN | 未导出图 | 未证 | 待专项 | tracked-inventory/blueprint-references |
| F62 Cesium ECEF 内部路径 | Geographic | 插件接口后方 | 未逐层检查 | UNKNOWN | 本工程未显式 ECEF 接口 | 未证 | 依赖插件核实 | S6/uproject |

## 3. 架构和业务约束

总表统计：62 个能力组。ACTIVE_VERIFIED 32，ACTIVE_PARTIAL 7，AVAILABLE_NOT_USED 2，LEGACY_PRESERVED 14，HISTORICAL_ONLY 1，HISTORICAL_VERIFIED 1（局部相机取图记录），STAGE2_RESERVED 2，UNKNOWN 3。Active Unverified/Partial 按7组统计；历史类合计2组。不能与按钮数量、测试数量相加。

Backend 是共享业务 Authority。Command 提交操作，Map 编辑航线并呈现 Backend 位置，Video 呈现独立 video target。三个 UE 实例各自拥有 Registry、Widget 和内存，不共享静态跨进程对象。Launcher 管进程与布局，不承担 Plan/Execution Authority。

Plan 仅有 DRAFT/READY/DEPLOYED。删除仅允许 DRAFT 且无编辑会话；DEPLOYED 只读，复制生成新 IDs/航线。修改内容增加 content_revision 并使旧检查/复核失效；语言、选择、视频不改内容修订。自动 Task 是当前 UI 传 `default_mission_name` 才创建，旧 API 调用仍可创建空 Plan。

Execution 是独立集合、独立 execution_version，控制另有 control_version/request_id。部署并不开始移动；Start 才创建执行。Return 是 Mock 直线回独立 Home，Abort 终止，Pause 停积分。CREATED/PREFLIGHT/STARTING/EXECUTING/PAUSED/RETURNING 都占用 UAV。Backend 的 Mock 检查不是地形、空域、电量或真实飞行安全检查。

重要纠偏：P2 tactical map 仍有实现和测试，但当前 CommandCenterPanel 只创建 SecurityPlanWorkspaceWidget，TacticalMap 成员未实例化。现场地图能力应在 Map Client 展示，不能把历史 Command 页面截图作为当前 UI。

## 4. 已保存的验证证据

| 层级 | 核对结果 | 边界 |
|---|---|---|
| Backend | XML 77 discovered，75 无 failure，2 failure | `SegmentDistanceTest.Crossing`、`AssemblyPlannerTest.ConflictHeightSeparation` 原样保留 |
| Protocol | P4 21/21、P5 8/8、P5.1 11/11、P5.2 14/14 | 已保存 synthetic 运行；不是 native UI 或真机 |
| UE | Command 3、Map 3、Video 1；7 个 SucceededWithWarnings，最终 0 failed | Map 单测独立 Editor；失败网络/连续 PIE 尝试保留 |
| Native | P5.2 A–U 21 项 ledger；validation 2946 observations、9 条不可变 route；E22 正常完成 | 多次执行/恢复组合；只有 E18/E22 是单独不中断完成试验 |
| 历史设备 | 2026-07-24 UDP 接收、GPS 锚点、Offline/Online/Lost、move 发出 | 不证明飞行结果、所有控制 ACK、相机到 UE 全链路 |
| 本次 | 源码/配置/历史及文件校验 | 新运行测试 0；无设备访问 |

既有截图保留 1082 vs 1080 的窗口客户区差异；没有以修改坐标、抹掉测试失败、合成截图替代实测。短时性能样本只说明当次资源占用，不是容量认证。

## 5. 问题清单（只记录）

| 级别 | 标记 | 发现 / 影响 | 来源 |
|---|---|---|---|
| BLOCKER（若要求现场实时视频） | UNVERIFIED | 当前 Saved 的四个 video_url 全为空，video_view 文件未生成；不能承诺开机即播放 | Saved/Stage1/data/drones.json |
| BLOCKER（若要求当前真机验收） | UNVERIFIED | 本轮无真机接入，Stage1 Mock 未接真实执行适配 | S4/S12 |
| MAJOR | ORPHANED（当前 UI 路径） | Command tactical map 未实例化；P2 历史能力与当前界面不同 | S2/F25 |
| MAJOR | STALE | 旧 24/32 byte 包仍存在，当前 Jetson 只收 JSON v1；混用会拒包 | ue_px4_msgs/packets.py、jetson_bridge.py::parse_control_packet |
| MAJOR | UNVERIFIED | 真机 AMSL→椭球高依赖 GeoidSeparationMeters，当前 0；现场正确值未证 | DefaultGame.ini、DroneNetworkManager.h |
| MAJOR | BROKEN（测试断言） | 两项旧规划器回归失败；不能宣传完整多机避碰验收 | results.xml |
| MAJOR | UNVERIFIED | Cesium 图层网络、3D 低高度遮挡、连续 PIE crash 既有边界 | E52 和 A2/M1 |
| MAJOR | STALE | ApiUpdateDrone 修改 IP/port 后只 SaveDrones，没有同步调用 DroneManager/UdpSender 更新目标；重启 LoadDrones 才使用新配置 | http_server.cpp:691 |
| MAJOR | UNVERIFIED | Backend 通用服务仍监听 0.0.0.0；客户端身份/role 不是认证。Launcher endpoint 用 loopback 不等于服务仅绑定 loopback | http_server.cpp::RunHttpServer/RunWsServer |
| MINOR | STALE | DefaultGame.ini “Video reserved”注释过时；Video 已实现 | DefaultGame.ini:6 |
| MINOR | STALE | Jetson 文件头描述旧 global 字段，实际读取 gp.lat/gp.lon/gp.alt；接回设备需核对 px4_msgs 版本 | jetson_bridge.py:16、832 |
| MINOR | UNVERIFIED | 历史地理输入/编队等 UI 硬编码未全部本地化 | localization-candidates.txt |
| INFO | ORPHANED | FollowTarget 方法固定 false；Backend debug/旧 arrays 对当前 Mock UI 不使用，并非服务不存在 | S7/S8 |
| INFO | ORPHANED | 根目录 DroneLinkMonitorSubsystem.cpp/.h 保留延迟统计实现，但在 Source 模块外且未找到当前 Source 引用；不能宣传为当前链路测量。Backend latency_tester 是另一实现 | 根目录源码、Backend/communication/latency_tester.h |
| INFO | UNVERIFIED | Blueprint 内部图、glTFRuntime、ECEF 内部链未证；不要推断已丢失或已启用 | F60–F62 |

## 6. 外部依赖与旧能力保留

| 依赖 | 用途/当前用法 | Stage 1 | Stage 2 / 限制 |
|---|---|---|---|
| CesiumForUnreal/CesiumRuntime | Map 地理转换、tileset/raster | Map 必需 | 图层网络/插件诊断；地形不是飞行安全保证 |
| WebBrowserWidget | UWebBrowser 单播放器 | Video 必需 | 浏览器页可播不等于多路硬解 |
| HTTP/WebSockets/Json/JsonUtilities | 客户端数据交换 | 必需 | 原生 UE 模块；不是 SimpleWebSocket 必需 |
| SimpleWebSocket 项目插件 | 保留旧插件源码 | 当前主网络用 UE WebSockets | 既有 5.7/5.8 兼容诊断，未本轮修复 |
| Boost system/json/beast、yaml-cpp、nlohmann_json、spdlog | Backend HTTP/WS/UDP/YAML/JSON/日志 | 必需 | CMakeLists/vcpkg；运行库需部署 |
| ROS 2/rclpy/px4_msgs/Micro XRCE-DDS | Jetson ↔ PX4 | Mock 不依赖 | 外部运行环境未在本机复验 |
| GStreamer/MediaMTX/FFmpeg | 相机编码/转流/演示源 | 空态 UI 不依赖真媒体 | Stage 2 真视频需恢复，脚本存在不保证安装 |
| ProceduralMeshComponent/ImageWrapper | 路线/地图平面/纹理 | 代码依赖 | 沿用 |
| glTFRuntime | 当前 uproject/Build.cs 无声明 | 未证明依赖 | UNKNOWN；不能据模型资产猜测 |
| Editor MCP/Modeling/StateTree 等 | 编辑器工具/模板 | 依角色区分 | uproject 完整列表为准；不等于飞控插件 |

旧路径编辑/JSON 保存、编队旋转、序列派发、地理点输入、接收机/影子机、本地目标预演仍有代码/部分资产。它们没有因 Stage 1 新 UI 被整体删除，但当前主线没有逐个暴露，也没有重新验收所有 Blueprint 入口。

Git 仅作溯源附录：基线 `e9ce394`，开始 clean；本次只增加审计文档/证据，不修改产品、数据或既有记录，不提交、不 push/merge/tag。

## 7. 本地化专项

主流程使用 `ProductText` 的 NSLOCTEXT/FText key，`UILanguageSubsystem::ApplyConfirmed` 设置 culture；`Config/Localization/DroneOps.ini` 配置 GatherText，`Content/Localization/DroneOps/{en,zh-Hans}` 含 po/archive/locres。Backend 参与语言偏好存储与广播，不直接替UE渲染翻译。当前不能把全部实现统称为“所有文字来自String Table”：可核实的是 FText/NSLOCTEXT 与已生成资源。

| 类型 | 位置 | 已确认例子 | 可见范围 |
|---|---|---|---|
| hardcoded Chinese | `UI/GeographicTargetPanelWidget.cpp:475`、`:605` | 海拔（m，平均海平面）；(经度, 纬度, 海拔 m) | 旧地理工具 |
| hardcoded Chinese | `UI/DefaultSegmentSpeedWidget.cpp:81`、`:87`、`:129` | 飞行速度；新航点和地图点击移动；应用 | 旧速度工具 |
| hardcoded Chinese | `DroneOps/Core/GeographicTypes.cpp` | 坐标解析失败消息 | 旧输入/批量解析 |
| hardcoded English | `Shared/SecurityPlanPanel.cpp:41`、`:72`、`:107`、`:115` | ASSIGNMENT UAV；Backend state updated；Waiting for Backend...；Backend not connected | 旧Panel；当前主流程改用Workspace |
| 标识/单位，不直接算翻译缺陷 | `DroneWaypointActor.cpp:98`、`HostileTargetActor.cpp:62`、`DefaultSegmentSpeedWidget.cpp:118` | D1\|S:0；T-0；m/s | 技术标签需产品判定 |

全量候选在 `Evidence/STAGE1-AUDIT/localization-candidates.txt`；其中包含已本地化调用，仅用于索引，不能把每条扫描命中都判成缺陷。Blueprint 图内部文字未导出，明确不承诺全工程零硬编码。
