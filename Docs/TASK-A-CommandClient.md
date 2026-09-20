# TASK-A Command Client（UE 5.8）

本次实现位于 `feat/triple-screen-command`。**最终集成结论：ACCEPTED FOR INTEGRATION**。用户已完成人工验收并明确授权将 Command Client 合并到 `feat/triple-screen`。原始自动化失败、验证边界和未验证项继续保留；集成接受不代表把所有测试改记为 PASS。本次只更新报告并执行 Command Client 集成，不增加功能、不重构、不修改测试或 Backend、不开始 Video Client 开发，也不合并到 `main`。

## 人工验收与集成接受（用户回报）

以下结果来自用户本次人工验收回报，不是代理重新运行测试所得：

- 地图物理鼠标点击 UAV：**PASS**。
- Selected UAV 高亮辨识：**PASS**。
- UAV 列表物理滚动：**NOT FULLY VERIFIABLE WITH CURRENT 3-DRONE DATASET**。当前只有 3 架无人机，没有超过可视区域，无法产生实际滚动距离；不是功能失败，不标记为 PASS，不为补测修改生产代码或伪造正式数据。
- 用户接受 `1920×1082` 与 `1920×1080` 的 2 px 差异，分类仍为 **VALIDATION / CAPTURE GEOMETRY MISMATCH**，不作为 Command Client 集成阻塞项；原截图高度断言仍为 FAIL，未修改断言或测试记录。

上述人工结果与已有 Command 核心检查、最新 UI 视觉复核共同支持 **ACCEPTED FOR INTEGRATION**。集成目标仅为 `feat/triple-screen`；多 DPI、完整在线地图影像、真实媒体流及其他既有验证边界不因此被宣称已验证。

## TASK-A 最终验收（2026-09-11 13:47，Asia/Shanghai）

验收代码基准：`76fd84c9926b11d5870ce4fe09e4144f04d67adc`。启动前已确认当前分支为 `feat/triple-screen-command`，工作区干净；本地 HEAD、`origin/feat/triple-screen-command` 和 GitHub 实际远程分支 SHA 全部一致。没有切换其他分支。

本轮成功启动 UE 5.8，进入 `/Game/Level/CesiumWorld` 的实际 `ClientRole=Command` 界面。仅运行已有定向检查 `DroneOps.Command.CesiumWorld`，没有重跑全部自动化或 Backend 测试。隔离配置仍位于 Saved，Backend 控制目标均为 127.0.0.1；以模拟遥测经真实 Backend/HTTP/WS 验证客户端，不代表真机飞行验收。使用本机现场验证的代理启动参数 `-httpproxy=127.0.0.1:7897`，未改系统/项目代理配置。

### 最终结论与逐项证据

| 验收项 | 最终结果与边界 |
|---|---|
| UE 5.8 编译 | **PASS**。沿用与当前代码一致的 `build-layout-final.log`，`Result: Succeeded`，无 C++ warning/error；本轮没有改代码或无必要地重新编译 |
| 实际 Command 启动 | **PASS**。日志显示 `Shell created, role=Command`，PIE 成功启动，Shell 销毁/重建检查完成 |
| 左区标题、字号、文字、按钮 | **视觉 PASS**。最新截图标题与旧视频入口不再相互遮挡，三行 UAV 的字号清晰，按钮没有先前的异常换行；未见严重文字截断 |
| Situation / Task / Alarm 布局 | **视觉 PASS**。区域分界清楚、无明显互相覆盖；告警超出可见区域时在自身 ScrollBox 内裁剪，有可见滚动条，不是覆盖任务区 |
| UAV 列表与滚动 | 三架 QA 无人机正常显示；用户确认当前 3 架数据没有超出可视区域，无法产生实际滚动距离。**NOT FULLY VERIFIABLE WITH CURRENT 3-DRONE DATASET**，不是功能失败，也不标记为 PASS |
| SelectedDrone 视觉 | **左侧高亮与详情同步 PASS**。QA UAV-3 显示青色背景和“主选中”，中区详情同步为 QA UAV-3/d3、在线、真实测试电量和 GPS。用户补充人工验收：**Selected UAV 高亮辨识 PASS** |
| 中区地图容器 / 指挥入口 | **容器与入口 PASS**。World、原 Drone Actor、相机 Focus 和 Command Panel 正常运行，指挥面板在中区上部，保留主要地图空间；坐标输入、规划、确认、取消、原停止/派发序列入口仍在 |
| Left → Registry → Center | **定向逻辑检查 PASS**。广播原列表真实选择按钮事件，检查 Registry Primary，再调用原 Actor/Focus 接口；不是物理鼠标点击验收 |
| Center → Registry → Left | **适配器闭环 PASS**。地图适配选择更新 Primary/Multi，新增的前一列表行选择文字反向验证通过；用户补充人工验收：**地图物理鼠标点击 UAV PASS**。不据此扩大为所有 UI 输入场景均已测试 |
| Alarm | **PASS**。真实 Backend WS 低电量事件进入 Store/UI；带 DroneId 的告警选择断言及 Shell 重建后历史保留断言通过。遥测源是明确标注的本机测试输入，生产 UI 未加入静态假数据 |
| Path / Waypoint / Playback | **PASS（本地流程）**。原 Path Editor 进入、地理航点添加、原确认弹窗、本地 DronePathActor 播放、取消清理均通过；日志记录 shadow playback started。未向真实无人机派发任务 |
| 原视频窗口回归 | **PASS（生命周期）**。原 Blueprint 加载、about:blank 打开、去重和重复关闭检查通过；VideoWindowManager/Widget 代码与基线一致。真实 MediaMTX 流不作为本次最终阻塞项 |
| 1920×1080 / DPI | 使用 1920×1080 启动参数和 PIE 设置；最新实际截图为 **1920×1082**，视觉上未见严重布局问题。**精确截图高度断言 FAIL**，用户已接受此 2 px 差异为非集成阻塞项；其他 DPI 比例未测试 |

