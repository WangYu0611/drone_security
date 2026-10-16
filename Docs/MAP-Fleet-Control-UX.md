# MAP 集群控制交互与视觉调整

基线：`f4e3870a1ce9cb338d21ae09c921d54fde9f85c6`。只调整 MAP 的交互与呈现，继续使用现有 Backend 执行、占用检查与 Route V2。

## 操作变化

- 框选后在卡片直接点击“下一步 · 选择目的地”，按“选飞机 → 选目标 → 检查确认”完成操作。下一步等待共享选择确认，框选和选点都不启动执行。
- 3D 高度提供滑杆、±10 米按钮和精确输入；同时显示当前高度与上升/下降差。滑杆用于相对当前锚点 ±100 米微调，精确输入仍沿用既有坐标域检查。高度是椭球高，不是离地高度。
- “重新选点”清除旧预览并保留输入的高度；“适配预览”由用户主动触发取景，目标坐标不变。坐标详情默认收起。
- 目标卡显示约计航程；Mock 环境按 Backend 配置速度给出约计耗时。未取得真实执行速度时不编造预计时间。预计时间不代替执行进度。
- 确认后切换成整组执行卡，进度来自 Backend 各成员快照。暂停、继续与终止集中在同一处。“终止任务”先显示整组确认，点击“终止整组”才提交；“保留任务”只关闭确认，不恢复或终止执行。
- 完成或终止后可“再次移动”，高度取最后确认的位置，必须重新选点。“收起”只关闭卡片，不改变任务数据。

## 视觉变化

- 使用较大的圆角卡片、分步标签、明确的主按钮、较大的高度数字和更多留白。内容区可滚动，主要操作保留在底部；面板高度随窗口和当前阶段调整。
- MAP 工具栏文字放大，连接状态使用清晰的同步/模拟环境提示；地图使用轻微蓝灰色遮罩提高标记对比度，没有添加虚构地形或替换底图。
- 集群交互期间用带引线的无人机标签替代重叠名称，黄色标识锚点，青色标识其余选中成员。退出该呈现时恢复原名称组件的可见状态。
- 集群卡片管理当前执行组的 HUD，减少重复的航点、无人机名称和旧监控卡片。路径主体始终由 Route V2 世界对象显示。
- 每架无人机只显示最近一次自由集群移动轨迹；新目标预览期间让出旧的终态轨迹呈现。历史 Backend 执行记录不删除，Security Plan 的 Mission 所有权规则保持不变。
- 原生检查发现室外场景曝光约 19.2 EV，超出默认缓存光照的 [-8, 12] EV 范围。按本机 UE5.8 渲染源码将项目缓存预曝光设为 12，使可用范围覆盖 [0, 20] EV，处理顶部光照截断提示的实际原因；未禁用引擎警告。

缓存预曝光按本次室外场景设置；其他日夜照明条件未在本次复查，超出 [0, 20] EV 的场景需要依据实测重新选择缓存范围。

## 验证方式

`Scripts/FleetControlUX/verify.py` 使用独立 Mock Backend，为每组 UE 测试启动新 Editor，并检查发现、完成和通过数量。覆盖三步引导、相对高度滑杆、重新选点、显式取景、就地终止确认、再次移动、2D/3D、相机锁定，以及 Security Plan 航线、执行监控与本地化回归。

`Scripts/FleetControlUX/qa_host.py` 仅管理任务拥有的 Mock Backend/MAP 进程；原生鼠标与键盘检查使用 Computer Use，HTTP 只做只读取证。证据保存在 `Evidence/FleetControlUX`，改版前截图为 `native/00-before-preview.png`。首轮编译发现局部变量 `Slot` 与 UE 控件属性同名，保留失败日志并修正命名，没有放宽编译检查。

## 检查结果

