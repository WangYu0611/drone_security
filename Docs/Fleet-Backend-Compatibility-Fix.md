# 集群移动 ROLE_FORBIDDEN：启动器后端版本不一致

2026-10-17 在正常启动器中复现：Map 已加载集群移动 UI，但实际运行的后端是 `Backend/build/Release/DroneBackend.exe`，修改时间为 2026-09-28。其 SHA-256 为 `b2f0d9bcd6f902bac3acd9448a411e0eb9fc34c58e74f07380ab2c363e41829f`。Map 角色的 `execution_group_move` 请求返回 HTTP 403、`ROLE_FORBIDDEN`、`Role cannot perform this plan action`，界面显示“此客户端无权执行该操作”。当时三个客户端均 ONLINE / hydrated，未创建任何执行记录。

此前集群功能验证使用了较新的 `Backend/build-p54/Release/DroneBackend.exe`，与正常启动器默认路径不同。前端更新、后端默认产物未重编译，旧的 readiness 只检查业务快照形状，因而将旧服务当成可用服务。这是本次遗漏。

## 修正

- 重新编译正常路径 `Backend/build/Release/DroneBackend.exe`，通过原启动器重启 Backend；原 Command / Map / Video 自动重连，本机现已生效。
- Backend 的 `GET /` 声明 `fleet_move_3d`、`map_fleet_controls` 能力。启动器在启动客户端、复用已运行后端和持续健康检查时验证能力；不匹配时明确提示停止旧后端并运行 `SetupDroneSecurity.cmd`，不会再把旧服务显示为 READY。
- FleetControl / FleetControl3D / FleetControlUX 的当前验证与 QA 脚本使用与启动器相同的 `load_config`，不再硬编码阶段专用后端。历史已封存证据保持原样。
- Map 的权限边界保持 Backend 决定：允许自由集群移动及其暂停、继续、终止；不允许创建或启动 Security Plan，也不允许控制 Security Plan 执行；Video 不能下发集群移动。

Git 只提交源码和证据，本机 EXE / DLL 不随源码提交。其他电脑更新源码后仍需关闭旧进程、运行 `SetupDroneSecurity.cmd` 并重新启动。

## 验证

证据位于 `Evidence/FleetBackendCompatibility`，修改前源码基线为 `77a515932157785368fdf8b881d0e9b4cb12c4eb`。

| 检查 | 结果与边界 |
| --- | --- |
| 旧程序独立 Mock 重现 | 有效 Map 请求返回 403 / ROLE_FORBIDDEN，执行记录不变。保存原程序后，新的启动兼容检查也能拒绝该程序；诊断脚本只在独立 Mock 中继续探测其原始响应。 |
| 启动器测试 | 16 / 16 通过，包含快照正常但缺少能力时拒绝复用、不取得外部进程所有权、不创建客户端或业务数据，以及运行中服务不兼容时转为 DEGRADED。 |
| 默认后端协议验证 | 7 个场景通过：多选不切换 Video、角色限制、任务占用排他、Map 不可终止方案执行、四机幂等建组与相对队形、暂停期间占用、继续并完成后释放占用。另有最小请求从 403 变为 200 的直接对照。 |
| 原生鼠标 / 当前正常启动器 | 使用 CesiumWorld 的 Map 窗口切到 3D，四机从 30 米出发，鼠标设置 40 米并选点、确认。4 个 GROUP_MOVE / RelativeOffset 执行全部 COMPLETED，最终经纬高逐轴等于各自的目标；Video 状态前后相同。HTTP 仅 GET 取证，移动由鼠标确认发出。截图保留报错、预览、执行 ACK 与完成状态。 |
| 全量后端单元测试 | 137 项，135 通过、2 失败；`SegmentDistanceTest.Crossing`（距离得到 5，预期 0）与 `AssemblyPlannerTest.ConflictHeightSeparation`（未报告交叉冲突 / 未改变高度）。修改前已存在的 build-p54 测试程序对这两项给出相同失败，记录在 baseline XML。本次未改装配算法或放宽测试。FleetMove、P55Reservation 及执行相关测试均通过。 |

当前验证全部为 **Mock**；本次原生检查覆盖提交并完成 3D 移动，暂停 / 继续等控制为协议证据，不冒充本次原生鼠标证据。没有真实无人机执行证据。独立诊断进程全部停止，用户原有的正常启动器及其重启后的后端保持运行，四机均已完成，无活动执行。

重新验证正常配置：

```powershell
python Scripts/FleetBackendCompatibility/verify.py
```

该脚本使用独立 Mock 数据和 21980 / 21981 端口。若本机保留了旧 EXE 及其运行库，可用 `--legacy-backend <旧程序完整路径>` 验证兼容拦截和原始 403；这不是产品启动器的兼容绕过选项。
