# 最新 Figma UI 调整（2026-10-29）

本轮重新读取 [无人机安防指挥系统 UI](https://www.figma.com/design/NcGzq4onhsQHsJFAe0JBJe?node-id=2579-112) 的最新设计上下文和截图。正式工程为 `D:\CodexPublish\drone_security`，基线 `b77e74f239100880a8dd6abc4e690453210740d6`，开始修改前已确认 `main` 与 `origin/main` 一致。

## 调整范围

- Command 总览 `2522:144`：四项统计之后显示等宽“当前任务 / 当前方案”，下方地图与最近事件对齐；活动任务仍使用可滚动的侧栏，保留地图和执行控制所需空间。恢复设计对应的统计图标、底盘、箭头和流程页脚。
- 方案详情 `2579:112`：左侧为方案摘要、真实阶段的五步流程、任务卡和部署操作；右侧为部署信息、所选任务的无人机 / 航点 / 距离、检查提示、部署记录和执行历史。任务操作继续使用现有选择、编辑、执行与查看视频入口。配置和执行页保持滚动可达。
- Map 主界面 `2527:1192`：补齐框选、集群移动图标，新增“手动接管”入口，并调整当前设备选择器。只有已确认单选、正在执行已部署任务且满足既有接管条件时启用；按钮只打开原有目标预览，不立即终止任务或发送移动命令。后果提示、确认及 Backend 版本校验沿用原流程。
- Video 视频情报 `2540:435`：用户指出预览遗漏后，重新读取最新设计，确认缺少底部“手动接管”按钮。已复用原始图标补齐按钮、调整底栏间距，并修复 AI / 系统 / 底部分隔线的亚像素显示问题。Video 角色不具有执行权限，按钮保持禁用并用中英文提示引导到 Map / Command 选择目标并确认；没有新增或伪造跨客户端交接。播放器和视频目标协议保持原有逻辑。Map 显式选机后的 Video 同步继续遵循 [现行有序确认协议](EditReturnVideo.md)。

运行时数量、名称、状态和记录均来自现有模型。设计中的示例编号、时间、距离和在线状态没有写死到业务界面。部署仍与开始执行分开。

新增原始 SVG 位于 `Content/UI/Figma/Refresh20261029`；[清单](../Content/UI/Figma/Refresh20261029/asset-manifest.json) 记录来源图层、尺寸与 SHA-256。资源未改写；代码使用本地资源，不使用临时 Figma 下载地址或整屏截图作为界面背景。五步底图保留原始 SVG，当前步骤由状态覆盖层显示。

## 验证

四个页面统一从 [完整截图预览](../Evidence/FigmaRefresh/2026-10-29/preview.md) 打开：总览、方案详情、战术地图、视频情报分别列出，另附其他状态。前三页保留对应验收原图；视频页新增当前构建的六格、四格、聚焦和英文四张截图，没有将旧图描述为本轮新截图。

本轮构建、测试计数、渲染截图、资源审计和用户文件保护结果记录在 [验收汇总](../Evidence/FigmaRefresh/2026-10-29/acceptance.json)。实际结果以该文件为准。

最新相关结果为 **11 发现 / 11 完成 / 11 通过 / 0 失败**：此前 Command / Map / 接管相关 9 项，加上本轮 Video 呈现与请求反馈 2 项。各项采用对应最后一次通过记录，并单独保留模块哈希；方案详情的视觉微调由 MissionActions 复验，Map 按钮修复由 MapPresentation 复验，Video 补齐由 VideoPresentation / RequestFeedback 复验，没有将此前所有检查混写为同一二进制的完整重跑。

视频页 [专项记录](../Evidence/FigmaRefresh/2026-10-29/video/validation.json) 保留改前 / 改后运行、当前模块哈希、四张新截图和权限边界。测试检查三个布局与英文中按钮持续可见但禁用、无执行委托、无选择 / 视频目标 / 播放器副作用、底栏互不重叠及原 SVG 分隔线非零实际尺寸；原有单播放器、VideoView ACK、重试、告警和媒体代次断言保留。播放器探针为合成组件数据，不能作为真实视频播放证据。

最终 Editor 构建通过，12 个新增 SVG 哈希一致，6 个排除在修改范围外的受保护文件在自动化验证前后保持一致，所有验证脚本拥有的进程均清理完成。首次局部变量重名构建失败，以及新增地图文字宽度断言发现的刷新内边距问题，都保留在验收记录中；修正生产代码后原断言通过。

中英文 locres 已生成，新增工具栏文字通过编译资源切换检查。GatherText 内部退出 0，整个进程仍因项目既有 `GameFeatureData` Asset Manager 配置诊断退出 1，详见 [诊断摘录](../Evidence/FigmaRefresh/2026-10-29/localization-diagnostics.txt)。

已核对截图：[总览](../Evidence/FigmaRefresh/2026-10-29/screenshots/command-overview.png)、[多任务状态](../Evidence/FigmaRefresh/2026-10-29/screenshots/overview-active-tasks.png)、[中文方案详情](../Evidence/FigmaRefresh/2026-10-29/screenshots/plan-detail-zh.png)、[英文方案详情](../Evidence/FigmaRefresh/2026-10-29/screenshots/plan-detail-en.png)、[Map 主界面](../Evidence/FigmaRefresh/2026-10-29/screenshots/map-main.png)、[航线编辑](../Evidence/FigmaRefresh/2026-10-29/screenshots/map-route.png)。

UE RHI 截图与控件动作属于渲染 / 组件自动化证据；隔离 Mock Backend 和合成 Map peer 只用于验证业务流程，不构成原生鼠标、真实无人机或真实视频流验收。
