# 三客户端 Figma UI 接入

目标工程：`D:\CodexPublish\drone_security`。设计文件：`NcGzq4onhsQHsJFAe0JBJe`。2026-10-20 继续完成 Map 与 Video，Command 的首次接入记录见 [FigmaCommandIntegration.md](FigmaCommandIntegration.md)，此前交互修复见 [UIInteractionFollowup.md](UIInteractionFollowup.md)。

## 设计范围

| 客户端 | 设计页面 | 完整界面 |
| --- | --- | --- |
| Command 指挥中心 | `2522:143` | 总览、方案、无人机；设备、告警、日志、新方案、标签弹窗 |
| Map 战术地图 | `2527:1191` | 2D `2527:1192`、3D `2528:1634`、设备下拉 `2528:1320`、航线编辑 `2528:1742` |
| Video 视频情报 | `2528:1313` | 六格 `2540:435`、四格 `2544:1896`、聚焦 `2544:1539` |

读取完整设计上下文与截图后，以 UE 原生 UMG / Slate 实现界面。截图只用于核对布局，不作为界面背景。原始 SVG 保存在 `Content/UI/Figma`，Map、Video 各自的资源和清单在子目录内，沿用该目录的 UFS 打包规则。

## 共享行为

Map、Video 使用新的 `UFigmaRoleHeaderWidget`，包含品牌、角色名称、语言、时钟和六项健康状态。语言请求经 `SetLanguage` 提交，选中状态来自已确认的 UI 偏好。六项状态分别来自系统状态、三客户端在线状态、后端会话连接和快照同步完成状态；它们不代表硬件连通或视频帧正常。

旧 `UStage1HeaderWidget` 保留给 Standalone。Command 使用已有带主导航的 Figma 头部。新头部不创建业务管理器、不改变 UAV 或视频目标。

所有客户端继续使用后端确认的方案、任务、选择与执行快照。选择无人机与视频目标独立；显式“查看视频”或 Video 源卡操作才请求修改 `video_target_uav_id`。部署与执行保持独立。

## Map 战术地图

界面包含地图作业标题、2D / 3D、定位、任务航线、查看视频、当前设备下拉和缩放按钮。框选、单机 / 集群移动、执行监控与方案移动预览沿用已有业务入口。地图内容仍是当前 Cesium / 世界视口，UI 外框不会建立第二个地图或用设计中的灰色示例图替换实际场景。

航线编辑侧栏复用 `UMapMissionRouteWidget` 的草稿、保存、退出保护、航点高度 / 速度 / 停留时间、后端提交与航线显示所有权。编辑时仍允许手动平移、缩放和 3D 旋转；自动定位 / 跟随继续受编辑锁约束。设备选择与目标预览以已确认的状态为准。

“编辑速度”打开默认航点速度面板，允许 0（使用默认值）或 0.5–15 m/s，取消不写入。它只影响之后新增的航点，不修改后端集群移动速度。地图外框、设备菜单、缩放区和编辑侧栏使用实际缩放后的控件边界阻止点击穿透。

## Video 视频情报

六格、四格和聚焦布局复用同一个 `UWebBrowser`。布局切换只调整主视频和副卡位置，不重新创建播放器。副卡中的设备来自注册表，点击后显式申请切换视频目标；没有分配设备的格子显示预留状态。

这与设计中的“单个活动播放器，其余为预留格位”一致，不表示六路视频同时解码。

播放器保持原有 URL、代次、媒体进度和过期回调校验。页面加载完成不代表播放成功；只有观测到媒体帧 / 时间进度后才显示播放状态。无目标、未配置源、连接中、停止进度与媒体错误均显示对应状态，重试仅对配置了视频源的目标开放。

右侧在线数量来自遥测，最近告警来自现有告警仓库，后端网络状态来自同步服务。AI 分类没有数据源时显示 `--` 和“未接入”，存储显示不可用。录像、回放、截图和告警筛选没有现有实现，工具栏保留设计入口但禁用并给出提示；没有伪造录像文件、检测结果或播放画面。

视频底部保留独立的“设为当前无人机”操作，以明确动作修改共享选择。它不会隐式切换视频源。语言变化与普通 UAV 选择变化不会重载播放器。

## 验证边界

测试采用隔离 Mock 后端和独立注册表文件。UE 渲染截图、组件动作和反射调用属于 **rendered/component automation**。真实鼠标操作、打包运行、真实视频流和真实无人机仍需单独验证。

首轮 Map / Video 自动化断言通过后，截图复核仍发现地图缩放符号、提示对比度、密集标签重叠，以及 Video 预留格位 PNG 加载和回放图标方向问题；这些问题另行修复，首轮截图和日志保留。地图标签只调整屏幕标注的位置，不移动无人机坐标；执行路径与航点颜色继续由原执行快照提供。

