# TASK-P5.4 — 交付与验收记录

当前软件实现包括三维路线、多机 Line 航迹、组控制、持久化定时任务、地图交互和共享 Mock/Real 执行架构。端到端执行证据为 **Simulation / Mock**；Real 部分为适配器契约、坐标计算和空机队接口门禁测试。没有连接、解锁或控制真实无人机。

最终结论及完整交付字段见文末。9 月 22 日未完成适配器时的评估保存在 `Evidence/TASK-P5.4/acceptance-before-adapter-20260922.md`，不代表本次最终源码状态。

## 基线与运行方式

- 工作树：`C:\Users\wy331\Documents\UE5DroneControl-p5-4`
- Branch：`feat/triple-screen-p5-4-3d-multi-uav`
- Base HEAD：`9f5dac4d398d5544529aa8cb34948c313d0f0c27`
- Protected main：`f6ed7f97fabc21c202333528b0cb030dd4b1b2fe`
- 独立工作树分阶段提交；未 push、merge、tag、reset、stash，未改写 P5.3 工作树。
- `DroneSecurityLauncher.cmd` 是本地手工入口。忽略的 `Launcher/config.local.json` 使用 P5.4 二进制、`Saved/Stage1P54`、19980/19981 端口；验收数据另存 `Saved/P54-QA` 或各次独立测试目录。
- 部署仍只激活配置；立即执行或定时执行是显式操作。Backend 持久状态是唯一权威，客户端独立同步。

## 已实现行为

**Map Camera。** 2D 浏览支持阈值区分的左键拖动、鼠标指向平面缩放；编辑时单击添加、航点拖动和地图手势分开。3D 支持右键旋转、中键平移、滚轮、双击聚焦及可配置 Pitch 限制。中心、距离和旋转通过目标状态与指数阻尼更新；2D/3D 保留观察区域。参数集中在 `Config/DefaultGame.ini` 的 `CommandMapCamera`。原生检查发现并修复了 GameAndUI 下丢失按键/起点、Y 方向错误及快速释放时过度惯性；现在释放后平滑收敛到目标，避免额外速度放大。

**3D Security Plan。** 沿用 `altitude`、`waitTime`、`segmentSpeed`，不创建第二套路线字段。增加 `altitude_reference`；缺省保留旧 Ellipsoid 含义。AGL 必须提供 `terrain_ellipsoid_m`，MSL 必须提供 `geoid_undulation_m`，未解析基准时 Backend 拒绝。原实现包含高度、悬停及 AGL/Ellipsoid 控件，但它们位于长滚动面板底部，新建点也没有自动选中；此前“面板支持”的描述与普通用户首屏可发现、可正常编辑的实际体验不一致。2026-09-26 原生 UI 复现和修复验收见 [Waypoint Inspector 修复记录](P5.4-Waypoint-Inspector-Fix.md)；新点击点默认加载碰撞表面以上 80 m，可配置。AGL 平移后重新采样，缺少表面或低于地表时阻止保存。运行快照一次性解析为椭球高度，编辑路线保留原基准。没有自动大地水准面服务或原生 MSL 选择器。

**Multi UAV Mission。** 现有 Mission 扩展 `assigned_uav_ids`，保留主成员 `assigned_uav_id`。任务中心路线与每架 UAV 的不可变执行航迹分离。Line 按首段方向生成固定横向槽位；Column、V、Grid、Custom 仅保留标识，执行时明确拒绝。中央最小间距默认 1.5 m，并考虑 UAV 半径与安全余量。连续分段相对运动检查涵盖接入、飞行、悬停、闭环和终点占用；运行时检查相邻观测之间的扫掠段，发现冲突则暂停/保持并返回冲突对和间距。适用非极区 10 km 内局部任务，不构成障碍规避或飞行安全认证。

**Group Control。** 共享选择支持列表 Ctrl/Shift、Map Shift 框选；主选择必须是成员。组移动使用锚点和相对偏移，目标空间不足时尝试紧凑 Line 或原子拒绝。组暂停、继续、中止沿用相同执行状态机。仅显式视频操作修改 Video Target；选择 UAV 不隐式换视频。原生修饰键手势验收边界见下表。

