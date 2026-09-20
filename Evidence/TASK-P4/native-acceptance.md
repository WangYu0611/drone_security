# TASK-P4 Native A–N

验收使用隔离 Backend 19282/19283、三个真实 Unreal -game 进程及 P2.QA 合成遥测。图片均来自实际窗口，没有合成界面。PASS 仅限本表业务边界；不代表真实飞机/摄像头验收。

| Native case | Result | 实测结果 | Evidence |
|---|---|---|---|
| A Startup | PASS | 三独立 -game 客户端 ONLINE；重启验收再次确认 3/3。 | [客户端](closure/N-presence.json) · [进程](closure/N-processes.json) |
| B Create Plan | PASS | Command 原生创建 plan-1。 | [创建截图](native/B-command-created-trial.png) |
| C Mission Organization | PASS | 三个 Mission 分配 UAV-01/02/03；重复分配被拒绝。 | [三任务](native/C-three-missions-assigned.png) · [重复拒绝](native/C-duplicate-assignment.png) |
| D Map Edit Request | PASS | Command 发出明确 Map 编辑请求，Map 获得编辑会话。 | [Map 编辑](native/D-map-editing.png) · [会话](native/D-edit-session.json) |
| E Dirty Draft | PASS | Map DIRTY；Command Review/Deploy 阻塞。 | [草稿](native/E-map-dirty.png) · [阻塞](native/E-command-review-blocked.png) |
| F Save Failure | PASS | 仅本次隔离 fixture 的临时写入路径受控失败；编辑器和航点保留，继续添加航点并重试。Actor/undo 精确对象断言另由真实 UE 测试证明。 | [失败后草稿](native/F-save-failure-retains-draft.png) · [继续编辑](native/F-continued-editing.png) |
| G Save Success | PASS | 三条航线获得保存 ACK。 | [East](native/G-east-saved.png) · [West](native/G-west-saved.png) · [North](native/G-north-saved.png) |
| H Validation | PASS | 不完整配置先失败，补齐后 READY，3/3/3。 | [不完整](native/H-validation-incomplete.png) · [READY](native/H-ready-three.png) |
| I Review | PASS | 显示三个 Mission 的 UAV、航点数、参数、issue 和 revision，并明确只检查业务配置。 | [完整审阅](native/I-review-details.png) |
| J Review Invalidation | PASS | revision 10→11 清除旧 Review，Deploy 不可用；重新检查/审阅。之后另一次航线保存形成 revision 12，再检查/审阅后才部署。 | [最终失效快照](native/J-review-invalidated-final.json) · [Deploy 不可用](native/J-deploy-unavailable.png) |
| K Localization | PASS | 三端中文/英文同步；此前 3 点未保存草稿在切换期间保留；selection、业务记录、review、video target 不变，source generation 不增加。最终两项明确显示缺陷已修复并截图。 | [不变性](native/K-invariance-results.json) · [草稿英文](native/K-map-en.png) · [最终六图索引](closure/localization-results.json) |
| L Video Target | PASS | 明确 UAV-01 后，Command active UAV-02 不改变 Video；明确调用 UAV-02 才切换。无真实视频源，未声称 PLAYING。 | [独立状态](native/L-independent-state.json) · [保持 01](native/L-video-remains-uav01.png) · [明确 02](native/L-video-explicit-uav02.png) |
| M Deploy | PASS | plan-1 revision 12 与三 Mission DEPLOYED，deployment-12 存在，界面明确执行未启动。执行边界证据区分静态可达性与日志计数。 | [确认](native/M-deploy-confirm.png) · [已部署](native/M-deployed.png) · [执行边界](closure/deploy-execution-boundary.json) |
| N Restart / Copy | PASS | 真实 Backend 和三个 -game 客户端均恢复；Command 原生复制 plan-13 并改名，原 deployment-12 完全不变。 | [Command 恢复](closure/N-command-restored-deployment.png) · [Map 恢复](closure/map-zh-Hans.png) · [Video 恢复](closure/N-video-restored-warning.png) · [原生复制](closure/N-copy-created.png) · [原生修改](closure/N-copy-modified.png) · [ID/哈希](closure/N-copy-results.json) |

## 试验说明

- J-review-invalidated.json 是按钮未成功操作时的早期快照；以 J-review-invalidated-final.json 为准。
- K 语言对比完成后，窗口坐标变化使离开操作实际选中了 Save；North 第三个点保存为 revision 12。此操作不计为原生 Discard 通过。Discard 的 ACK/待选任务行为由 MapLiveDraftGuard 真正验证。
- N 恢复先核对 plan-1/mission-4/active UAV-02/video UAV-02/中文，再操作复制。复制后活动 Plan 按设计变为 plan-13。
- N-video-restored-warning.png 保留瞬时 VSM/Nanite 工作队列警告；最终 video-zh-Hans.png 与 video-en.png 为警告消失后的真实截图，没有隐藏控制台警告或重绘。
- Command 系统菜单期间发生一次心跳超时并恢复，离线语言请求未提交；截图与日志均保留。