最终验证汇总见 [validation.json](../Evidence/FigmaClients/2026-10-20/validation.json)。

| 检查 | 结果与范围 |
| --- | --- |
| UE5.8 Development Editor 构建 | PASS；最终 [构建日志](../Evidence/FigmaClients/2026-10-20/build-map-fill.log)，模块 SHA256 `b2d87f1447de1d7a7b146ec69b16b9b20550cb352c7a0b745e291ea07407ecfe` |
| Map | 10 个测试用例通过：呈现、标签避让、单机选点、集群移动、3D 移动、航线草稿/默认速度保护、2D/3D 编辑相机、方案地图控件、Mock/received 遥测与 Actor 投影保护 |
| Command | 3 个用例通过：Figma 呈现、总览执行列表、方案内任务编辑/执行 |
| Video | 2 个用例通过：三布局/资源/生命周期/告警呈现、显式目标与播放器隔离 |
| 总计 | 最终各用例记录：预期 15、发现 15、完成 15、通过 15、失败 0。每次运行期间模块哈希不变，全部拥有的测试进程已回收，无清理错误 |
| 原始资源 | Map 25 文件、Video 30 文件，引用/尺寸/hash 无缺失；见 [资源审计](../Evidence/FigmaClients/2026-10-20/assets.json) |
| 中英文 | 新增 62 键，在 PO、archive 和已编译 locres 中均完整覆盖；见 [本地化审计](../Evidence/FigmaClients/2026-10-20/localization-audit.json) |
| 用户数据 | `Saved/DroneRegistry.json`、用户已修改的 `CesiumIonSaaS.uasset`、Backend 二进制均与本轮开始前 hash 一致 |

Video 的最终验证先于最后一行 Map 标签填充色调整，之后 Video 与共享源码未变；Map/Command 在该调整后运行，按每轮二进制分别保留记录，不混写为所有用例来自同一模块 hash。代码变更范围见 [记录](../Evidence/FigmaClients/2026-10-20/post-video-source-change.json)。

保留的失败与修正：初次编译存在局部变量遮蔽和 Scrollbar API 名称错误，后续构建通过；初次 GatherText 的 PowerShell 参数被截断，改用明确 argv 后资源生成完成。最终 GatherText 内部步骤退出 0，整个进程仍因项目既有 `GameFeatureData` AssetManager 配置错误退出 1，不能称为整个本地化命令无错误通过，见 [日志](../Evidence/FigmaClients/2026-10-20/localization-final.log)。

首次将扩展后的 `Mock.TelemetrySource` 放在 Command 角色运行，因该角色刻意不初始化地图坐标服务而失败；改为 Map 角色后原断言完整通过，没有修改源码或降低断言。失败记录位于 [Command 运行](../Evidence/PlanConfiguration/verification-1792478895558398100/summary.json)，成功重跑位于 [Map 运行](../Evidence/PlanConfiguration/verification-1792479298013699800/summary.json)。测试中的常规启动、离线 receiver、坐标朝向和精简快照日志警告均保留，没有将退出码 0 单独作为通过依据。

最终截图：

- Map：[主界面](../Evidence/FigmaClients/2026-10-20/screenshots/map/20261020-064845/01-main.png)、[设备选择](../Evidence/FigmaClients/2026-10-20/screenshots/map/20261020-064845/02-dropdown.png)、[3D](../Evidence/FigmaClients/2026-10-20/screenshots/map/20261020-064845/03-3d.png)、[航线编辑](../Evidence/FigmaClients/2026-10-20/screenshots/map/20261020-064845/04-route.png)。
- Video：[六格](../Evidence/FigmaClients/2026-10-20/screenshots/video/20261020-064321/01-six-slots.png)、[四格](../Evidence/FigmaClients/2026-10-20/screenshots/video/20261020-064321/02-four-slots.png)、[聚焦](../Evidence/FigmaClients/2026-10-20/screenshots/video/20261020-064321/03-focus.png)、[英文](../Evidence/FigmaClients/2026-10-20/screenshots/video/20261020-064321/04-focus-english.png)。

Map 截图中的灰色区域是离线测试世界的真实视口；这些截图不证明实时 Cesium 瓦片加载。Video 测试使用 `about:blank` 等隔离夹具，不证明真实视频流播放。两端视觉逐项记录见 [Map](../Evidence/FigmaClients/2026-10-20/map-visual-review.json) 与 [Video](../Evidence/FigmaClients/2026-10-20/video-visual-review.json)。