**Scheduled Mission。** 本地时间输入转为持久 UTC；显示倒计时，支持 Start Now、Reschedule、Cancel。Backend 重启保留快照和截止时间，正常停机不取消 SCHEDULED。实测等待两分钟后重启恢复，使用同一执行状态机启动。

**Command 2D Situation Map。** 复用已有组件展示 UAV、朝向、选择、任务路线、执行状态、告警和目标；保留平移、缩放、选择、Follow 及事件日志。选择通过共享上下文同步到独立 Map。已有目标/告警位置继续使用现有数据；不伪造未提供位置的告警坐标。

**共享执行架构。** `SecurityPlanStore` 中的同一个 Mission Controller 管理进度、航点、悬停、编队、冲突和调度，历史文件名 `mock_execution.inl` 保留。`security_mission::Command` 是统一 WGS84 椭球目标/速度模型。Mock Adapter 返回插值运动；Real Adapter 通过现有 DroneManager/NED 命令与遥测返回实测运动，不持有第二套任务循环。下发成功不等于到点。Real 重启后活动任务暂停待显式继续，失联/拒绝下发/保持失败可见，模式不匹配不下发。Real 模式阻止旧移动、编队、调试注入和注册表改写入口竞争控制权。UE 不用执行快照覆盖 Real 遥测。

Real 桥接使用显式 GPS/NED 对应点标定，不把首个 GPS 样本当作 PX4 零点；检查新鲜且有效遥测、Armed/Offboard 和锚点一致性。启用必须使用独立 Real 配置；样例默认禁用，没有开启硬件。详见 [P54-Real-Adapter.md](P54-Real-Adapter.md)。原有 38 个接口/转换文件中 36 个保持字节一致，DroneManager 两个文件仅增加只读新鲜遥测 accessor；新旧哈希分别保留，不再声称全部未改动。

## 自动化与协议证据

证据根目录为 `Evidence/TASK-P5.4`。每次执行保留独立文件，失败不被后来成功覆盖。

- Backend：`build/backend-16.xml`、`build/tests-16.log`，**122 discovered / 120 passed / 2 failed**。失败仍是继承的 `SegmentDistanceTest.Crossing`、`AssemblyPlannerTest.ConflictHeightSeparation`。
- 新适配器测试涵盖实测到点/悬停、暂停保持/继续、下发与保持拒绝、恢复暂停、模式隔离、发布失败保留已提交意图、整组故障、暂停中失联、实测交叉航迹及预检失败不下发；专门验证定时预检后遥测丢失原子失败。
- 坐标：Backend WGS84/ECEF/非零 NED 标定与 40/80/120/60 m 高度往返，异常域拒绝；UE 使用既有 Cesium 转换做高度和整组平移回归。旧转换链未替换。
- Launcher：`build/launcher-tests.log`，15 passed。
- P5.4：`adapter-protocol-1790414953245135300`，四机 HTTP/WS 8 项 + 实际重启/两分钟调度 2 项通过。73 个采样快照的最小观测间距为 2.556129809 m；采样结果不是连续碰撞证明，连续检查由 Backend 测试另行覆盖。
- Real 空机队：`real-adapter-gate-1790414963386493500`，Real hydration、旧控制入口拒绝、Mock 停机入口拒绝。没有 UAV/UDP 端口映射，没有传输硬件命令。此前误写接口路径产生的 404 失败仍保留。
- 继承协议：`regression-1790413551828344900`，P4/P5/P5.1/P5.2 分别 21/8/11/14 项通过。原始 P5.3 在 8 项后被新增安全规则拒绝：接入轨迹距停放 UAV 约 0.563 m，小于 1.5 m；原失败日志保留。
- 独立安全 P5.3 fixture：只将空闲 UAV 移出接入区域，未改原测试脚本；`p53-safe-1790414065427978800` 10 项通过。这不能表述成原 fixture 原样全过。
- UE：最终批次 `ue-regression-1790414494354954900`：9 discovered/performed，9 succeeded with warnings，0 failed，全部 exit 0；每项使用新 Editor 进程、有效发现数、结果文件及队列完成行。Map 测试额外包含两个独立执行组同时渲染、Real 快照不覆盖 Registry 位置/在线状态的副本测试。
- `build/ue-19.log`：UE 5.8 Development Editor 成功；`build/localization-04.log`：本地化生成。引擎缺少 GameFeatureData 规则、Cesium/source prepass/瓦片等环境警告保留，不宣称无警告启动。

