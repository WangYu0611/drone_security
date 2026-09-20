# TASK-A2：Command UI Cleanup + Cesium 2D / 3D（UE 5.8）

**收尾验收结论（2026-09-12）：CONDITIONAL PASS。** TASK-A2 的默认 2D、切换及业务状态保持、路径显示、物理点击航点、确认/本地 Playback 和工具抽屉收尾通过。原 CesiumWorld Smoke 的 1082 vs 1080 断言仍为 Fail；3D Terrain 遮挡低高度路径明确 Deferred，真实 Backend 链路尚未验证。详见文末“收尾验收”。该结论不是全套自动测试零失败或真实飞控端到端认证。

## TASK-A2 PRE-IMPLEMENTATION AUDIT

基线：`7754811023519c0115da847f47b163a124241c84`，TASK-A 已合入 `feat/triple-screen`。工作区干净、fetch 后本地与远端一致，从集成分支创建并推送 `feat/triple-screen-command-a2`。本任务不自动合并。

### UI 分类与证据

| 分类 | UI / 类 | 依据与处理范围 |
|---|---|---|
| KEEP | CommandScreenManager / CommandShell、DroneList / DroneListItem、CommandAlarmPanel、Selected Info、Task | 当前正式 Command 入口；Registry 是唯一业务数据/选择来源 |
| KEEP | PreviewConfirmPopup、AssemblyPopup、Toast、PathFileListItem、PathDroneMatchItem、DroneNameEditPopup | 路径确认、匹配、反馈、名称编辑仍有运行时依赖；不是无用窗口 |
| KEEP | 框选 NewWidgetBlueprint | BP_DroneOpsPlayerController 的 BoxSelectWidgetClass 引用，控制器驱动框选显示 |
| KEEP | DroneVideoWindowManager / DroneVideoWindowWidget | 视频窗口及关闭委托生命周期保持；只调整 Command 的入口，不开发 TASK-B |
| FUNCTION KEEP / OLD UI REMOVE | SequenceDispatchPanel | UIManager 已转发到 Shell，但旧面板仍包含独立规划/停止/序列按钮，NativeTick 还会把自身改成 Visible；保留状态机、确认、保存、Playback、序列派发，通过 Shell 按需展示 |
| FUNCTION KEEP / OLD UI REMOVE | GeographicTargetPanel | 已嵌入 Shell，但坐标旧图标仍自动显示；保留坐标输入和批量/单点派发，改由 Command 入口打开 |
| FUNCTION KEEP / OLD UI REMOVE | WBP_DroneOpsHUD、DroneInfoPanel、DefaultSegmentSpeedWidget | BeginPlay 仍创建旧 HUD（视频选择按钮）、速度顶层控件，中键可弹旧详情；Command 收口，非 Command 保持兼容 |
| KEEP（其他上下文） | EnemyDroneList / EnemyDroneRow、GroundProjectionSettings、MainMenu、RuntimeHUD、DronePathPlayback、VideoStreamSelector、模板 UI | 有 C++ / Blueprint / Level / 配置依赖；不按名称猜测无用，不删除业务入口 |
| SAFE TO DELETE | 无 | 尚无候选同时满足 C++、Blueprint、Level、软引用、配置、测试、运行时创建全部无引用；本次不删除资产 |

搜索覆盖 CreateWidget、AddToViewport、SetVisibility、WBP_、WidgetClass、LoadClass、ConstructorHelpers；重点读取 Controller BeginPlay / 输入、UIManager、Command manager/shell、列表、告警、Sequence、Geographic、视频类。UE 5.8 Python AssetRegistry 读取 `/Game` 的软/硬/管理依赖及 referencers，并实际加载 CesiumWorld；结果在忽略目录 `Saved/CommandQA/a2-audit.json`。资产依赖扫描不等于证明动态字符串引用全部不存在，因此无删除结论。

GameMode 在 PreInitializeComponents 加载 BP_DroneOpsPlayerController。该 BP 默认 HUD 指向 WBP_DroneOpsHUD，框选指向 NewWidgetBlueprint；HUD 还依赖 WBP_DroneInfoPanel / WBP_VideoStreamSelector。CesiumWorld 包依赖含 InputCore、CesiumRuntime 和项目模块，没有 Widget 资产依赖；其他 Level 仍引用 RuntimeHUD / DroneOpsHUD。不修改 Blueprint / Level 资产来切断兼容引用。

