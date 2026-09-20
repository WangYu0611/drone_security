# TASK-V1 — Video Client Audit

审计日期：2026-09-14。阶段：先只读审计，再迁移；不进入 TASK-A4。

## Git 基线与策略

- 初始工作区干净，当前分支 `feat/triple-screen-command-a3`，HEAD `50c631de9a55f9d1cd25e9f6520d11cf6fa888ec`。
- `git fetch --all --prune` 成功。集成分支 origin/feat/triple-screen=`7754811`；Video=`f6ed7f9`；A3=`50c631d`。本地对应三分支与 remote 一致。
- Video 是集成分支祖先。`git log integration..video` 为空；没有 Video 独有提交。集成与旧 Video 的所有 Video C++/Content 资产无差异。
- `git diff integration video` 是旧快照相对集成的反向差异：29 文件，15 插入、1540 删除；不是 Video 主动删除功能。完整 name-status 见 QA/TASK-V1/git-diff.txt。
- 集成→A3：53 文件，1626 插入、81 删除，包含 A2/A3 startup、Command UI 和已验收文档。不能用旧 Video 快照覆盖。
- 推荐 **Strategy B**：从最新 origin/feat/triple-screen 建立 feat/triple-screen-video-v1，再 fast-forward 到冻结 A3 提交作为已验收依赖。A3 已包含集成祖先，快进无需重新实现/冲突解决。仅移动 V1；不移动 A3、integration、main。旧 Video 代码已在基线内，无需 cherry-pick。

## 必须回答的问题

| 问题 | 审计结论 |
|---|---|
| Video UI 入口 | 旧 WBP_DroneOpsHUD → WBP_VideoStreamSelector → WBP_VideoStreamRow；C++ OpenDroneVideoWindow(DroneId)。另有中键 DroneInfoPanel 视频入口。不是独立 role Shell。 |
| 谁创建 Widget | ADroneOpsPlayerController::EnsureVideoWindowManager 创建 UDroneVideoWindowManager；OpenVideoWindow 创建 WBP_DroneVideoWindow，再由 SWindow 承载。 |
| 依赖 Command UI | 播放 Widget 本身不调用 CommandShell，但旧选择器嵌在 HUD；InfoPanel、选机和键盘飞行控制耦合 Controller。独立启动不能仅跳过 CommandShell。 |
| 特定地图 | 播放器只需要 owning world/controller 和 URL，无地图名判断。旧入口跟随地图的 HUD/Controller；CesiumWorld 是默认项目地图，旧启动仍加载场景。 |
| 特定 GameMode | Shell 没有直接 GameMode 依赖；实际启动依赖 DroneOpsGameMode 提供 DroneOpsPlayerController；GameMode 配置 Registry/坐标/receiver。其他 GameMode 若没有该 Controller 不会进入现有启动链路。 |
| 特定 Controller | 是。窗口 manager 从 Controller 按需创建，BP class 由 Controller 指定，旧 Widget 还向 DroneOpsPlayerController 转发键盘控制。 |
| 实际媒体技术 | UWebBrowser/WebBrowserWidget（CEF）加载 MediaMTX 浏览器 WebRTC 页面。FFmpeg 可循环本地 MP4→RTSP→MediaMTX→WebRTC。不是 UE MediaPlayer/MediaTexture 链路。 |
| Video Source 来源 | Backend/data/drones.json 的 video_url，经 GET /api/drones、DroneNetworkManager 的 JSON 解析进入 FDroneDescriptor.VideoUrl，再由 Registry 查询。例 http://127.0.0.1:8889/drone-1。不能传 RTSP 或 WHEP endpoint 代替页面。 |
| UAV 和视频切换 | 旧 checkbox 以 DroneId 打开/关闭各自窗口；OpenDroneVideoWindow 根据对应 Descriptor 取 URL。改变 PrimarySelection 本身不会自动重绑已打开窗口；不存在单 Shell 跟随选机的完成实现。 |
| 多 UAV | 支持多 Descriptor；旧窗口一机一个、最多 4 个，友方选择器使用 GetFriendlyDroneDescriptors 过滤敌方。V1 范围为单视频区域切换多 UAV。 |
| 重复实例 | 旧 manager 内同 DroneId 去重、CloseAll 清理、UPROPERTY LiveWidgets 保活。InfoPanel 与独立窗口各有 Browser，允许同源并行，不能据此宣称全局只有一个流。NetworkManager 是 GameInstanceSubsystem，UI 不应另建网络连接。 |
| Blueprint-only 缺口 | Selector/Row 是 WidgetBlueprint 资产，没有对应 Selector/Row C++ 类；HUD 持有选择器。窗口有可调用的原生基类及 C++ 创建入口，非全部播放功能只在 BP 中。role 分支目前未调用任何 Video Shell；需要原生入口替代对 HUD 的依赖。 |
| Video role 还缺什么 | Manager 当前只打印 reserved 并 return；Controller 随后仍创建旧 HUD/列表/面板/速度 Widget。缺独立唯一 Shell、早期 legacy UI 隔离、Registry 选择投影、源变更处理、清理及物理播放/回归验证。 |

## 资产和代码清单

本次使用 UE 5.8 AssetRegistry commandlet 实际枚举 /Game、加载相关 BP generated class/defaults。结果见 QA/TASK-V1/assets.json；包含所有媒体/Material/RenderTarget 候选，不靠文件名推断资产类型。脚本完成并输出 TASK-V1 ASSET AUDIT COMPLETE，但 commandlet exit=1，来自已存在 AllToolsets GameFeatureData AssetManager 配置错误；不得写成 commandlet PASS。

