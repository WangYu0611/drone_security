# UE5DroneControl 项目代码学习计划（当前主线）

> 更新时间：2026-09-09
> 学习顺序按当前代码入口整理。旧 UDP 直连代码可以用于理解迁移背景，但不应作为当前控制协议的实现依据。

## 0. 先建立边界

```text
UE 5.7
  ├─ 交互、Cesium/UE 世界坐标、Registry、UI、路径和本地预演
  └─ HTTP/WS ── DroneBackend
                  ├─ 注册表、状态、队列、集结、执行、坐标
                  └─ UDP JSON v1 / YAML + control_ack ── Jetson/PX4
```

当前最重要的三个不变量：

1. UE 的相对偏移单位是 cm；后端内部和 JSON `target` 是 NED m。
2. 每架无人机有独立的 `power_on_origin`；多机阵列不能复用一个偏移。
3. `command_ack` 只代表后端接受；机载应用确认要看 YAML `control_ack`。

## 1. 推荐入口

按以下顺序阅读：

1. `Docs/README.md`
2. `Docs/PROJECT_FACTS.md`
3. `Docs/架构设计.md`
4. `Docs/接口与通讯数据规范.md`
5. `Docs/坐标系转换说明.md`
6. `Config/DefaultGame.ini`、`Config/DefaultEngine.ini`
7. `Source/UE5DroneControl/DroneOps/Network/DroneNetworkManager.*`
8. `Source/UE5DroneControl/DroneOps/Core/DroneRegistrySubsystem.*`
9. `Source/UE5DroneControl/DroneOps/Control/DroneOpsPlayerController.*`
10. `Source/UE5DroneControl/UI/SequenceDispatchPanelWidget.*`
11. `Source/UE5DroneControl/PathEditor/`
12. `Backend/main.cpp` 和 `Backend/http/http_server.cpp`
13. `Backend/drone/`、`Backend/execution/`、`Backend/conversion/`、`Backend/communication/`

## 2. 阶段一：UE 工程和运行入口

目标：知道模块、插件、GameMode 和网络配置从哪里来。

阅读：

- `UE5DroneControl.uproject`：工程绑定 UE 5.7，插件开关。
- `Source/UE5DroneControl/UE5DroneControl.Build.cs`：模块依赖。
- `Source/UE5DroneControl/DroneOps/Control/DroneOpsGameMode.*`：Cesium、坐标服务、离线地图、Actor 启动。
- `Config/DefaultGame.ini`：后端 URL、轮询、地理高程修正。
- `Config/DefaultEngine.ini`：Cesium tile server、代理、引擎设置。

练习：启动编辑器，记录 `Saved/Logs/UE5DroneControl.log` 中的 GameMode、网络配置和 Cesium 相关日志。静态阅读不等同于 Play Mode 通过。

## 3. 阶段二：Registry 和数据模型

目标：区分身份、遥测、任务和 UE-only 状态。

阅读：

- `DroneOps/Core/DroneOpsTypes.h`
- `DroneOps/Core/DroneRegistrySubsystem.h/.cpp`
- `DroneOps/Drone/DroneTelemetryComponent.*`
- `DroneOps/Drone/DroneSelectionComponent.*`

重点理解：

- `FDroneDescriptor` 的 `DroneId/BackendIdString/Slot/VideoUrl`。
- `FDroneTelemetrySnapshot` 的可用性、电量、GPS、`bLocalPositionValid` 和任务字段。
- `FDroneTaskStateSnapshot` 与后端 `drone_task_state` 的对应关系。
- `EnemyIdBase` 以上的敌方目标是 UE-only，不得发给后端。
- 多选、控制锁和本地巡逻预演状态不属于后端事实。

## 4. 阶段三：HTTP + WebSocket

目标：跟完一条“初始化 → 拉列表 → WS 遥测 → 用户控制”的闭环。

阅读：

- `DroneOps/Network/DroneHttpClient.*`
- `DroneOps/Network/DroneWebSocketClient.*`
- `DroneOps/Network/DroneNetworkManager.*`

重点函数：

| 函数/字段 | 作用 |
|---|---|
| `Initialize()` | 读取 ini，创建客户端，启动轮询和 WS |
| `PollDroneList()` | `GET /api/drones` |
| `OnWsMessage()` | 解析 telemetry/event/alert/task 等 |
| `SendMoveCommand()` | 发送 `mode` 目标和速度 |
| `SendPauseCommand()` | 发送 pause/resume |
| `SendArrayTaskFromData()` | 组装 `POST /api/arrays` |
| `SetStrictLocalPreviewIsolation()` | 停止/恢复所有后端通信 |

练习：使用 `wscat` 或 `tools/mock_ue/`，观察 `command_ack` 和 `telemetry`。明确记录 mock、真实后端和真实 Jetson 三种证据不能互换。

## 5. 阶段四：坐标、锚点和镜像机

目标：理解相对坐标如何放回 Cesium 世界，以及为什么多机不能共享偏移。