Command 同步检查日志为 `Command synchronous checks completed; new errors=0`。随后截图检查发现高度为 1082 而非 1080，因此原始自动化结果必须保留：**1 项执行，0 Success / 1 Fail / 0 NotRun，1 error / 37 warnings，耗时 16.963 秒**。唯一测试 error 是 `Expected 'viewport height' to be 1080, but it was 1082`。未调整断言、裁切图片或抑制错误以获得 PASS。

这项差异归为 **VALIDATION / CAPTURE GEOMETRY MISMATCH**。源代码调用 Slate 对 SViewport 截图，输出的是该 Widget 截图尺寸；现有证据只证明尺寸相差 2 像素，尚未独立确认具体边框/DPI 成因，不能武断宣称是已确认的系统错误。它不是已观察到的严重 Command 布局失效，但也不能隐藏自动化失败。此前因此记录为“有条件通过”；现经用户明确接受为非阻塞项，集成结论更新为 **ACCEPTED FOR INTEGRATION**，原始自动化结果不变。

### 外部问题与历史失败分类

- **ENVIRONMENT / EXTERNAL CONNECTIVITY ISSUE**：前两轮 Cesium 瓦片请求的 SSL/ConnectionError。此次定向窗口内没有再次记录 Cesium tile error，但截图未证明完整在线影像加载，不能据此宣布外部影像问题已修复；World、Actor、Registry、Command UI 的运行证据单独成立。
- **PRE-EXISTING FAILURE**：Backend 的 `SegmentDistanceTest.Crossing` 和 `AssemblyPlannerTest.ConflictHeightSeparation`。已确认 Backend 全目录与基线无代码差异；保留此前 42/44 结果，本次未重跑或修改几何算法。
- **PRE-EXISTING / THIRD-PARTY COMPATIBILITY OBSERVATION**：仍有 Cesium 插件枚举初始化日志、SimpleWebSocket 的 UE 5.7 兼容性警告，以及截图中的原场景曝光调试提示。没有将它们描述成新 Command 逻辑失败，也未修改插件/曝光参数消除提示。
- **EXECUTION POLICY（历史）**：以前的第三轮启动被拦截，保留历史记录；本次启动成功，不能把本次标为 `FINAL_RUNTIME_UI_VERIFICATION NOT EXECUTED DUE TO EXECUTION POLICY`。

### 保留的验证边界与集成决定

最新布局的运行截图已经实际检查，因此不再标记 `Latest UI layout runtime visual verification pending`。物理地图点击 UAV、Selected UAV 高亮辨识已由用户人工验收 PASS。列表实际滚动仍为 **NOT FULLY VERIFIABLE WITH CURRENT 3-DRONE DATASET**；多 DPI 比例、精确 1080 像素截图断言通过证明、完整在线地图影像以及未覆盖的其他输入场景继续保留为未验证。真实媒体流未验证，原视频窗口生命周期已有通过证据；视频真流与 Backend 历史失败均不作为本次集成阻塞项，也不新增修复工作。

