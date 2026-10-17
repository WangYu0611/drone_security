# MAP 3D 集群移动

基线：`1e31715ceb7d3f35f1819c9fe6dfdc503d15c2cf`。在 `D:/CodexPublish/drone_security` 的 main 上扩展已有框选与保持队形移动。

## 操作

1. 切换 MAP 到 3D，点击“框选无人机”，左键拖框选择至少两架。黄色标记是主选锚点。
2. 点击“集群移动”，在已加载的实际地表上选取目标；天空和未加载地表不能伪造落点。2D 继续使用锚点当前飞行平面选取水平位置。
3. 右侧默认设置“目标离地高度”，也可通过高级按钮切换为椭球高。点击实际地表后，系统按该处已加载地表高度换算目标椭球高；“当前高度”恢复有效的当前离地高度。旧模拟位置低于地表时，先预览并确认垂直抬升。2026-10-17 的更新与在线地形证据见 [地表高度修正](MAP-Fleet-Terrain-Height.md)。
4. 检查四条 Route V2 立体航线、目标锚点标记和高度参考线。修改高度不改变已选经纬度。其余成员保持相对于主选锚点的水平偏移与高度差，整体平移到目标。
5. 点击“确认集群移动”才提交 Backend。移动期间可暂停、继续或终止整组。Esc 或“返回浏览”取消尚未提交的预览。

2D 保留当前高度移动。切换 2D/3D 会取消旧目标与预览，需要重新选点；不会沿用另一视图中未确认的目标。原有安保方案占用、Camera Lock 和 Video Target 规则继续生效。

## 实现

- 3D 高度仅在集群移动模式显示；数值变化重建已有 Route V2 预览，不创建执行记录。±10 米按钮显式刷新预览，避免依赖程序设置文本是否触发输入事件。
- 严格校验完整数字文本、有限值以及与锚点当前高度不超过 10 km 的差值，防止极端输入先进入坐标/样条计算。10 km 来自既有 Backend 本地坐标域，并不是新增的飞行安全范围；完整三维距离、无人机占用与安全间距仍由 Backend 原子复核。
- 继续复用 `execution_group_move`，目标含经纬度与高度；不新增另一套客户端执行引擎。Backend 既有逻辑支持三维相对队形，新增测试覆盖分层队形和纯升降。
- 路径主体仍由 `ADronePathActor / DronePathVisual / M_CommandPathV2` 显示；高度参考线是临时 UI 标记，不是另一条执行路径。确认 ACK 后撤销预览所有权。
- 未修改相机锁定、相机阻尼或真实飞行适配器。

## 复现与证据

`python Scripts/FleetControl3D/verify.py` 为每组 UE 测试启动独立 Editor，并检查发现、完成和通过数量；使用独立 Mock Backend。协议验证设置 `FLEET_ALTITUDE_DELTA=50`，同时保留原有 2D 协议脚本的默认行为。

`python Scripts/FleetControl3D/qa_host.py` 是原生检查宿主，仅管理任务自己的本地 Mock Backend 与 MAP 进程。运行前在 `Saved/FleetControl3D-QA/command.json` 写入 `{"action":"start","role":"Map"}`；收尾写入 `{"action":"stop"}`。原生输入使用 Computer Use，HTTP 快照只做只读取证。

证据集中保存在 `Evidence/FleetControl3D`，不覆盖此前的 `Evidence/FleetControl`。首轮失败日志保留：宽松数字解析接受尾随字符、程序设置输入值未触发预览事件、相机切换尚未稳定时比较位置，以及测试名称前缀造成重复发现。分别修正解析、按钮事件和测试夹具；相机比较容差与安全间距阈值没有放宽。

## 自动化结果

- UE5.8 Development Editor 编译通过，Backend Release 编译通过。
- Backend 相关测试 **35/35 通过**，包含三维分层队形与纯升降。全量 **137 项中 135 项通过**，历史失败 `SegmentDistanceTest.Crossing`、`AssemblyPlannerTest.ConflictHeightSeparation` 仍保留，未计作通过。
- `verification-1792130424600612300`：**5 项发现、5 项完成、5 项通过**；其中 3D、2D 两项交互测试带既有离线环境警告。覆盖射线选点、严格高度解析、超大有限高度拦截、保持经纬度的高度编辑、四机立体预览、切换视图失效、整组暂停/终止、释放及两个相机锁定测试。
- 同目录协议测试 **7 个场景通过**。四机以主选 UAV-02 为锚点，目标包含 +50 米高度变化；逐项核对目标快照与最终三维位置，保留 MAP 权限、占用冲突、幂等、暂停/继续与安全间距检查。
- 中英文 9 个新增键均生成到 locres，中文 UI 原生显示正常。GatherText 内部步骤返回 0，但进程因既有 `GameFeatureData` Asset Manager 配置错误返回 1；原始日志保留，不宣称整个本地化命令无错误。

## 原生结果

采用 Computer Use 真实鼠标/键盘操作，Backend 为本地 Mock。HTTP 只读快照用于核对结果，不代替操作：

- `native/01-box-four-in-3d.png`：3D 模式左键拖框，4 架选择已同步。
- `03-invalid-height-disabled.png` / `invalid-height-no-execution.json`：键盘输入 `90oops`，确认禁用，执行数仍为 0。
- `04-80m-3d-preview-en.png`、`05-80m-3d-preview-zh.png`、`06-80m-preview-maximized.png`：输入 70 米后点击 +10 米，得到 80 米立体预览；目标经纬度保持不变，路线高度图例 30–80 米。
- `07-view-change-clears-preview.png`：真实点击切换至 2D 后旧预览消失，并提示重新选点。
- `09-native-3d-executing.png`、`10-native-3d-paused.png`、`11-native-3d-completed-80m.png`：重新选点并原生确认，整组暂停、继续。`paused-state.json` 为 4/4 PAUSED，高度均为 72.38085809966343 米；`completed-state.json` 为 4/4 COMPLETED，高度均为 80 米，三维位置等于各自航线终点。

首轮原生检查还发现英文高度恢复按钮文字被截断（`02-first-height-controls.png`），因此将按钮缩短为 `Current / 当前高度`。功能测试和上述升高执行取证使用 build-3；后续仅调整这个按钮文案并重新编译为 build-4。

最终 build-4 原生复查：`12-final-height-controls-zh.png` / `13-final-height-controls-en.png` 两种语言按钮文字完整。重新读取完成后 80 米的 Backend 位置，点击三次 -10 米得到 50 米下降预览（`14-final-descent-preview-50m.png`）；原生确认后 4/4 COMPLETED，最终高度 50 米且三维位置等于终点（`15-final-descent-executing.png`、`16-final-descent-completed-50m.png`、`descent-completed-state.json`）。

这些证据仅验证离线 Mock 流程。真实飞行、Cesium 在线地形/离地高度、持续右键/中键 3D 相机手势不在本次原生验证范围，均为 **NOT NATIVE VERIFIED**。相机锁定行为保持原状。原有 `CesiumIonSaaS.uasset` 本地修改不纳入本次提交。

任务拥有的 Backend 与 MAP 进程在取证后停止。`manifest.json` 保存最终二进制与证据文件 SHA-256；`tested-build-3.json` 保留自动化和升高原生检查时的二进制哈希。证据封存脚本为 `Scripts/FleetControl3D/seal.py`。
