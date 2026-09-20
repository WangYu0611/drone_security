# TASK-P2 验收证据

日期 2026-09-14。完整说明：[TASK-P2-CommandCenterV2.md](../../Docs/TASK-P2-CommandCenterV2.md)。结论 CONDITIONAL PASS；P2 功能通过，Git 作者身份阻塞已解除，现有源码/测试/证据已提交并核验 clean。保留历史问题及新显存容量风险；不是实际 UAV/视频/飞行验收证明。最终 Git 收尾见关联报告。

## 实际窗口截图

所有 JPG 都来自真实 UE game 原生窗口捕获的 JPEG 数据，仅修正文件后缀，未重新编码、裁剪、拼接或生成。A–H 中三角色在独立进程同时运行，逐窗口截图由 Context version/进程记录关联。启动参数 1920×1080，主机缩放后捕获尺寸约 1538×878；不是 1920×1080 逐像素认证。

| 场景 | 截图 | 结果 |
| --- | --- | --- |
| A 三端在线 | [Command](A-command-center-v2.jpg)、[Map](A-map.jpg)、[Video](A-video-follow-off.jpg) | PASS，Clients 3/3，v18 |
| B Command map 选择 2 | [Command](B-command-map-selection.jpg)、[Map](B-map-highlight.jpg)、[Video](B-video-independent.jpg) | PASS，v19，Video 保持 1 |
| C Main Map 实体选择 3 | [Map](C-map-entity-selection.jpg)、[Command](C-command-synchronized.jpg)、[Video](C-video-source-retained.jpg) | PASS，v20，Video 保持 1 |
| D Video 本地 2→1 | [Source 2](D-video-local-two.jpg)、[Source 1](D-video-local-one.jpg) | PASS，Global 3 / v20 不变 |
| E Follow ON | [立即跟随](E-follow-on-current-target.jpg)、[Command 选择 2](E-command-change.jpg)、[Video 跟随 2](E-video-follows-two.jpg) | PASS，v21 |
| F Follow OFF | [Command 选择 3](F-command-change.jpg)、[Video 保持 2](F-follow-off-retains-two.jpg) | PASS，v22 |
| G Alert | [Raised / Log](G-alert-raised-event-log.jpg)、[原子选择](G-command-atomic-alert.jpg)、[Map](G-map-alert-target.jpg)、[Video](G-video-source-retained.jpg) | PASS，v22→v23 一次更新三个字段 |
| H Command 重启 | [恢复 Context](H-command-context-restored.jpg)、[Video 保持](H-video-unchanged.jpg) | PASS，恢复 v23 |
| I Set As Active | [Video](I-video-set-active.jpg)、[Map](I-map-set-active-result.jpg)、[Command](I-command-set-active-result.jpg) | PASS，v24；Map 截图同时记录新显存警告 |
| J Filter / Clear | [ALERT 分类](J-alert-category-filter.jpg)、[Clear Display](J-clear-display.jpg) | PASS，Backend v24 不变 |
| 最终 UI | [Command V2](final-command-center-v2.jpg)、[筛选菜单](final-readable-filter-menu.jpg) | 最终样式构建复核，单独 Command / Clients 1/3 |
| 历史遮挡复核 | [2D](regression-map-2d.png)、[3D](regression-map-3d-occlusion.png) | 本轮 A2 引擎渲染原始 PNG；3D 低高度遮挡仍存在 |

仅 P2.QA fixture 的 GPS/告警为合成输入，并在 UI/日志明确标注。Context HTTP/WS、Backend 和三进程均真实。没有真实摄像头流或无人机控制验收。A-video 截图含启动时 UE 调试警告；后续 B–F 截图提供稳定画面，未修改截图隐藏警告。

## 结构化结果与日志

- [automation-summary.json](automation-summary.json)：21 个选定 UE 用例，20 Success / 1 Fail；最终两项 UI 用例来自 final-ui。每个用例均有 Test Completed。
- `*-results.json`：完整 UE Automation report；`*-events.log`：测试开始/完成、断言或 P1/Video 关键事件。command-delivery 为业务验收构建历史结果，最终 UI 验收以 final-ui 为准，不重复计数。
- [P1 protocol](p1-protocol.log)：11/11，真实独立 Backend 19080/19081。
- [Backend tests](backend-tests.log)、[GTest XML](backend-tests.xml)：47 执行，45 通过，2 指定历史 planner 失败。
- [最终 UE build](ue-final-build.log)、[业务验收 UE build](ue-build.log)、[筛选样式 build](ue-filter-style-build.log)、[Backend build](backend-build.log)。
- [A Context](A-context.json)、[A Presence](A-presence.json)、[G before](G-before-context.json)、[G after](G-after-context.json)、[H restored](H-context.json)、[J after Clear](J-context-after-clear.json)。
- [三个 UE 命令行](three-processes.json)、[重启后进程](final-live-processes.json)：历史取证快照；不能作为当前在线 PID 使用。
- [显存记录](gpu-memory.txt)：RTX 3060 Laptop 6GB，5851 / 6144 MiB。本次新风险，不属于历史 Known Issue。
- [业务版本 SHA256](business-acceptance-binary-sha256.json)、[最终版本 SHA256](binary-sha256.json)：A–H/I/J 使用前者；最终菜单对比度及 attribution 展示修正后，仅重跑有关 UI 用例及实机复核，未声称全部旧截图来自最终 DLL。
- [截图清单](screenshot-manifest.json)：真实编码、字节数和 SHA256。
- [Git 状态](git-status.txt)：收尾前的分支/HEAD/dirty 历史快照，原样保留；最终 Git 状态见报告及最终交付回复。

完整本机试跑日志在 `C:/Users/wy331/Documents/UE5DroneControl-p2/Saved/P2QA`，包含已修复 Mission.ArrayId 回归和已排除预览并发 SQLite lock 的历史试跑。它们不计入选定最终统计，也不归为历史产品问题。

## 复现与限制

[launch-game.ps1](launch-game.ps1)、[run-automation.ps1](run-automation.ps1) 保存本机路径及参数。先启动独立 Backend，三角色顺序启动；自动化期间关闭预览。P2.QA 为显式测试命令，普通生产启动省略。底图依赖配置的 XYZ 服务；网络失败显示重试，不保证离线可用。Event Log 仅为 500 条会话投影，重启不恢复旧日志历史。

Git 作者已验证为 WangYu0611 <331508364@qq.com>；源码/测试/证据提交后工作区 clean，报告更新独立提交后再次核验。未 merge 或改 main/P1，原 P1 工作区两个用户未提交项保留。31 张清单截图和最终 DLL 哈希复核匹配；本次没有重跑运行时验收。