**集成决定：ACCEPTED FOR INTEGRATION。** 用户已明确授权从 `feat/triple-screen-command` 合并到 `feat/triple-screen`，接受上述非阻塞限制；此次授权不包括合并到 `main` 或开始 Video Client 开发。合并前同步两个远程分支并确认工作区干净；如出现冲突立即停止，不自行选择版本。实际分支 SHA、合并结果与远端同步状态以本次交付 Git 输出为准。

证据均保存在本机忽略目录：

- `Saved/CommandQA/FinalSmoke/index.json`：完整定向结果与事件。
- `Saved/CommandQA/final-smoke.log`：本轮 UE 启动及运行日志。
- `Saved/CommandQA/FinalSmoke/command-shell.png`：最新实际截图，1920×1082；SHA256 `377E7F5A9704F1CEBBA5164C1D1D0D8EA127A2CFB4D2B574F2F3F8739252BB7B`。
- `Saved/CommandQA/final-backend.stdout.log`：本轮真实 loopback Backend 运行记录。
- `Saved/CommandQA/build-layout-final.log`：当前代码编译 PASS 证据。

本轮测试创建的 UE 进程已自行退出，已关闭本轮创建的 Backend 和遥测输入；未停止本机代理或其他既有服务。源码、测试、Content 和 Config 均未修改，只有本报告更新；报告提交后的 HEAD / upstream / git status 以最终交付 Git 输出为准。

## 实现与使用入口

- 使用 UE 5.8 打开 `UE5DroneControl.uproject`，进入 `/Game/Level/CesiumWorld` 后运行。
- `Standalone` 和 `Command` 均创建同一套 Command Shell。默认来自 `Config/DefaultGame.ini` 的 `[CommandClient] ClientRole=Standalone`，启动参数 `-ClientRole=Command` 可覆盖。
- Shell 使用 C++ UMG 构建，无须新增或重存 Blueprint/Content。左区占窗口宽度 28%，中区使用原游戏视口的透明覆盖层。这是单窗口逻辑分区，尚不是两个独立 Monitor 窗口，也不是裁剪后的独立地图视口。
- 左区显示 Registry 可推导的注册数、在线数、活动任务数、待命数、未处理告警、无人机列表与任务状态。通信质量和 AI 标为未接入；未知电量/GPS/任务显示 N/A 或未收到状态。
- 列表的“选择无人机”按钮调用 Registry Primary/Multi Selection；地图仍使用原控制器选中流程。Manager 订阅 Primary Selection，将选中投影到原高亮和相机接口；地图 Actor 尚未生成时延后重试。
- 中区提供定位、开始规划、删除选中航点、确认/预演/派发、取消、坐标输入、暂停/恢复请求。坐标输入、原预演确认弹窗、Playback 和 `/api/arrays` 派发流程继续复用现有实现。返航明确禁用。
- 暂停/恢复仅在后端可发送、WebSocket 连接、所选无人机在线时允许提交请求；不乐观修改任务状态。
- 告警订阅原 `OnDroneWsAlert`，记录 UTC 接收时间、DroneId、类型、消息。独立 GameInstance Subsystem 保存最近 200 条会话记录，支持移除与标记已处理；不是磁盘持久化告警历史。
- 原告警协议不含位置，所以告警通过 DroneId 联动选择；位置接口保留 TODO，不伪造 GPS。

## 架构与数据流

```text
Before: PlayerController -> UIManager -> 多个独立 Widget

After:
UE / CesiumWorld / DroneOpsGameMode
    -> DroneOpsPlayerController
        -> CommandScreenManager（Controller 生命周期）
            -> CommandShellWidget
                -> Left: 原 DroneList + AlarmPanel + 态势/任务投影
                -> Center: 原视口 + 信息卡 + 指挥入口
            -> 原 SequenceDispatchPanel / GeographicTargetPanel
        -> 原地图输入 / Drone Actors / Path / Playback

既有 Blueprint -> UIManager -> CommandScreenManager
                              （非 Command 上下文仍走原接口）

DroneBackend -> DroneNetworkManager -> DroneRegistrySubsystem -> Command UI
Left/Center Selection -> Registry Primary/Multi -> Left/Center Projection
DroneNetworkManager.OnDroneWsAlert -> CommandAlertStore -> AlarmPanel
Command UI -> CommandMapInteractionService -> 原 Controller / 路径面板
```

新增代码没有第二份 SelectedDroneId、无人机注册表或遥测缓存。Manager 的定时刷新只合并界面投影更新，数据仍从 Registry 读取。Widget 显示的文本不是新的业务权威状态。