9 月 22 日的所有成功、OOM/零结果、编译失败、DLL 被占用、坐标方向/过冲截图仍保留。零测试或没有结果的进程退出不计 PASS。原始阶段证据详见旧评估副本。

## Native A–L 验收矩阵

现有 Windows computer-use API 没有支持按住修饰键、右键或中键的拖动参数。这些未验证手势不能用协议或组件测试冒充物理操作。截图是原生采集；API 建立 fixture 与原生创建明确分开。

| 项 | 已取得证据 | 尚需完成的物理验收 |
|---|---|---|
| A 2D Camera | 9 月 26 日补完原生 Pan→Zoom→Pan→Zoom，正确方向/位移及静止终点；`native/resume-*.png` | 长时间操作与主观顺滑度仍需用户评审；旧中断记录保留 |
| B 3D Camera | 原生 3D 显示，Pitch/阻尼组件断言 | 右键 Orbit、中键 Pan 完整手势 |
| C 2D/3D | `native/map-3d.png`、`map-back-2d.png`；同区域往返 | 更广泛场景体验 |
| D 3D Route | 40/80/120/60 m API fixture、原生路线图、Cesium 往返 | 四点原生创建/编辑/保存完整路径 |
| E Hover | 5/10 s 等待数值与完成状态 | 完整原生屏幕观察 |
| F Multi UAV | 三机单元完成、四机协议完成、独立航迹 | 模型外形视觉不重叠 |
| G Safety | 连续预测、运行扫掠、冲突冻结/保持及数值间距 | 实际机体/障碍/飞控安全不在本次证据范围 |
| H Group | 共享多选、原子组移动与暂停/继续/中止 | Ctrl/Shift 列表与框选完整原生旅程 |
| I Conflict | 狭窄目标和交叉接入原子拒绝、结构化原因 | 原生冲突面板操作 |
| J Schedule | 实际 UTC +2 分钟执行 | 原生表单输入全过程 |
| K Restart | 实际 Backend PID 更换、同持久目录恢复 | 已通过记录中的隔离 Mock 场景 |
| L Command Map | 原生平移/缩放/选择/日志，独立 Map 同步 UAV04 | 完整告警/目标/Follow 旅程及远视图标签拥挤 |

`native/selection-before.json` / `selection-after.json` 验证主选择 UAV01→UAV04、四成员选择保留、Video Target 仍为 null/version 0。最新 Map 已支持所有活动执行组，且隐藏与新选草稿无关的历史监视面板；其组件测试不等于同时多组的原生视觉验收。

## 重跑与证据处理

- `python Scripts/P54/protocol_regression.py`：隔离继承协议，各失败如实返回。
- `python Scripts/P54/p53_safe_regression.py`：独立安全 P5.3 fixture。
- `python Scripts/P54/adapter_protocol_regression.py`：四机、真正重启、真正等待两分钟。
- `python Scripts/P54/real_adapter_gate.py`：空机队 Real 门禁，不激活硬件。
- `python Scripts/P54/ue_regression.py`：默认九项串行新进程 UE 回归；参数可筛选 Map 用例。
- `python Scripts/P54/qa_host.py`：原生 QA 生命周期，只停止已验证归属的任务进程；每次控制命令等待完成。
- `python Scripts/P54/package_evidence.py`：日志 JWT 脱敏；忽略目录 `Saved/P54-raw-evidence` 保留原始字节，manifest 记录原始/评审 SHA-256。Evidence 路径 Git 禁止文本换行转换，以保持审计哈希。