- 功能自动化使用 build-6。首轮 `verification-1792142136280641800` 发现并完成 10 项，8 项通过；两个旧回归分别缺少“三 Mission 方案”和“已暂停执行”的夹具。补齐原有夹具并隔离到另一 Mock runtime 后，`verification-1792142438650404200` 的两项均发现、完成并通过。合计 **10 个唯一测试全部通过**，其中 5 项保留既有环境警告；没有修改失败断言或安全阈值。
- HTTP/WS 的 **7 个场景通过**，两次运行均保留结果。覆盖共享选择、Video Target 隔离、MAP 权限、双向占用冲突、幂等、整组控制和四机三维到达。Backend 生产代码未改动。
- 中英文新增 **43 个键**均进入 locres。GatherText 内部步骤返回 0，整体进程仍因既有 `GameFeatureData` Asset Manager 配置错误返回 1；保留原始日志，不把整条命令描述为无错误通过。
- build-8 进行了完整原生输入验收；之后只调整任务结束卡片的一行呈现，避免重复成员信息挤占结束提示。`functional-build-6.json`、`native-tested-build-8.json` 和封存清单分别记录验证时与最终二进制哈希。
- 最终 build-9 编译通过；`native/17-final-terminal-card.png` 复查结束提示、状态、再次移动和收起均完整显示。相较 build-8 只隐藏终态卡片内重复的成员行，执行中的成员信息仍显示。

## 原生验收

- `03-empty-selection-guidance.png`、`04-four-selected-dark-markers.png`：空选择时入口保持在第一步、下一步禁用；真实拖框后共享选择确认，标签用引线区分。
- `05-final-80m-preview-en.png`、`06-final-preview-en-large.png`、`07-final-3d-preview-zh.png`、`08-framed-preview-zh.png`：真实滑杆从 30 米调到 80 米，显示上升 50 米、约 70 米航程和约 9 秒；中英文、普通窗口与最大化窗口均复查。图例显示 30–80 米，适配预览保留目标和高度。`final-preview-no-execution.json` 验证尚未创建执行。
- `09-native-executing.png` / `10-native-paused.png`：点击确认后进入统一执行卡，四机在 65.93795975596488 米整组暂停；`paused-state.json` 为 4/4 PAUSED。
- `11-inline-stop-confirmation.png` / `12-keep-task-remains-paused.png`：打开终止确认、选择保留任务，Backend 执行对象与暂停快照完全一致，既没有终止也没有恢复。对应只读快照为 `stop-prompt-no-execution-change.json`、`kept-task-state.json`。
- `13-native-completed.png` / `completed-state.json`：真实点击继续后，4/4 COMPLETED，80 米且三维位置等于各自终点。
- `14-again-preview-old-routes-suppressed.png`：再次移动默认使用 80 米当前位置，新目标预览隐藏旧的终态轨迹。第二次短途四机到达 60 米；完成早于检查暂停的点击，未将该次点击记作暂停成功，快照保留在 `second-completed-state.json`。
- `15-high-target-framed.png` / `16-native-aborted.png`：键盘精确输入 200 米、重新选点并适配预览，再原生确认较长移动。暂停后确认终止，最新组 4/4 ABORTED，位置与暂停时相同，未到达 200 米；`before-abort-state.json` 与 `aborted-state.json` 核对。全部 3 组中没有残留活跃执行。
- `video-before.json` 与 `video-after.json` 完全一致，框选、调高度、切换语言与执行没有自动切换 Video Target。

原生首轮截图 `01-layout-review-before-refinement.png` / `02-native-slider-review.png` 保留标签底色和预览区布局问题。最终改为显式深色标签、固定底部常用操作，并逐帧刷新路径图例及预览期间的旧轨迹显隐；不是用 Backend 写入刷新画面。

以上仅为离线 Mock 软件验收。真实飞行、Cesium 在线地形/离地高度、持续右键/中键 3D 手势均为 **NOT NATIVE VERIFIED**。相机锁定实现未改；原有 `CesiumIonSaaS.uasset` 修改不纳入本次提交。任务拥有的进程在取证后停止。