Manager 提供 Initialize/Show/Hide/Destroy，销毁时清定时器、解绑 Registry 委托并释放适配器与子 Widget。Hide 保留原路径面板及告警状态；完整销毁不销毁 GameInstance 中的告警存储。

### 必须保留与后续解耦点

| 保留对象 | 原因 / 本次边界 |
|---|---|
| DroneNetworkManager / DroneWebSocketClient | 保持唯一 HTTP、WS 接入与解析，未重写 |
| DroneRegistrySubsystem | 注册、遥测、任务、选中的唯一进程内权威 |
| DroneOpsGameMode / DroneOpsPlayerController | 原地图启动、输入、Actor、相机；仅加入管理器和适配入口 |
| DroneListWidget / DroneListItemWidget | 复用原注册表列表与数据更新，Command 布局隐藏并阻断手动网络刷新入口 |
| CesiumWorld / CoordinateService / RealTimeDroneReceiver / MultiDroneCharacter | 原地图、坐标转换、镜像/影子机 |
| DronePathActor / DroneWaypointActor / SaveLibrary / PlaybackManager | 原航点与路线结构、预演执行 |
| SequenceDispatchPanel / GeographicTargetPanel | 原确认、坐标输入与派发流程 |
| UIManagerBlueprintLibrary | 保留旧 Blueprint 接口，向有生命周期的 Manager 迁移 |
| DroneVideoWindowManager / DroneVideoWindowWidget | 原视频生命周期完全保留，本次无源码修改 |

后续可从 `CommandScreenManager` 抽取多窗口宿主，替换 Shell 的左/中承载方式。继续逐步迁移 UIManager 静态缓存和 PlayerController 的旧 UI/选择兼容字段，但不要新增并行状态系统。原 Sequence 面板仍包含流程执行逻辑，未来可在保持接口的前提下逐步服务化。

`OnRequestOpenVideo(DroneId)` 仅是边界事件，本次没有订阅者或视频按钮。Video 枚举仅预留并跳过 Command Shell，不等于完成 Video Client，也不保证切断原视频/其他旧 UI 的初始化。

## 新增文件（逐项）

以下路径均相对仓库根目录：

1. `Source/UE5DroneControl/Command/CommandScreenManager.h`：角色、生命周期、宿主及事件接口。
2. `Source/UE5DroneControl/Command/CommandScreenManager.cpp`：创建/释放、Registry 订阅、旧面板承载。
3. `Source/UE5DroneControl/Command/CommandShellWidget.h`：Shell 组件与界面刷新声明。
4. `Source/UE5DroneControl/Command/CommandShellWidget.cpp`：左/中布局、真实状态投影与操作入口。
5. `Source/UE5DroneControl/Command/CommandMapInteractionService.h`：选中、聚焦、规划与暂停请求适配接口。
6. `Source/UE5DroneControl/Command/CommandMapInteractionService.cpp`：调用原控制器/面板，不复制路径或相机系统。
7. `Source/UE5DroneControl/Command/CommandAlertStore.h`：会话告警记录与状态事件。
8. `Source/UE5DroneControl/Command/CommandAlertStore.cpp`：绑定原 WS 告警、200 条上限、标记/删除。
9. `Source/UE5DroneControl/Command/CommandAlarmPanel.h`：告警面板与生命周期接口。
10. `Source/UE5DroneControl/Command/CommandAlarmPanel.cpp`：告警列表、选择/处理/清除入口。
11. `Source/UE5DroneControl/Command/CommandActionButton.h`：传递 Action/ItemId 的轻量按钮。
12. `Source/UE5DroneControl/Command/CommandActionButton.cpp`：按钮事件路由，不保存业务状态。
13. `Source/UE5DroneControl/Tests/CommandClientTest.cpp`：告警存储测试、CesiumWorld 编辑器集成测试。
14. `Docs/TASK-A-CommandClient.md`：本交付与验收说明。

## 修改文件（逐项）

