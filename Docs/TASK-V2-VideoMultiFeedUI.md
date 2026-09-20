# TASK-V2 — Video Client Multi-Feed UI

## 结论

**CONDITIONAL PASS。** V2 页面结构、单路真实 Mock 播放和交互验收通过；继承的 Command 高度断言仍失败，真实多路/AI/Backend 能力不在此结论内。**本任务只有一个实际媒体播放器，不声明 6 simultaneous live streams PASS。**

- Branch: `feat/triple-screen-video-v2-ui`
- Source: `feat/triple-screen-video-v1` / `0a151e6d996deecaf633a5363f7db00476060658`
- Implementation commit: `18c24447a857c19a543caf32f8ee924de6248420`；报告与截图另随本分支文档提交交付。
- 开始时工作区干净，已 fetch remote refs；没有修改 A3 冻结分支、V1 分支或 integration/main refs。

## Implemented

- Video role 继续由 V1 role/startup → CommandScreenManager → 唯一 VideoShell 管理；没有新的 Manager、网络连接或业务模型。
- 监看中心：双语标题、当前时间、Registry UAV 数、布局位置数、实际播放数，左侧可滚动飞机列表，中央 Primary，右侧 AI/告警区域，底部工具栏。
- 6 路布局为 1 Primary + 5 Secondary，4 路为 1 + 3，Focus 折叠所有次级槽位并释放高度。三种布局复用同一 Browser。
- 列表与次级卡片共享点击处理：写 Registry.PrimarySelection。次级交换保留原 Primary 在被点击的槽位；其他位置顺序不变。超过 6 架可通过列表选择，空位置显示 RESERVED。
- Primary 复用 V1 MediaMTX WebRTC 浏览器播放、实际帧进度检测、浏览器单调时间测量 FPS、URL generation 隔离及 Retry/Cleanup。
- 状态遮罩在解码帧未前进时覆盖旧画面，防止导航期间旧 UAV 图像与新标题混淆。页面加载完成只进入 PAGE READY，不能进入 PLAYING。
- 无注册 UAV、无源、连接中、重试、加载失败/无帧、关闭均有明确状态。只有实际帧进度会显示绿色 PLAYING。
- 明确显示 MODEL N/A：FDroneDescriptor 没有飞机型号字段；Registry.GetDroneModelType 是渲染外观预设，缺省 Original，不能当作已知飞机型号。工具提示读取并明确标记 Configured visual。
- 单机连接状态从 Registry telemetry 读取，没有更新时间时为 N/A。媒体有源不等于飞机在线。System Online、Storage、Network 暂为 N/A，AI 为 NOT CONNECTED。
- Mock/QA Descriptor 名称触发页头 QA VISUAL STATE / MOCK 标记；未向生产数据模型增加 Demo 状态。

## UI Reserved

这些区域只有 UI，不计为对应业务功能通过：

- 五个 Secondary Feed 为 `VIDEO SLOT / NO ACTIVE STREAM`；无并发播放器、无截图循环。
- AI Detection 的人员、车辆、无人机、异常行为、其他目标分类；`AI DETECTION NOT CONNECTED`。
- Recent Alerts 的 Title/UAV/Target/Timestamp/Severity 字段说明及 INFO/WARNING/CRITICAL 图例；没有告警行，显示 `NO ACTIVE ALERTS`。
- Primary 层级：VideoSurface → DetectionOverlay(Canvas，空) → StatusOverlay → UserInteractionLayer(Canvas，空)。没有写死框、标签或置信度。
- Fullscreen、Record、Playback、Screenshot、Alert Filter：Disabled，tooltip 为 Reserved for future integration。Refresh/Retry 已连接 V1 重试。

## 实际修改范围

| 文件 | 修改 |
|---|---|
| Source/UE5DroneControl/Video/VideoShellWidget.h/.cpp | 监看中心布局、可点击 FeedButton、展示槽位顺序、视图模式、状态遮罩；保留唯一 Browser 与原媒体检测 |
| Source/UE5DroneControl/Tests/VideoClientTest.cpp | 扩展真实 PIE 内的列表/交换/布局/空状态/无源/生命周期检查 |
| Scripts/video_demo_fixture.py | 可选 --multi-feed，增加 4/6 无源、5 未发布源，以及空列表 fixture 开关 |
| Docs/TASK-V2-VideoMultiFeedUI.md、Docs/QA/TASK-V2 | 验收记录和截图 |

Config、Content、Command UI、GameMode/Controller、Registry、NetworkManager 和 V1 startup manager 无修改。

## 编译与自动化

最终构建 **Succeeded**，13.18 秒，增量 4 actions；V1 媒体 JSON 探测局部变量已显式初始化，最终编译无该变量告警。构建使用 UE 5.8 / Win64 Development Editor，ASCII junction `C:/Users/wy331/Documents/UE5DroneControl-command` 指向原工程，不是复制工程。日志在 Saved/VideoQA/build-v2.log。

| 启动参数 | discovered / completed | 成功 / 失败 | 结果 |
|---|---:|---:|---|
| -ClientRole=Video | 2 / 2 | 2 / 0 | StartupRole + RoleLifecycle 通过 |
| 无 override，默认 Command | 5 / 5 | 4 / 1 | 仅 CesiumWorld 高度断言失败 |
| -ClientRole=Standalone | 1 / 1 | 1 / 0 | 原 Shell 存在，Video Shell 数量为 0 |

逐测试结果见 [automation-results.json](QA/TASK-V2/automation-results.json)。报告的 succeeded 顶层字段不包含 succeededWithWarnings，因此按 tests[].state 和日志 Test Completed 统计。测试进程均 exit 0，不能据此推断全成功。

