# TASK-V1 — Video Client Migration

## 结论

**CONDITIONAL PASS：独立 Video Client 本地 Mock 媒体验收通过。** `-ClientRole=Video` 创建唯一 Video Shell、没有 Command Shell/旧 HUD；物理 Selector 可切换三份 MP4，实际 WebRTC 视频播放正常。继承的 Command `1082 vs 1080` 高度断言仍失败，未修改。此结论不代表真实 Backend、UAV、Jetson、PX4 或飞行验收。

- Branch：`feat/triple-screen-video-v1`
- Implementation commit：`4f762b4c9c9a5edd41c39eed15e3d05cb0b820b2`
- 审计 commit：`fe17cab`；报告和截图另随本分支文档提交交付。
- A3 冻结节点：`feat/triple-screen-command-a3`=`50c631de9a55f9d1cd25e9f6520d11cf6fa888ec`，保持不动。
- V1 从最新集成 `7754811` 建立，再 fast-forward 承接冻结 A3 作为依赖。旧 Video 分支是集成祖先，无独有 Video 实现需要 cherry-pick。完整分析见 [TASK-V1-Audit.md](TASK-V1-Audit.md)。
- 没有 merge 到 `feat/triple-screen` 或 `main`，没有复制 UE 工程。

## 实际修改

| 文件（Source 路径相对 Source/UE5DroneControl） | 行为 |
|---|---|
| Video/VideoShellWidget.h/.cpp | 新增原生功能 UI：标题/当前 UAV、状态、最大化 Browser、友方 UAV Selector、底部信息和 Retry。一个 UWebBrowser 根据 Registry 的 DroneId→VideoUrl 导航；不创建数据模型或 HTTP/WS client。 |
| Command/CommandScreenManager.h/.cpp | 在既有 role/Show/Hide/Destroy 生命周期中管理 VideoShell。重复 Show/Video Initialize 不复制 Shell；Hide 停止 Browser；Destroy 清理引用和 Timer。Command Shell 创建逻辑保留。 |
| DroneOps/Control/DroneOpsPlayerController.cpp | Video 提前路由 manager，采用 UI-only input，跳过旧 HUD/工具/场景操作，关闭 Controller tick；旧 OpenDroneVideoWindow 在 Video role 路由为共享 Registry 选机。Command/Standalone 继续原逻辑。 |
| DroneOps/Control/DroneOpsGameMode.cpp | Video 使用现有原生 DroneOpsPlayerController，避开旧 BP HUD 覆盖；跳过坐标初始化、receiver/shadow 生成和相关注册订阅。 |
| UI/UIManagerBlueprintLibrary.cpp | 在 Video role 防止旧 DroneList/EnemyList/Sequence/Geographic/GroundProjection 面板被 Blueprint 兼容入口重新创建。 |
| Tests/VideoClientTest.cpp | PIE role、唯一 Shell、一个 Browser、Selector→Registry→source、同机 URL 更新、删除、Hide/Show/Destroy/重开和兼容入口回归。 |
| Scripts/audit_video_assets.py | UE AssetRegistry 资产审计复现脚本。输出至 Saved/VideoQA/assets.json，先确保输出目录存在。 |
| Scripts/video_demo_fixture.py | 生成三份 Mock MP4，循环推送到机器已有 MediaMTX；本地只读 HTTP fixture 提供 /api/drones。所有生成数据在 Saved/VideoQA/demo。 |

**Config、Content、A3 CommandShell/Theme、Registry/NetworkManager 业务代码和旧 DroneVideoWindowManager/Widget 均无 V1 修改。** 不实现 Map Role、TASK-A4 或跨主机选择同步。

## 数据流与状态语义

```text
既有 Backend GET /api/drones（本次是明确的 loopback HTTP fixture）
  → 既有 DroneNetworkManager → 既有 DroneRegistrySubsystem
  → VideoShell Selector / PrimarySelection
  → FDroneDescriptor.VideoUrl → 同一个 UWebBrowser
  → MediaMTX WebRTC 浏览器页面 → H.264 解码帧
```

Shell 每 0.25 秒投影 Registry，处理延迟注册、名称/URL 更新和删除；选择器只写既有 PrimarySelection。只保存展示/播放器状态，没有第二份业务模型。

`PLAYING` 需要浏览器视频元素尺寸有效、未暂停/结束且实际帧计数推进，不把页面导航成功当成视频成功。每约 0.5 秒读取当前页面的 video 状态、videoWidth/Height、totalVideoFrames；FPS 使用浏览器单调采样时间，避免失焦后回调集中到达造成虚高。generation + token + URL 校验丢弃旧页面结果；停止后导航 about:blank。