## P5.4 FINAL RESULT

**CONDITIONAL PASS — 限于已验证的软件/Mock 场景和 Real 适配器契约；完整原生 A–L 验收尚未全部完成。**

| Required field | Delivered state |
|---|---|
| Branch | `feat/triple-screen-p5-4-3d-multi-uav` |
| Base HEAD | `9f5dac4d398d5544529aa8cb34948c313d0f0c27` |
| Final HEAD | 最终文档/证据提交完整哈希在交付消息；本文件不能包含自身提交哈希。源码提交 `a14a6e8297d93433f0a14926f50b40f664ec4a3d`。 |
| Map Camera / 2D / 3D / Damping | 已实现；原生四步 pan/zoom 完成，3D 显示与区域保持；右键/中键物理手势未验证。 |
| 3D Security Plan / Altitude / Hover / 3D Path | 复用字段；显式高度基准、悬停和三维路径；高度与 5/10 s 等待自动化通过。 |
| Multi UAV Mission / Assigned UAV / Formation / Safety Separation | 多成员分配、独立不可变 Line 航迹、集中 1.5 m + 半径/余量、连续局部冲突检查。 |
| Group Control / Multi Select / Group Move / Conflict Detection | 共享多选、偏移保持、紧凑布局或原子拒绝及组生命周期；原生修饰键/框选尚未验收。 |
| Scheduled Mission / Persistence | 本地输入转 UTC，立即执行/改期/取消，重启保留；最终首次观测 EXECUTING 距截止 482 ms。 |
| Command 2D Situation Map | 复用组件，状态/朝向/路线/告警/目标、选择/Follow/日志；已观察跨客户端选择且不修改视频目标。 |
| Coordinate Conversion Regression | 既有 WGS84/UE/Cesium 链通过；新增显式标定 NED 往返通过；36 个接口文件未变，2 个仅增加只读 accessor。 |
| Backend Tests | 122 discovered，120 passed，2 inherited failures；Launcher 15 passed。 |
| Protocol Tests | P5.4 8+2；P4/P5/P5.1/P5.2 21/8/11/14；安全 P5.3 fixture 10；原 P5.3 在 8 项后安全拒绝；空机队 Real 门禁通过。 |
| UE Tests | 9/9 succeeded with warnings，0 failed，独立新进程、非零发现数及完成行。 |
| Native Tests | 上表 A–L 分项记录，局部完成；不是全项原生 PASS，没有实机/实视频验收。 |
| Known Issues | 两项继承 Backend 失败；原 P5.3 fixture 触发新增安全拒绝；远距标签拥挤；Cesium/插件/引擎警告及 3D Lumen 曝光警告；AGL 依赖已加载碰撞表面；局部近似/固定编队朝向。 |
| Deferred | 完整原生 A–L 旅程、模型视觉间距、多组原生视觉验收；面板 Collapse 和完整 geofence/目标 UX；自动 geoid；非 Line 模式；感知空间布局/自主避障；真实飞控与视频重新联调。共享 Real 适配器已实现，未列为未完成开发。 |
| git status | 最终证据提交后检查为 clean；main/P5.3 保护身份单独复核。 |

最终 Backend 二进制为 `build/backend-16.log`；新定时遥测异常由 `backend-16.xml` 覆盖，最终四机/重启协议在该二进制上重跑。九项 UE 使用最终前端源码和 Backend 前一轮二进制；两轮 Backend 唯一差异为定时启动时遥测丢失的原子异常处理，正常 Mock 路径已由最终协议重跑覆盖。

本次原生操作保留 `native/resume-observation.json`、截图及独立 `resume-logs`。四步平移/缩放和三维区域保持已观察；末次返回 2D 的截图处于阻尼过渡，不冒充稳定终点。3D 出现 Lumen cached-lighting exposure clipping 警告，保留原图和日志。QA 和本次新建辅助进程已停止，既有辅助进程未终止。