### Cesium / Basemap 实际审计

| Actor / Component | 当前资产设置 | 2D / 3D 规划 |
|---|---|---|
| CesiumGeoreference_0 | 原点 lon 116.347、lat 39.981、height 43；DEFAULT_GEOREFERENCE | 共享，不重建，不改变坐标系 |
| CesiumCameraManager / CreditSystem / SunSky | 已存在 | 共享，保留归属信息 |
| Cesium3DTileset_2 | World Terrain，Ion asset 1；挂载 Bing Maps Aerial / CesiumIonRasterOverlay，Ion asset 2 | 保留同一 raster 载体；2D 使用 Cesium ellipsoid，3D 恢复原 terrain source |
| Cesium3DTileset_0 | Google Photorealistic 3D Tiles，Ion asset 2275207 | 重型 3D 专用层，2D 禁用 |
| Cesium3DTileset_1 | OSM Buildings，Ion asset 96188 | 3D 专用层，2D 禁用 |
| Cesium3DTileset_5 | FromUrl，URL 为空 | 未配置数据源，不凭空填充或制造数据 |
| CesiumCoordinateService | GameMode 初始化，Registry 共用 | Drone / Route / Waypoint 共用，不随模式切换 |

**不能报告 No additional 3D dataset configured**：项目已配置外部 Google、OSM、Terrain 数据。用户自有正式城市/ODM 数据未接入，不为测试生产三维城市。

当前 `DefaultEngine.ini [CesiumTileServer] UseLocalTileServer=false`：实际在线 raster 是 **CesiumIonRasterOverlay / Ion imagery asset 2**，不是从一条显式 XYZ/TMS/WMTS/WMS URL 加载；endpoint 由 Ion 解析。不可把它猜成 XYZ。敏感 token 不写入报告。

另有已配置但关闭的奥维 205 路径：`[CesiumTileServer] RasterTemplateUrl`，URL 模板 `getomap_205_$z_$x_$y_0_0.jpg`，代码恢复 `{z}/{x}/{y}`，HTTP、XYZ 索引、WebMercator、GCJ02，512 px，level 9–18。该模式使用已有 GCJ02→WGS84 校正的 ProceduralMesh 栅格平面；CreateUrlTemplateRasterOverlay=false，不能绕过坐标修正强行换成普通 Cesium overlay。本任务保留该配置/流程，不复制离线瓦片系统。

预审计加载发生外部连接失败（当前已有 HTTP 代理端口拒绝连接），属于 **ENVIRONMENT / EXTERNAL CONNECTIVITY ISSUE**；不关闭 TLS、不屏蔽日志或原始断言。

## 实施与验证

### UI 收口

- Command BeginPlay 不再创建旧 WBP_DroneOpsHUD、自动顶层航段速度控件；中键不再弹旧详情。详情由 Shell 展示，速度通过任务工具按需打开。Standalone 保留历史入口。
- SequenceDispatchPanel 保留内部状态机、保存、确认、预演和派发；默认 Collapsed，NativeTick 不再自行恢复显示。Shell 的任务工具调用原能力，序列文件面板按需显示在 Shell 内。
- GeographicTargetPanel 保留原坐标输入、转换和派发，隐藏旧自动图标，缩小为 Shell 内按需显示的面板。同一坐标入口可打开/收起；坐标、序列与速度面板在 Command 下互斥展示。修正 NativeTick 覆盖 Command 尺寸的问题，保留 480×420 与可见滚动条；不触碰路径编辑状态。
- DroneList / Alarm / Situation / Task 仍由 Registry / AlertStore 驱动。列表隔离状态提示缩小并换行；没有制造生产遥测或电量。
- 地图右上角增加紧凑 2D / 3D / 定位选中 / 任务工具；任务动作默认折叠。选中信息卡固定合理宽度，透明面板根使用 SelfHitTestInvisible；地图命中检测只排除可见卡片和控件。
- 没有删除 C++ 类、Widget Blueprint、Level 或其他 Content 资产。Network、Registry、坐标转换、PathActor、WaypointActor、Playback、视频窗口底层实现均保留。TASK-B 未开始。

### MapMode 所有权与启动