最终状态排版修正后再次运行 Video：**discovered 2 / completed 2 / success 2 / fail 0**。日志为 Saved/VideoQA/v2-video-final.log。Command/Standalone 回归在该纯 Video 状态排版微调前完成，对应代码未再改动。

自动化入口（分别设置上述 role 和报告目录）：

```text
UnrealEditor.exe <project> -unattended -nosplash
  -ExecCmds="Automation RunTests DroneOps.Command+DroneOps.Video.RoleLifecycle"
  -TestExit="Automation Test Queue Empty" -ReportExportPath=<report> -abslog=<log>
```

Video 运行 StartupRole+RoleLifecycle；Standalone 运行 RoleLifecycle。RoleLifecycle 实际启动 PIE、操作按钮 delegate、检查唯一实例与共享选择，并执行 FEndPlayMapCommand。Fixture 的 about:blank 只测架构，媒体播放通过下面物理窗口验证。

## Physical Play

目标 -game -ClientRole=Video -windowed -ResX=1920 -ResY=1080，使用 3 个本地合成 H.264/1280×720/24 FPS/无音轨 MP4，经现有 MediaMTX RTSP→WebRTC 播放。HTTP18989 是 Descriptor fixture，真实 Backend/WS 未连接。

复现：

```powershell
python Scripts/video_demo_fixture.py --ffmpeg Saved/VideoQA/deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe --multi-feed
```

运行前移除上一轮 Saved/VideoQA/demo/stop；写入 stop 文件可停止本轮 fixture 的子进程。empty 文件存在时 HTTP fixture 返回空列表。测试不能与其他同端口服务并行启动，脚本会检查端口占用。MP4、可执行依赖、完整引擎日志留在 Saved 下，不提交。

### 物理窗口证据

| 检查 | 实际观察 / 证据 |
|---|---|
| 默认独立 Video / 六位置 | 唯一 Shell 日志、无 Command，Primary+5 槽位、6 架列表、右侧面板完整：[默认截图](QA/TASK-V2/default-six.jpg) |
| 列表选择 | UAV-02 Cyan 选中，播放图像内为 UAV-02 Mock：[Selected UAV](QA/TASK-V2/selected-uav02.jpg) |
| Secondary 交换 | 点击 UAV-03，原 Primary UAV-02 留在原 UAV-03 槽位：[交换截图](QA/TASK-V2/secondary-switch-uav03.jpg) |
| 4 / 6 / Focus | 真实按钮点击，1+3、1+5、Primary 高度增大：[4 view](QA/TASK-V2/four-view.jpg)、[Focus](QA/TASK-V2/focus.jpg) |
| 无源 / AI / Alert | UAV-04 NO SOURCE、Retry disabled；AI disconnected 与 NO ACTIVE ALERTS：[空媒体状态](QA/TASK-V2/no-source-ai-alert-empty.jpg) |
| 未发布源 | UAV-05 有 URL，未产生视频帧，STALLED / NO FRAMES：[失败状态](QA/TASK-V2/offline-no-frames.jpg) |
| Retry | 未发布源重载进入 PAGE READY 仍不报 PLAYING；有效源 RETRYING 后恢复实际视频：[重试](QA/TASK-V2/retrying.jpg)、[恢复](QA/TASK-V2/retry-recovered.jpg) |
| 空 Registry | 独立重开，HTTP 空列表、0 UAV、UAV N/A、6 个 Reserved、Retry disabled：[空列表](QA/TASK-V2/empty-registry.jpg) |
| 资源释放 | 稳定播放/交换/Retry 后 MediaMTX read 会话各为 1；物理关闭后 0，正常 LogExit，无 crash。JSON 证据见本目录 media-*.json |
| Close / Reopen / PIE Stop | 自动化 Hide/Show 同 Browser、Destroy/Initialize 重开仍唯一；多次真实进程关闭和重开；PIE Stop 由 FEndPlayMapCommand 执行并完成测试 |

默认/状态图来自最终构建；切换与布局图来自仅状态排版微调前的同一 V2 实现。最终 Video 测试再次覆盖同一交互和生命周期。

## Known Issues / 验收边界

- 继承的 Command CesiumWorld viewport height 1082 vs 1080 断言，本轮仍是唯一失败；不修改断言或 Command UI。
- 未做完整 Multi-DPI；截图工具按 Windows 显示缩放输出，截图编码尺寸不等于客户端渲染尺寸。
- 既有地图启动可能短暂显示 VSM Non-Nanite 警告；不以隐藏引擎告警作为修复。
- 单播放器、静音 Mock 视频；不代表真实 UAV、Backend、音频或六路并发验收。

## Deferred

真正 4/6 路并发解码、AI Backend/Detection/Bounding Box/Tracking、真实告警、录制/回放/截图/告警过滤、原生全屏控制、真实型号字段、系统网络/存储聚合、真实 UAV/Jetson/PX4、第二台 Video Host、视频墙、PTZ、飞行链路、多路压力测试、完整 Multi-DPI、Map role 和 TASK-A4 三联屏集成。

## 清理与证据

测试 fixture、3 个 FFmpeg publisher、MediaMTX 和测试 UE 进程均已退出；Saved/DroneRegistry.json 已恢复测试前备份。完整日志位于 Saved/VideoQA，提交的精简证据见 [runtime-evidence.txt](QA/TASK-V2/runtime-evidence.txt)。默认 Editor Play 的配置和 Command 启动逻辑无变更。