无源为 OFFLINE；页面/媒体等待、暂停/结束、媒体错误和超过 10 秒无帧均有独立状态。UI 使用 PLAYING 而不是把本地循环视频写成 LIVE。Camera ID 在当前 Descriptor 中不可得，显示 N/A；测不出的分辨率/FPS 也显示 N/A。

## 编译与修复

环境：UE 5.8.0、VS 14.44、Win64 Development Editor。

- 首次直接用中文工程路径编译失败：MSVC/PCH 路径被错误编码，C1083。没有改代码绕过编译检查。
- 核实已有 `C:/Users/wy331/Documents/UE5DroneControl-command` 是指向同一工程的 junction，改用该 ASCII 路径成功编译，非复制工程。
- 完整 native 编译：100 actions，Succeeded；测试增量编译：Succeeded。
- GameMode 隔离、FPS 采样和最终 Selector 对比度修复均重新编译成功。最后一次 [build-selector.txt](QA/TASK-V1/build-selector.txt)：Succeeded，11.17 秒；功能修复 [build-final.txt](QA/TASK-V1/build-final.txt)：Succeeded，22.73 秒。
- 第一轮 Video 测试发现源被清空：GameMode 生成 MultiDroneCharacter 后再次注册无 VideoUrl 的 Descriptor。Video role 跳过场景生成后源切换测试通过。没有修改 Command/Standalone 的影子机逻辑。
- Physical 发现默认 Selector 未选项黑底黑字，修复列表背景后重新物理点击并保存截图；未做复杂视觉设计。

## 自动化结果

最终功能代码测试发现/完成数均有日志和 JSON；不根据进程 exit 0 判断测试成功。

| 实际启动 | Discovered / Completed | Success（含 warning） | Failed | 结果 |
|---|---:|---:|---:|---|
| 无 ClientRole 参数，默认 Command | 5 / 5 | 4 | 1 | A2MapMode、AlertStore、StartupRole、RoleLifecycle 通过；CesiumWorld 唯一错误为原有 1082/1080。 |
| -ClientRole=Video | 2 / 2 | 2 | 0 | StartupRole、RoleLifecycle 通过。 |
| -ClientRole=Standalone | 1 / 1 | 1 | 0 | RoleLifecycle 实际 PIE 启动，旧 Shell 存在，VideoShell=0。 |

[automation-final.json](QA/TASK-V1/automation-final.json) 保存逐项状态、错误、发现数与 `Test Completed`/Queue Empty 证据。执行日志和完整 HTML/JSON 报告留在 `Saved/VideoQA/final-{command,video,standalone}`。

Video 生命周期测试实际覆盖：无 CommandShell/MapService、唯一 VideoShell/Browser、多次 Show 和 Initialize、旧 facade 不生成 DroneList、没有旧 VideoWindow、真正 ComboBox selection delegate 修改 Registry、URL 同机更新、选中 UAV 删除、Hide 后停止、Show 重用、重复 Destroy、重建后仍唯一。自动化中的 about:blank 只测生命周期，不作为媒体播放证据。

最后一次背景颜色调整发生在该功能回归之后；随后编译和物理 Selector/三路播放/Retry 再验通过。没有为纯背景颜色重跑无关 Command 业务矩阵。

## Physical Play 与媒体证据

物理 OS 窗口使用 `UnrealEditor.exe <project> -game -ClientRole=Video -windowed -ResX=1600 -ResY=900`，默认 CesiumWorld；仅本次命令行将 BackendBaseUrl 指向 `http://127.0.0.1:18989`，没有修改项目默认配置。

原 demo script 引用的旧 MP4/FFmpeg 路径不存在。用户授权“随便弄几个 MP4”后，生成三份 8 秒、1280×720、24 FPS、H.264 baseline、无音频 MP4，画面内有 UAV-01/02/03 和 `MOCK / LOCAL DEMO` 字样。通过现有 MediaMTX 的 `drone-1/2/3` 路径循环播放；[demo-sources.json](QA/TASK-V1/demo-sources.json) 保存文件路径、URL 与 SHA256。

