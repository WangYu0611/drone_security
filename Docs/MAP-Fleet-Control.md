# MAP 鼠标框选与集群移动

基于 `9951ad20177a71c743b0be89dbb7be7acf6bc7fa`，在 `D:/CodexPublish/drone_security` 的 main 实现。

## 操作

1. 在 MAP 工具栏点击“框选无人机”，按住左键拖出矩形。普通框选替换选择，Shift 追加，Ctrl 切换；空白处短点击清空。主选无人机为黄色标记，其他选中无人机为青色标记。
2. 点击“集群移动”，再点击地图目标位置。目标是主选无人机的目的位置，其他无人机保持相对于主选无人机的位置与高度。
3. 检查 Route V2 预览航线，点击“确认集群移动”才向 Backend 提交执行。Esc 或“返回浏览”取消预览。
4. 移动时可在 MAP 暂停、继续或终止整组。已有安保方案或集群执行占用的无人机会显示占用原因；需要先完成或终止当前任务。

普通浏览保留原有地图手势，也可使用 Shift + 左键拖动追加框选。航线编辑、相机锁定和整体方案移动期间，集群输入让出控制权。

## 实现边界

- MAP 原生指针事件由 `FMapPointerInput` 转交 `UMapFleetInteractionWidget`，按下/拖动/释放只有一个输入所有者。离开地图、焦点丢失或丢失释放事件会取消当前手势。
- 框选只投影已注册的友方无人机。UI 使用同一套逻辑坐标处理矩形与投影，避免 DPI 重复换算。共享选择经过 Backend 确认，不修改 Video Target。
- 选点只创建 Route V2 预览。确认复用 `execution_group_move`，由 Backend 原子检查占用、轨迹和安全间距并创建执行组。主选 ID 放在成员列表首位，保持原队形与相对高度，不请求重新编队。
- Mock 位置来自 Backend 执行记录或配置的初始位置；真实无人机依旧要求有效位置遥测。
- MAP 新增的执行权限只覆盖集群移动及其暂停、继续、终止。MAP 不能创建/部署安保方案、启动方案执行，也不能控制方案执行记录；Video 不能发起执行。
- 活跃集群执行也参与无人机占用检查，防止方案分配与集群移动双向抢占。既有同方案规则不变。

## 验证

证据目录：`Evidence/FleetControl`。自动化、HTTP/WS 集成与原生鼠标操作分别记录。采用独立端口、任务拥有的 Backend/UE 进程和离线 Mock 地图，不替代真实飞行或 Cesium 网络瓦片验证。

- UE5.8 Development Editor 与 Backend Release 编译通过。
- Backend 相关测试 33/33 通过，其中包含新增的集群/方案占用、主选锚点与并发启动测试。全量 135 项中 133 项通过，保留历史失败 `SegmentDistanceTest.Crossing` 和 `AssemblyPlannerTest.ConflictHeightSeparation`。
- 最终交互自动化：4 项发现、4 项完成、4 项通过（含 1 项带环境警告）；覆盖框选、Backend ACK、Video Target 隔离、预览不执行、取消、整组启动/暂停/终止、释放和相机锁定。
- HTTP/WS 集成：7 个场景通过，包含 MAP 权限边界、不能抢占方案无人机、方案不能抢占暂停集群、幂等启动、四机保持相对队形到达目标以及采样安全间距不低于 1.5 米。
- 原生检查发现并修正了首次 Mock 缺少遥测时误报位置不可用，以及初始机模位置未从 Backend 同步造成预览起点偏移的问题。新增自动化断言直接比较每条预览起点与对应可见机模的位置。
- 保留了编译错误、测试夹具误用已部署方案、旧二进制权限拒绝和初次预览偏移等失败证据，没有调整安全阈值使测试通过。

具体报告见 `verification-1792117548148931200`。航线/监控/相机回归和最终原生证据在以下收尾记录中列出。


## 既有功能回归

`regression-final.log` / `ue-1792117548148502300`：6 项发现、6 项完成、6 项通过，均保留离线环境警告：

- Security Plan Route V2 的浏览、编辑、就绪、部署、执行、完成和整体移动生命周期。
- MAP 执行监控与执行组显示。
- P5.4 相机阻尼、3D 航线，以及独立相机锁定单元测试。
- MAP 未保存草稿保护。
- 中英文 FText 与草稿隔离。
- 跨方案占用禁用、清除本地旧勾选与完成后释放。

首次 Mock 位置对齐使用 `ExecutionUI::MockPosition`：有非预约执行记录时取最新 Backend 位置，否则取 Backend 配置的初始位置。MAP 机模、框选投影与目标预览统一读这份位置，不由客户端计时器推进运动。原生发现的偏移截图保留为 `native/02-preview-first-position-mismatch.png`，不作为修正后通过证据。

预览速度也取 Backend 的 `default_speed_mps`，使 Route V2 流动速度和图例与确认后的执行一致。最后的预览专项复测记录在 `preview-followup-final.log`。


## 最终原生检查

`native/03-final-box-selected-four.png`：真实鼠标拖框后，MAP 显示已选 4 架，四机标记与 Backend 位置一致。

`04-aligned-preview-eight-mps.png` / `06-preview-zh.png`：四条 Route V2 从对应可见机位出发，保留队形，图例为 Backend 默认 8 米/秒，英文/中文目标与确认文案正常。

`05-escape-cancelled.png` / `preview-cancel-no-execution.json`：真实 Esc 取消预览，Backend 执行数仍为 0。

`07-native-executing.png`、`08-native-paused.png`、`09-native-completed.png`：原生点击确认、暂停、继续；四机沿各自航线同步移动。只读 HTTP 快照 `paused-state.json` 验证 4/4 PAUSED，`completed-state.json` 验证 4/4 COMPLETED 且位置等于各自最终目标。

`10-native-aborted.png` / `aborted-state.json`：完成释放后再次原生确认移动，再点击终止；第二组 4/4 ABORTED，不再保留活跃占用。

以上执行操作均通过原生鼠标/键盘完成；HTTP 仅用于读取状态辅助取证。最终版本未通过 API 代替原生启动、暂停、继续或终止。

最终状态：本次功能在离线 Mock 范围验证通过。自动化 10 个独立测试全部发现并完成（7 项保留环境警告），预览速度专项追加复测 1/1 通过，HTTP 集成 7 场景通过。Shift/Ctrl 组合选择由自动化验证，原生仅验证普通框选；持续右键/中键 3D 手势、真实飞行及 Cesium 网络瓦片未在本次验证。历史 Backend 两项失败保留，其他进程与原有 Cesium 资产改动不纳入提交。

任务拥有的 QA Backend 和 UE 进程均在取证后停止。