阅读：

- `DroneOps/Core/ICoordinateService.*`
- `DroneOps/Core/SimpleCoordinateService.*`
- `DroneOps/Core/CesiumCoordinateService.*`
- `RealTimeDroneReceiver.*`
- `MultiDroneCharacter.*`
- `Docs/坐标系转换说明.md`

跟踪：

```text
WS event(power_on/reconnect)
  -> CachedGpsAnchor
  -> Cesium Georeference / AnchorWorldLocation
  -> telemetry 相对偏移
  -> 镜像 Actor 世界位置
```

控制反向链路：

```text
地图世界点 - 目标机 AnchorWorldLocation
  -> UE 相对 cm
  -> WebSocket mode
  -> 后端 UeOffsetToNed
  -> UDP JSON target NED m
```

重点检查 `local_position_valid`：有效本地 NED 才能用于控制/到达判断；GPS 经纬度不替代本地位置。

## 6. 阶段五：输入和任务派发

目标：理解从点击、选择到控制/阵列请求的所有守门条件。

阅读：

- `DroneOps/Control/DroneOpsPlayerController.*`
- `UI/DroneListWidget.*`
- `UI/DroneInfoPanelWidget.*`
- `UI/SequenceDispatchPanelWidget.*`
- `PathEditor/DronePathActor.*`
- `PathEditor/DronePlaybackManager.*`

重点：

- 离线、控制锁、严格本地预演会阻止什么。
- `move` 的世界坐标何时减锚点、速度如何传递。
- `auto_assign` 时 path→drone 映射何时由后端返回。
- `assembling` 和 `assembly_complete` 如何控制在线真机影子机。
- 循环路径、停止、路径 Actor 清理的生命周期。

当前高风险点：阵列路径的某些重映射仍可能把 Cesium/UE 世界坐标直接送入后端相对偏移接口；外场学习/验证时优先跟踪这一条。

## 7. 阶段六：后端主线

目标：从 REST/WS 入口一路跟到 UDP JSON v1 和 YAML 回传。

阅读顺序：

1. `Backend/main.cpp`
2. `Backend/core/config_loader.*`
3. `Backend/http/http_server.*`
4. `Backend/drone/drone_manager.*`
5. `Backend/drone/state_machine.*`
6. `Backend/drone/command_queue.*`
7. `Backend/drone/heartbeat_manager.*`
8. `Backend/communication/udp_sender.*`
9. `Backend/communication/udp_receiver.*`
10. `Backend/conversion/coordinate_converter.*`
11. `Backend/conversion/gps_anchor_manager.*`
12. `Backend/execution/assembly_controller.*`
13. `Backend/execution/execution_engine.*`

跟踪两条链路：

```text
UE WS mode target
  -> HttpServer 校验
  -> DroneManager 入队
  -> CoordinateConverter
  -> HeartbeatManager/UdpSender
  -> JSON v1 重发
  -> Jetson 3-of-5
  -> YAML control_ack
```

```text
Jetson YAML
  -> UdpReceiver
  -> local_position / GPS / ACK
  -> DroneManager 状态与锚点
  -> HttpServer WS telemetry/event/alert
  -> UE Registry
```

## 8. 阶段七：测试和证据

本地后端：

```powershell
curl.exe http://127.0.0.1:8080/
cmake --build Backend\build --config Release --target DroneBackend_tests
.\Backend\build\Release\DroneBackend_tests.exe
```

无实机：使用 debug inject、`tools/mock_ue/`、集成脚本，结论写为“后端/协议模拟验证”。

有实机：使用 `Docs/后端无人机通讯外场测试清单.md`，分别记录：

- 后端收到请求；
- UDP JSON 发出；
- Jetson 收到并通过 3-of-5；
- YAML `control_ack` 回传；
- PX4 ARM/OFFBOARD/位置实际结果；
- UE Play Mode、Cesium、WebRTC 和跨机网络证据。

## 9. 六周节奏

| 周期 | 结果 |
|---|---|
| 第 1 周 | 能启动 UE 5.7，找到 GameMode、配置和日志 |
| 第 2 周 | 能解释 Registry、遥测快照、选择和控制锁 |
| 第 3 周 | 能用 mock 后端完成 HTTP/WS 初始化和列表同步 |
| 第 4 周 | 能解释 GPS/Cesium 锚点和 cm↔NED 转换 |
| 第 5 周 | 能跟完点击 move、pause/resume、阵列提交 |
| 第 6 周 | 能独立跑后端测试并按证据层级分析外场问题 |

## 10. 反模式

- 不要把 `BackEnd`、旧 24 字节包、旧 Python 后端当成当前主线。
- 不要把 `VideoUrl`、日志或持久化 `drones.json` 当成在线/可执行证明。
- 不要把 GPS 经纬度直接当 NED 位置。
- 不要把 UE 世界坐标直接当后端相对偏移。
- 不要用 prompt/retry 解决本应由坐标、状态和协议边界解决的问题。