| 验证 | 观察结果 |
|---|---|
| 独立 Startup | 日志 role=Video；Command Shell skipped；Video Shell created 仅一次；场景 spawn/坐标初始化 skipped。 |
| 物理 UI | 一个 Video View，占窗口主体；无 Command/旧 HUD；当前 UAV、Selector、状态和底部信息可见。 |
| UAV-01→02→03 | 实际点击 Selector；标题/底部/片内编号与对应路径一致，均实际播放并显示 1280×720、约 24 measured FPS。 |
| 重试 | 点击 Retry 后同一 UAV-03 恢复播放；原 WebRTC 会话关闭、新会话建立，稳定状态下仍为一个。 |
| 实例/音频 | 稳态 API 每次都是 1 个 WebRTC read session；三份素材无音轨，不声称做过真实音频并发验收。 |
| 关闭/重开 | 多次普通退出和重启无观察到 crash；最终日志 Browser stopped→Exiting。停止 fixture 前 API read session=0，旧会话记录 peer connection closed。 |
| Command | 最终自动化 PIE 实际渲染并截图；A3 shell 保持。白色地图区域和高度断言保留。 |

截图和证据入口：[QA/TASK-V1/README.md](QA/TASK-V1/README.md)。截图为实际 UE/OS 窗口，不是设计图；Windows 缩放后的 Video capture 1282×746，不能当作 viewport 1600×900 或修正 1082 断言的依据。

会话证据：[webrtc-sessions.json](QA/TASK-V1/webrtc-sessions.json)、[media-lifecycle.txt](QA/TASK-V1/media-lifecycle.txt)、[startup-lifecycle.txt](QA/TASK-V1/startup-lifecycle.txt)。切换/Retry 的短暂握手不等于稳态双播放器；旧 peer 关闭有单独日志。

测试结束后已停止本任务启动的 fixture/FFmpeg/MediaMTX，恢复测试前 Saved/DroneRegistry.json。生成的 MP4、依赖和完整日志保留在 Saved/VideoQA，未将大文件/工具二进制加入 Git。

## 复现

1. 使用本机 UE 5.8 编译当前 V1 分支，优先已有 ASCII junction。
2. 运行 `python Scripts/video_demo_fixture.py --ffmpeg <FFmpeg.exe>`；已有 stop 文件时，确认上一轮结束后删除 `Saved/VideoQA/demo/stop` 再运行。本次 FFmpeg 位于 `Saved/VideoQA/deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe`。
3. 启动 Video，并为此次 Demo 增加参数：`-ini:Game:[/Script/UE5DroneControl.DroneNetworkManager]:BackendBaseUrl=http://127.0.0.1:18989`。普通真实部署仍读原配置/API，无此 fixture override。
4. 选机、重试、退出；写入 `Saved/VideoQA/demo/stop` 停止本轮所有 fixture 子进程。fixture 在端口被占用时拒绝启动，不接管已有服务。

## Known Issues / 验收边界

- 保留既有 Command `1082 vs 1080` 断言；本次整体为 CONDITIONAL PASS，不将该失败隐去。
- 保留 A3 白色地图区域、低高度 3D 地形遮挡；Video 默认仍加载 CesiumWorld 场景资产，启动时曾出现 VSM non-Nanite 队列警告。未新增轻量地图或解决场景渲染问题。
- 现有 SimpleWebSocket UE 5.7/5.8 compatibility 警告、AllToolsets GameFeatureData 配置错误、uncooked -game 下实验性 toolset Python 错误仍可见。资产审计数据生成完成，但原 commandlet exit=1，不能算作无错误 commandlet PASS。
- 本地 fixture 仅提供 HTTP Descriptor；不提供真实 WebSocket，连接失败日志属于本次无 Backend 的测试边界。没有声称验证真实 backend selection broadcast 或 telemetry。
- 测试媒体页面是现有 MediaMTX 直接 video 元素；嵌套跨域 iframe、自定义跳转页面、其他播放协议和音频未覆盖。Media 状态/FPS 是播放端观测，不是 UAV 链路健康或相机采样率。
- 启动仍使用既有 DroneOpsGameMode/原生 DroneOpsPlayerController；Shell 播放无地图名依赖，但任意第三方 GameMode/地图 override 未逐一验收。
- 没有 packaged/cooked build、多 DPI/多显示器布局、长时间性能/内存压力测试。

## Deferred

第二台 Video 主机；Jetson/PX4/实际飞行；多路高清视频压力；录制；AI 目标识别；PTZ；Video Wall；最终网络部署；Map Role；TASK-A4 三联屏总集成；跨进程/跨主机 UAV selection 同步。