| 文件 | 修改原因 |
|---|---|
| `UE5DroneControl.uproject` | EngineAssociation 改为用户要求的 5.8 |
| `Source/UE5DroneControl.Target.cs` | UE 5.8 V7 构建默认值和包含顺序 |
| `Source/UE5DroneControlEditor.Target.cs` | 同步 UE 5.8 Editor 构建设置 |
| `Source/UE5DroneControl/UE5DroneControl.Build.cs` | 仅 Editor 目标增加 UnrealEd，支持编辑器自动化测试 |
| `Config/DefaultGame.ini` | CommandClient 角色配置，原网络配置保留 |
| `Source/UE5DroneControl/DroneOps/Control/DroneOpsPlayerController.h` | Manager 所有权和小型地图适配入口 |
| `Source/UE5DroneControl/DroneOps/Control/DroneOpsPlayerController.cpp` | 初始化/销毁 Manager、复用聚焦/删除/高亮，并限制左区点击与跨区框选落点 |
| `Source/UE5DroneControl/UI/UIManagerBlueprintLibrary.cpp` | 原 Show/Hide 接口在 Command 上下文转交 Manager |
| `Source/UE5DroneControl/UI/DroneListWidget.h` | Command 嵌入布局声明与 Panel Slot |
| `Source/UE5DroneControl/UI/DroneListWidget.cpp` | 拉伸布局、折叠旧控件、Registry 投影、阻断 Command 手动网络刷新；无状态时不显示失联 |
| `Source/UE5DroneControl/UI/DroneListItemWidget.h` | Command 行状态摘要和 Primary 委托 |
| `Source/UE5DroneControl/UI/DroneListItemWidget.cpp` | Registry 单选、颜色与摘要投影、委托去重与解绑 |
| `Source/UE5DroneControl/UI/SequenceDispatchPanelWidget.h` | 公开小型 Command 规划适配入口 |
| `Source/UE5DroneControl/UI/SequenceDispatchPanelWidget.cpp` | 复用原规划状态机，确认前校验航点与原弹窗配置 |
| `Source/UE5DroneControl/UI/GeographicTargetPanelWidget.h` | 公开展开原坐标输入面板的入口 |

Backend、Content、MediaMTX、VideoWindowManager/Widget 没有修改。测试产物、本机配置和模拟输入均在忽略的 `Saved/CommandQA` / `Backend/build-command` 内，不提交。

## 开发阶段历史编译与测试证据（2026-09-11，最终状态见上文）

| 项目 | 当前结果 |
|---|---|
| UE 5.8 Editor C++ / UHT / Link | 已通过完整编译及最新布局增量编译，`Result: Succeeded`；最终增量无 C++ warning/error |
| Backend 原源码构建 | 已通过 MSVC Release 构建，未修改 Backend |
| Backend 原单元测试 | 44 项，42 通过、2 失败；未调低断言或修改测试 |
| Backend HTTP | 本机真实 Backend 的 `GET /api/drones` 返回 3 架 QA 无人机，online、有效 GPS、动态电量；输入为 loopback UDP 模拟遥测 |
| 告警 Store 自动化 | 第二轮 `DroneOps.Command.AlertStore`：Success，0 errors / 0 warnings；上限、处理、移除、ID 不复用断言通过 |
| CesiumWorld 集成自动化总结果 | 第二轮 `DroneOps.Command.CesiumWorld`：Fail，2 errors / 142 warnings；两个 error 均为 Cesium 瓦片影像请求 ConnectionError，伴随 libcurl SSL connect error 35；没有把这些错误加入忽略名单 |
| UE CesiumWorld 启动 / 地图 Actor | PIE 成功启动；原镜像机和影子机出现，Shell 成功创建/销毁/重建。地理坐标接口可用，但地图影像加载不完整，不能标为地图完整通过 |
| HTTP → UE Registry / WS / 遥测闭环 | 第二轮 HTTP 注册名称、WS Connected、在线有效 GPS 遥测断言均未失败；Backend 日志确认真实 WS 会话 |
| 左/中选择与 Focus | 列表真实按钮事件 → Registry、地图适配选择 → 同一 Primary/Multi、原 Actor 存在/Focus 断言均未失败；没有模拟物理鼠标点击射线选取。新增的列表文字反向验证留待第三轮 |
| 航点、确认、本地 Playback、取消 | 第二轮添加地理航点、原确认弹窗、本地 DronePathActor 播放、取消清理断言均未失败；日志明确记录 shadow playback started。没有向真实无人机派发任务 |
| 真实 WS 告警 → Store → 选择 | 第二轮真实 Backend 低电量告警入 Store 断言未失败；独立测试夹具告警选择与 Shell 重建保留记录的断言未失败 |
| 原视频窗口 | 原 Blueprint 成功加载；打开 about:blank、重复打开去重、重复关闭断言均未失败。无真实 RTSP/WebRTC 媒体播放证据 |
| 1920×1080 界面 | 第二轮独立 PIE 使用 1920×1080 视口设置，截得含边框 1928×1130 窗口并人工检查；发现标题被旧视频入口遮挡、按钮文字换行、列表字号偏大。已修正并编译，但第三轮运行复核被拦截 |