`ECommandMapMode::{Map2D, Map3D}` 和唯一 `CurrentMapMode` 位于已有 `UCommandMapInteractionService`。模块在 Command CesiumWorld 的 OnPostWorldInitialization 准备同一世界生命周期服务，早于 uncooked Cesium 构造脚本；GameMode 的 PreInitializeComponents 通过 GetOrCreateForWorld 复用它，CommandScreenManager 绑定同一服务及 Controller。服务在 World.ExtraReferencedObjects 中受到 GC 引用保护，随 World 释放；模块卸载解除世界委托。不新增业务 Manager。UI 只调用 SetMapMode，OnMapModeChanged 驱动按钮高亮。Enter2DMode / Enter3DMode 是同一接口的便利方法。

两种模式使用同一 CesiumWorld、Registry、CoordinateService、Drone / Path / Waypoint Actor。切换不 OpenLevel、不清空 Registry、不重新连接 Backend、不重启 Playback。Command 专用 CameraActor 的焦点和距离保持；2D pitch 为 -89.9°、限制旋转，3D 恢复透视 pitch / yaw。中键拖动平移、滚轮缩放、3D 右键拖动旋转；定位选中与告警定位走原 DroneId。FocusGeographicLocation 使用已有 CoordinateService；没有给当前告警虚构 GPS 字段。

远距离 GIS 缩放时，Controller 的 HitResultTraceDistance 至少覆盖相机焦距的四倍，避免 UE 默认 1 km 射线在远景下无法命中地表；服务解除绑定时恢复原值。Command 启动和点击选择的旧屏幕调试文字停止显示，UE_LOG、操作失败反馈和 Cesium 原始错误仍保留。

实际 2D 点击已写入原 PathActor，但原 0.25 cm 线宽与只按 yaw 朝向相机的文字不适合 GIS 俯视。因此只增加显示适配：Map Service 根据视距/视口更新原 spline mesh 半径；WaypointActor 在 Command 相机下调整原 mesh 尺寸及文字尺寸、偏移和完整朝向。没有复制 Path/Waypoint，没有调用 RefreshPath 重算业务状态，没有移动航点 Actor、修改经纬度/速度/编号或重启 Playback。退出 Command 显示时恢复原表现参数；Standalone 保留原表现。

### 配置、图层和未来接入

`Config/DefaultGame.ini [CommandMap]`：DefaultMapMode=2D，BasemapIonAssetId=2，BasemapUrl 留空以使用实际 Ion imagery。可选 BasemapUrl 仅接受 HTTP(S) WGS84/WebMercator XYZ 模板（$z/$x/$y 或大括号形式）；无效配置记录 COMMAND FUNCTION ERROR 并保留原 raster。私有 URL / token 应放本机配置覆盖，本次没有提交凭据或代理配置。

2D 将 raster 载体源切为 Cesium FromEllipsoid，保持 imagery 激活；Google / OSM 层隐藏、禁碰撞、SuspendUpdate，且源临时改为非网络 ellipsoid，避免仅 SuspendUpdate 仍触发 Cesium BeginPlay 加载重型源。3D 恢复各层原 source、hidden、suspended、collision 设置。没有删除 raster 所依赖的载体。

现有奥维平面开启时，由原 GameMode 流程创建经过 GCJ02 修正的平面；模式服务保持 Cesium online raster / layers 停用，不用普通 WGS84 overlay 替代该流程。本次未启动离线瓦片服务器，此兼容路径只有代码检查、没有新的运行认证。

未来 Cesium Actor 可标记 Command3DOnly、CommandBasemapSurface 或 CommandSharedLayer。服务初始化扫描配置好的 Actor；新增图层应在世界启动前配置，运行中动态添加层不属于本次实现。Drone / Route / Waypoint 不属于被切换的 Cesium 层。

`[CommandMap]` 日志记录各层模式、source、suspended、hidden 和 raster activation。Cesium 失败保留原日志，并附加 BASEMAP CONNECTIVITY ERROR；不会屏蔽自动测试 Error。

**启动请求修正**：早期实际 `-game` 日志显示，UE 在 GameMode PreInitializeComponents 前重跑 Cesium OnConstruction，出现 Google / OSM / Terrain 的 `Loading tileset for asset ID`。因此准备时机已提前到 OnPostWorldInitialization，并保持同一服务。修正后的 a2-physical-final.log 在默认 2D、首次切换 3D 之前，asset 1 / 96188 / 2275207 的初始化加载记录为 0，ellipsoid 与 active raster 初始化正常。本次没有抓取逐请求字节量，不能以 Actor 状态冒充完整网络抓包。编辑器编辑世界资产预览不受 Command 游戏世界策略控制；其他地图仍有 GameMode 回退初始化，但最早构造前准备目前仅针对 CesiumWorld。