- C++ UI：UI/DroneVideoWindowWidget.{h,cpp}、DroneVideoWindowManager.{h,cpp}、DroneInfoPanelWidget.{h,cpp}、DroneListItemWidget、UIManagerBlueprintLibrary。
- Blueprint/UMG：/Game/DroneOps/UI/WBP_DroneVideoWindow、WBP_DroneInfoPanel、WBP_DroneOpsHUD、WBP_VideoStreamSelector、WBP_VideoStreamRow。
- Controller：/Game/DroneOps/Blueprints/BP_DroneOpsPlayerController；CDO 已确认 HUD=WBP_DroneOpsHUD、video class=WBP_DroneVideoWindow、box select=NewWidgetBlueprint。
- Startup：Command/CommandScreenManager、DroneOps/Control/DroneOpsPlayerController、DroneOpsGameMode、UE5DroneControl.cpp world hook、Config/DefaultGame.ini、DefaultEngine.ini。
- Data/network：DroneOps/Core/DroneRegistrySubsystem、DroneOpsTypes.FDroneDescriptor.VideoUrl；DroneOps/Network/DroneNetworkManager、DroneHttpClient、DroneWebSocketClient；Backend/http/http_server、Backend/data/drones.json。
- Stream/decoder：WebBrowserWidget 插件负责浏览器渲染/解码；Docs/MediaMTX、机器上的 ignored tools/MediaMTX（exe、配置、demo scripts）；Jetson/jetson_video_stream.py 为外部推流准备，不属本次实链验收。
- MediaPlayer / MediaSource / MediaTexture / RenderTarget / Material：相关 Video Widget 的本次实际依赖是 UMG、SlateCore、UE5DroneControl、WebBrowserWidget；没有依赖项目内 UE 媒体纹理、渲染目标或视频材质。完整类清单见 assets.json，不能将项目其他材质视为视频依赖。
- Level：默认 /Game/Level/CesiumWorld；另有 MainMenu 等历史入口，实际依赖及引用见资产清单。此轮不重写/修改地图 Blueprint。

## 当前数据流和启动流程

Backend /api/drones → 既有 NetworkManager → Registry[DroneId].VideoUrl → Controller OpenDroneVideoWindow → 既有 VideoWindowManager → WBP_DroneVideoWindow.VideoBrowser → MediaMTX WebRTC 页面。

现有 DefaultClientRole=Command；显式 -ClientRole 优先。Module world hook / GameMode 使用同一 ResolveClientRole；Controller BeginPlay 创建 CommandScreenManager。Command 创建 CommandShell；Video 仅 reserved return，但旧兼容 UI 创建继续执行。Standalone 保留旧流程。

## 已发现问题与预计修改范围

1. Video role legacy UI 泄漏：在 Controller native BeginPlay 的 legacy 逻辑之前路由 Video，并绕开 BP HUD 创建依赖；保护 UI facade 防止 Blueprint 再打开旧面板。
2. 新增原生 VideoShell，单 UWebBrowser，Selector 使用既有 Registry，按 DroneId→VideoUrl 切换；不增加业务模型/Backend transport。
3. 复用现有 manager 的 role/Show/Hide/Destroy 生命周期；Command/Standalone 分支逻辑不重写。
4. 不把 OnUrlChanged 当成“视频实际播放/LIVE”证据。UI 应区分页面状态与媒体播放状态，未知 resolution/FPS/camera 不伪造。
5. 旧 WebBrowser 页面加载 watchdog 只判断导航；不能证明视频帧。V1 必须配合物理画面和可获得的媒体状态进行验收。
6. 自动化覆盖唯一 Shell、选机/源更新/删除、关闭重开/清理、role 回归；另做真实窗口操作和项目现有 Demo 来源播放验证。现有 demo script 的外部 MP4 路径仍需核实可用性。
7. A3 默认配置及 Command UI 不重写；如基础依赖需承接，完整快进保持原提交。

## Deferred

第二台 Video 主机、Jetson/PX4/实际飞行、多路高清压力、录制、AI、PTZ、Video Wall、最终部署、Map role、TASK-A4、跨进程 selection 广播。既有 1082 vs 1080 和低高度 3D 遮挡保留，不能修改业务坐标掩盖。Demo 播放不能称为真实 UAV/Backend 控制链验收。

## 审计补充（迁移验证中确认）

- AssetRegistry 本次 /Game 枚举：Material 19、MaterialInstanceConstant 29、MaterialFunction 1；MediaPlayer、MediaSource、MediaTexture、TextureRenderTarget 等类均为 0。WebBrowser 引擎默认材质是 `/Engine/WebBrowser/WebTexture_M` 和 `/Engine/WebBrowser/WebTexture_TM`（UE 5.8 WebBrowserSingleton.cpp），不是项目 MediaTexture 播放管线。
- 第一轮原生自动化确认：GameMode 的 OnDroneRegistered → SpawnReceiversFromRegistry → MultiDroneCharacter::BeginPlay 会再次注册不带 VideoUrl 的部分 Descriptor，清空刚写入的 source。V1 在 Video role 跳过 GameMode 场景生成/坐标初始化，Command/Standalone 保留原有行为；没有修改 Registry 或 MultiDroneCharacter 的业务实现。
- 用户已授权“随便弄几个 MP4”。原录像路径不可用后，使用 Scripts/video_demo_fixture.py 生成三份明确标注 MOCK 的 H.264 MP4，经机器已有 MediaMTX 播放；这替代本次本地 Demo 素材，不属于真实 UAV 验收。