Backend 两个失败为 `SegmentDistanceTest.Crossing`（距离返回 5，期望 0）和 `AssemblyPlannerTest.ConflictHeightSeparation`（未识别交叉冲突/未分层）。相关 Backend 源码与基线相同，属于本轮发现的既有问题；本任务未扩展修改 Backend。

第二轮 UE 实际发现并运行 2 项测试：1 Success、1 Fail、0 NotRun，测试总时长 19.308 秒。Command 断言未失败与整项集成测试失败是两个不同结论。

第一轮保留失败证据：项目代理配置是 `127.0.0.1:7890`，本机该端口没有监听，HTTP/WS/地图请求未正常启动；本地预演测试同时误查了 `DronePlaybackManager`，而原 Sequence 流程实际驱动的是 `DronePathActor`。修正测试对象后，第二轮使用启动参数 `-httpproxy=127.0.0.1:7897`，该端口现场确认为本机已运行的 `verge-mihomo.exe`。没有修改系统代理、项目代理配置、Backend 协议或 NetworkManager。

UE 5.8 引擎源码显示 libwebsockets 现在直接读取配置/启动参数代理；原项目仅临时清除 FHttpModule 代理的方式不再足以保证该上下文绕过代理。本轮通过正确的本机代理连通，**未声称修复代理策略本身**。后续应明确部署环境的代理设置，避免依赖开发者固定端口。

启动时还观察到 Cesium 插件 `FCesiumMetadataPropertyStatisticValue::Semantic` 未初始化的日志，以及 SimpleWebSocket 插件标记为 UE 5.7、在 unattended 启动中被跳过的兼容性警告。本轮实际 Drone 网络使用原 UE WebSockets 客户端并成功连接；未修改第三方插件以消除警告。

第三轮原计划验证 `ClientRole=Command`、布局修正、视口精确尺寸和新增反向列表断言，但启动命令在用户已授权后再次被自动审批以 `blocked by policy` 拦截，未提供具体原因。没有将该轮记为执行或通过。

本机编译使用 ASCII 路径联接 `C:\Users\wy331\Documents\UE5DroneControl-command` 指向本仓库，避开 MSVC 中文路径问题。该路径不是新仓库或新工作树。将并行数限制为 2，避免本机 PCH 编译内存不足；未更改系统配置。

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' UE5DroneControlEditor Win64 Development '-Project=C:\Users\wy331\Documents\UE5DroneControl-command\UE5DroneControl.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

待运行的 UE 测试名称为 `DroneOps.Command.AlertStore` 和 `DroneOps.Command.CesiumWorld`。后者默认使用隔离的 Registry 测试夹具；只有显式传入 `-CommandBackendQA` 才要求本机真实 Backend 的 HTTP/WS/UDP 输入证据。不能将模拟夹具断言当作真机验证。

历史证据位置：`Saved/CommandQA/backend-tests.xml`、`backend-tests.log`、`http-drones.json`、`backend.stdout.log`、`run1-results.json`、`Automation2/index.json`、`automation2.log`、`build-layout-final.log`。`command-shell.png` 已由本轮最新截图更新，最终固定副本在 `FinalSmoke/command-shell.png`。完整日志可能包含第三方请求 URL，不上传或复制到仓库文档。

## 开发阶段遗留事项与范围边界（按最终验收记录判定是否关闭）

1. 完成第三轮布局、反向列表文字断言、`ClientRole=Command` 的运行复核，以及物理地图点击/高亮与真实任务派发的人工验收。
2. 处理 Cesium 瓦片影像 SSL/连接错误并完成地图影像验收；确认 UE 5.8 第三方插件兼容性。
3. 完成真实媒体播放回归；已通过的窗口生命周期断言不构成 RTSP/WebRTC 播放验收。
4. Backend 两个既有几何测试失败另行处理，不将本次全链路回归标为通过。
5. 多显示器窗口宿主、Video Client、跨进程/跨主机选择同步、operator_selection、AI 均未实现；不能以预留枚举/事件代替实现。
6. 告警位置协议、跨会话持久化告警历史尚未接入。

Git 提交仅落在 `feat/triple-screen-command`；最终 SHA、提交列表、upstream 和工作区状态以本轮交付回复中的实际 Git 输出为准。