### 验证方法及保留证据

日期：2026-09-12，UE 5.8 / UE5DroneControlEditor / Win64 / Development。日志、UE 导出报告、QA 截图均在忽略目录 Saved/CommandQA，没有加入 Git。

- R1：发现并执行 3 项，1 Success / 2 Fail。A2 测试夹具的经纬度次序错误与代理参数解析错误已修正；原错误报告保留在 A2/Automation。没有删除原 Smoke 的精确高度断言。
- R2：A2MapMode 和 AlertStore 各出现 Test Completed Success。随后同一编辑器进程启动下一次 PIE 时，在未修改的 DroneWebSocketClient::ScheduleReconnect 崩溃；整轮不能标为 PASS，报告未完整导出。崩溃目录 Saved/Crashes/UECC-Windows-90EB2AFD4B587B4C8CE1B2B6EAFE0408_0000，日志 a2-automation-r2.log。
- 独立进程初次复核：A2/FinalA2 导出 1 Success（58 warnings）/ 0 Fail / 0 NotRun；但截图复核发现栅格未激活。原因是 PreInitializeComponents 捕获 IsActive 时组件尚未自动激活；修正为同时保留 bAutoActivate 意图，并增加默认在线 raster activation 断言。该次逻辑成功不代表底图视觉成功。
- 同期独立原 Smoke（A2/FinalSmoke）：CesiumWorld 完成，唯一测试错误为 height 1082 vs 1080；原断言和 Fail 状态保留。最终代码须再次复核。

新增 A2MapMode 使用明确标记的、只存在于独立 PIE 的 9011 测试夹具，不将它写入生产数据。测试经过原 Path Editor → 三航点 → Confirm → 原 LocalPreview，跨 tick 检查 2D→3D→2D 的同一 Registry、Drone Actor、Primary Selection、CoordinateService、PathId、航点数量/顺序/位置/速度、Mission、Alarm，以及同一仍在移动的 Playback Actor。工具栏 OnClicked 事件测试不冒充物理鼠标；告警委托注入测试不冒充真实 Backend WS。

### 已知边界

1. QA Backend / 遥测输入启动被自动审批审查以 blocked by policy 拒绝，未给出更具体原因；没有改用其他工具或包装命令绕过。本次真实 HTTP/WS/UDP、真实 Backend 告警端到端复核未执行。UE 自身可运行，不属于 UE 启动被 policy 阻断。
2. 连续 PIE 的断线回调崩溃落在与基线相同的 Network 代码；尚未在原基线独立复现，因此只能报告“未修改 Network 生命周期代码中的已观察失败”，不能宣称已证明 PRE-EXISTING。本次未重构 Network，集成前需人工评估该边界。
3. TASK-A 的两个 Backend 几何测试历史失败（SegmentDistanceTest.Crossing、AssemblyPlannerTest.ConflictHeightSeparation）继续分类 PRE-EXISTING FAILURE，历史结果 42/44；本次不重跑或修改 Backend 算法。真实 MediaMTX 流未重验；视频窗口管理/关闭实现与基线无差异。
4. 1920×1082 与请求 1920×1080 的差异保持 VALIDATION / CAPTURE GEOMETRY MISMATCH。未裁切截图或放宽原断言；并非 A2 全屏 DPI 精确认证。
5. Cesium 外部连接失败按 ENVIRONMENT / EXTERNAL CONNECTIVITY ISSUE 记录；连接成功的一次截图不保证所有在线图层长期可用。未接入自有正式 3D 城市/ODM 数据。
6. 插件启动日志包含 Cesium Metadata Semantic 未初始化错误；不是新项目编译错误，但原日志保留。自动测试期间的结果与启动日志分别报告。
7. **3D PATH VISUAL OCCLUSION / KNOWN ISSUE / DEFERRED**：物理点击创建的低高度路径在 2D ellipsoid 可见；切到 3D 的 terrain / photogrammetry 完成加载后，路径会被地形遮挡，返回 2D 后完整恢复。这不是连接失败，也不是已经证明的历史失败，不能计为 3D 路径视觉 PASS。按 2026-09-12 收尾指令，本项不在 TASK-A2 扩展实现，待正式 3D 地图和高度语义确定后处理；保留真实航点高度与世界深度，不通过抬高业务坐标或改变深度测试掩盖问题。

### 上轮结果（收尾前历史证据）

| 项目 | 最终证据与结果 |
|---|---|
| UE 5.8 编译 | PASS：a2-build-startup.log，UHT / C++ / Link 成功，14 个构建动作，92.16 秒；含最终初始化和拾取修正 |
| A2MapMode | PASS：A2/FinalStartupA2/index.json，发现/执行 1，Success 1（59 warnings），Fail 0，NotRun 0，14.66 秒；明确 Test Completed Success，含远景拾取距离断言 |
| 2D→3D→2D / Selection / Mission / Alarm / Path / Waypoint | 上述独立 PIE 夹具断言通过；同一业务对象和航点值保持；不代表真实 Backend 复核 |
| Playback | 原 Confirm→LocalPreview 在切换两次及跨 tick 后，同一 Playback Path Actor 仍有效且移动；没有重启播放 |
| AlertStore | R2 单元测试 Test Completed Success；真实 WS 告警本次未重验 |
| 原 Command.CesiumWorld | A2/FinalStartupSmoke/index.json，发现/执行 1，Fail 1（1 error / 72 warnings），NotRun 0，15.14 秒；唯一测试错误 height=1082、expected=1080，其他 Selection / Path / 视频生命周期断言无失败；不能把该测试总状态写为 PASS |
| Basemap | 修正后日志 raster active=1，2D截图显示在线 imagery；截图加载阶段仍见局部白色区域，实际窗口继续复核 |
| 2D layer 状态 | Google / OSM source=2(ellipsoid)、suspended=1、hidden=1；共享 carrier source=2、suspended=0、hidden=0。3D恢复 source=0(Ion)、原显隐/更新；编辑世界初始化加载与游戏模式状态分开记录 |

### 物理窗口复核

通过 Windows 原生鼠标输入控制 UE 游戏窗口，使用已有缓存的 3 架 QA UAV；当前 Backend 未运行，界面正确显示失联、BAT / GPS / Task N/A。没有为物理验收新增生产数据。

- 默认 2D，实际在线影像加载；启动没有旧 HUD 视频入口、重复 DroneList 或自动弹出的 Sequence / Geographic 顶层面板。任务工具可展开/折叠，动作文字未出现明显异常换行。默认信息卡与紧凑工具栏遮挡面积明显小于中屏的 25%；此为截图几何估算，未进行逐像素遮挡测量。
- 左侧选择 UAV-3：列表主选中、中心详情及相机定位更新。地图物理点击 UAV-2：主选中变为 d2，左侧按钮显示主选中，日志记录 HandleDroneClick。原多选语义仍保留，其他已多选行可以继续高亮。
- 第二个实际窗口中，物理点击 3D→选择 UAV-3→2D，模式高亮、相机视角、d3 主选择保持。日志 a2-physical-ui-r2.log 对应 Mode=3D/2D。地图滚轮产生可见缩放。
- 第一次窗口部分在屏幕外，工具输入坐标有偏差；最大化后物理模式按钮正常，不据此宣称产品按钮失败。原 1920×1082 捕获证据仍保留；最大化窗口的复核不是精确 1920×1080 DPI 认证。
- 三架数据在当前列表容器内没有足够实际滚动距离；继续记录 NOT FULLY VERIFIABLE WITH CURRENT 3-DRONE DATASET，不用自动测试夹具的第四行冒充正式物理滚动 PASS。

最新物理路径验收（a2-physical-route-visual.log，代码 95b4ada）：选择 UAV-3 → 开始规划 → 三次真实地图点击 → 原 Confirm 弹窗 → 本地预演。三处航点和连线在 2D 可见；日志 Local preview summary: started=1、skippedNoShadow=0、skippedInsufficientWaypoints=0。预演过程中点击 3D，仍可看到预演运动及同一三点连线；随后真实地形加载导致遮挡。返回 2D 后同一路径、三个放置点与 d3 主选择恢复可见。自动测试覆盖持续跨 tick 播放，物理观察不替代所有时间连续性断言。

中键平移、右键轨道旋转、框选、拖动 UAV 列表 ScrollBar、多 DPI、真实 Backend 告警、完整离线地图仍需人工/环境复核；服务层 Pan / Zoom / Orbit 和业务状态保持已通过自动测试。当前不自动合并，TASK-A 的人工验收不自动继承为 A2 通过。

## 收尾验收（2026-09-12）

### 工作区与最终编译

恢复时实际分支为 `feat/triple-screen-command-a2`，HEAD `95b4ada`，相对其远端 ahead 6；七个未提交文件与中断前清单一致，`git diff --check` 通过。构建使用的 `C:/Users/wy331/Documents/UE5DroneControl-command` 是指向本工作区的 Junction，并非另一份源码。`a2-build-drawers.log` 已完整结束：**Result: Succeeded，14/14 actions，81.56 秒**；DLL 时间为 15:08:44，包含 15:06–15:07 的抽屉和测试修改。本轮没有再修改 C++，无需重复编译。

本次收尾的功能变更仅为原有七文件修改：Geographic 同入口开合；Command 下 Geographic / Sequence / 航段速度工具互斥；Geographic 采用固定 480×420 Slate 布局尺寸、显示滚动条，旧 RefreshAvailability / NativeTick 不再覆盖为 600 宽度。保留原路径编辑和 Playback 对象。默认 2D、图层启动策略、切换、拾取和路径显示沿用此前已提交的 A2 实现。

### Automated PASS（逐项报告，不覆盖原 Smoke Fail）

以下均运行 UE 5.8 Editor / Win64 / Development，`ClientRole=Command`，请求视口 1920×1080，沿用本机已有代理参数；未开启 `CommandBackendQA`。A2MapMode 单独进程；AlertStore 与 CesiumWorld 在另一个进程中执行，后者只有一次 PIE。

| 测试 | 发现 / 执行 | 结果 | 报告及日志 |
|---|---|---|---|
| DroneOps.Command.A2MapMode | 1 / 1 | **PASS**：Success 1，Error 0，Warning 53，NotRun 0；15.72 秒，明确 Test Completed Success | `Saved/CommandQA/A2/DrawersFinalA2/index.json`；`a2-drawers-final-a2.log` |
| DroneOps.Command.AlertStore | 1 / 1 | **PASS**：Success 1，Error 0，Warning 0 | `Saved/CommandQA/A2/DrawersFinalSmoke/index.json`；`a2-drawers-final-smoke.log` |
| DroneOps.Command.CesiumWorld | 1 / 1 | **FAIL 保留**：Error 1，Warning 65；唯一错误是 viewport height expected 1080 / actual 1082；业务同步检查 `new errors=0` | 同上；AlertStore + CesiumWorld 合计 16.09 秒，NotRun 0 |

总计发现并执行 **3 项：2 Success / 1 Fail / 0 NotRun**。没有用进程退出码替代测试结果，也没有放宽或删除高度断言。

A2MapMode 覆盖默认 2D、在线 raster activation、2D→3D→2D、远景拾取距离、原三航点路线和确认、持续跨 Tick 的同一 Playback Actor、路径渲染段宽度、航点文字完整朝向，以及 Registry / CoordinateService / Drone / Primary Selection / Mission / Alarm / PathId / 航点顺序位置速度保持。新增抽屉断言覆盖唯一 Geographic 实例、480×420 尺寸经过 NativeTick 后保持、同入口收起和隐藏 Sequence。CesiumWorld 中 Selection、原路径/确认/Playback、视频窗口去重及关闭、Shell 销毁重建等业务检查无新错误。告警委托与 PIE 夹具不代表真实 Backend。

### Physical mouse/window verification PASS（限定本轮已观察范围）

新启动实际 `CesiumWorld -game -ClientRole=Command` 窗口，加载最终 DLL，使用既有三架 QA 缓存数据；窗口最大化后用 Windows 原生鼠标输入逐步操作并观察截图。未增加生产数据、未启动 Backend 或遥测发送器；左侧在线 0、失联、BAT/GPS/Task N/A 如实显示。此为代理执行的真实窗口/鼠标验证，不是用户人工签字，也不以按钮委托广播冒充物理操作。

证据：`Saved/CommandQA/a2-drawers-final-physical.log`；未裁切窗口截图 `Saved/CommandQA/A2/DrawersPhysical/01-*.png` 至 `12-*.png`。最大化窗口截图为 1638×945，属于本机窗口捕获尺度，不替代自动 Smoke 的 1920×1082 原始视口证据。

| 检查 | 实际结果 |
|---|---|
| 默认 2D / 启动图层 | **PASS**：2D 按钮高亮、俯视在线影像；08:24:56 UTC 启动层为 ellipsoid，raster active=1。08:28:50 UTC 首次点击 3D 之前，Google 2275207 / OSM 96188 / Terrain 1 的 `Loading tileset for asset ID` 为 0；三者只在该次切换后初始化。属于日志级初始化验证，未做网络逐包审计。 |
| UAV 选择与高亮 | **PASS**：鼠标点击 UAV-3 列表，主选中高亮、中心详情 d3、定位更新，后续操作与切换保持 d3。 |
| 物理地图航点 / 2D 显示 | **PASS**：开始规划后在地图三个不同位置真实点击，出现起点及三个新增航点、连接线和 D3 标签；俯视下可辨认。截图 `01-2d-three-clicks.png`。 |
| Geographic | **PASS**：坐标入口打开并再次点击收起；滚轮可滚动内容；多次重开、滚动和模式切换后外框不扩张，480×420 的精确 Slate 尺寸由自动断言验证。截图 `02`、`04`、`11`。 |
| Sequence 与互斥 | **PASS**：打开序列文件面板、同入口收起；Geographic 打开会关闭 Sequence，反向亦然，未同时遮挡。Sequence 当前为真实空文件列表，未将此项扩展为序列文件派发验证。截图 `03`–`06`。 |
| 确认 / 本地 Playback | **PASS**：原确认弹窗→物理点击“本地预演”；08:28:38 UTC 日志 `started=1, skippedNoShadow=0, skippedInsufficientWaypoints=0`；观察到 UAV 沿路线运动。截图 `07`–`09`。 |
| 2D→3D→2D 状态保持 | **PASS**：08:28:50 / 08:29:12 UTC 切换；d3 主选中、同一路线和工具面板状态保持，返回 2D 恢复完整路线显示，Playback 继续进入下一段，未再次点击预演。截图 `09`、`11`、`12`。3D 地形遮挡本身不计视觉 PASS。 |

### Known Issues / Deferred

1. **3D Terrain 遮挡低高度路径：DEFERRED。** 本轮再次观察，保留截图 `10-3d-terrain-deferred.png`。待正式 3D 地图和高度语义确定后处理；本次未修改高度、深度测试或扩展地图实现。
2. **1082 vs 1080：VALIDATION / CAPTURE GEOMETRY MISMATCH。** 原 Smoke 维持 Fail，`CommandClientTest.cpp` 未修改；不为该差异改业务代码，不宣称精确全屏/DPI 认证。
3. 上轮连续 PIE 的 `DroneWebSocketClient::ScheduleReconnect` 崩溃仍是已观察、未定位的问题；本轮独立 A2 进程及单次 PIE Smoke 未复现，不据此宣布已修复或已证明为基线旧问题。
4. Cesium Metadata Semantic 启动 Error、外部图层连接/配置告警保留于日志；本轮自动化 Warning 数如上，不宣称零告警或所有在线源可用。
5. 既有三架 UAV 数据不足以完成列表长距离滚动验证；多 DPI、完整离线地图、真实视频流以及历史 Backend 算法问题的边界保持，不纳入本轮通过范围。

### 尚未验证的真实 Backend 链路

本轮**未验证**真实 HTTP 注册表、WS 连接和告警、UDP→Backend→UE 遥测、真实任务派发/暂停/恢复及执行回报。此前 Backend / 遥测 QA 启动遭自动审批拒绝的历史记录保留；本轮只收尾客户端与本地 Playback，没有重试或绕过该动作。缓存 QA 列表、测试夹具、LocalPreview 和委托注入均不能证明上述链路通过。

### 明确结论与分支交付

**TASK-A2 = CONDITIONAL PASS。** 客户端范围内的 Automated PASS 与 Physical mouse/window verification PASS 成立，条件是保留上述 Smoke Fail、Deferred 和真实 Backend 未验证边界；不是正式 3D 路径可见性或真实 Backend 的完整验收。

建议在接受这些明确边界的前提下合入 `feat/triple-screen`，后续正式 Backend / 3D 联调另行验收。本任务不执行该合并，也不修改 main。根据收尾指令，验收提交先保留于现有 A2 分支，再将用户指定的 `feat/triple-screen-command` **快进**到同一验收提交并 push；不强推、不重写历史。恢复时该 Command 分支本地和远端均为 `6120aaf`，是 A2 HEAD 的祖先。
