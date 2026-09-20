# 项目现状基线（代码核对版）

> 审计日期：2026-09-09
> 适用仓库：`D:/RedAlert/UE5DroneControl`
> 事实优先级：当前源码与配置 > 可执行测试/运行日志 > 本目录其他说明文档。
> 本文件只记录已在仓库中找到实现或配置依据的事实；“待验证”不等于“已完成”。

## 1. 当前技术基线

- UE 工程：Unreal Engine 5.7，工程关联见 `UE5DroneControl.uproject`，Target Rules 使用 UE 5.7 的 Include Order/Build Settings。
- UE 模块：`Source/UE5DroneControl/`，核心运行时网络入口是 `UDroneNetworkManager`（`UGameInstanceSubsystem`）。
- 后端：`Backend/`，C++17/CMake，进程名通常为 `DroneBackend.exe`。
- 后端入口：`Backend/main.cpp`；装配顺序为配置加载、UDP 发送/接收、心跳、`DroneManager`、`AssemblyController`、`ExecutionEngine`、HTTP/WS 服务。
- Jetson/PX4 桥接脚本不在本仓库的 `Backend/` 或 `Source/` 中；真实飞控验证必须使用与当前 JSON v1/YAML 协议匹配的外部 Jetson 版本。
- 当前仓库包含 `Backend/data/drones.json` 示例注册数据，但它是运行时持久化文件，不是无人机真实在线证明。

## 2. 当前运行拓扑

```text
UE5
  ├─ HTTP GET/POST/PUT/DELETE ───────────────┐
  ├─ WebSocket telemetry/event/control ──────┤
  │                                          ▼
  │                                  DroneBackend
  │                              ┌──────┴────────┐
  │                              │ HTTP/WS       │
  │                              │ DroneManager  │
  │                              │ Heartbeat     │── UDP JSON v1 ──► Jetson/PX4
  │                              │ Assembly      │◄─ UDP YAML ────── Jetson
  │                              │ Execution     │
  │                              └───────────────┘
  │
  └─ browser WebRTC video_url ───────────────► MediaMTX（独立视频服务）
```

职责边界：

- UE 负责交互、Cesium/UE 世界坐标、注册表镜像、路径和阵列编辑、影子机预演、视频窗口以及向后端发起请求。
- 后端负责注册表持久化、slot/端口映射、状态机、GPS 锚点、坐标转换、队列、重发、集结、任务执行、基础避障和协议转换。
- Jetson 负责接收控制 JSON、进行 3-of-5 应用确认、向 PX4 维持 Offboard setpoint，并把遥测与 `control_ack` 回传。
- PX4 负责飞控执行。后端收到 UDP 成功发送、WS `command_ack` 或 UI 成功回调，都不能单独证明 PX4 已执行。

## 3. 端口与配置事实

### 3.1 后端

配置文件：`Backend/config.yaml`。

| 用途 | 当前值 |
|---|---:|
| HTTP | `0.0.0.0:8080` |
| WebSocket | `0.0.0.0:8081`，客户端统一使用 `/ws` |
| slot 1 遥测接收 | `8888` |
| slot 2 遥测接收 | `8890` |
| slot 3 遥测接收 | `8892` |
| slot 4 遥测接收 | `8894` |
| slot 5 遥测接收 | `8896` |
| slot 6 遥测接收 | `8898` |

每个 slot 的控制发送端口分别是 `8889/8891/8893/8895/8897/8899`。实际目标地址优先取 `port_map[slot].host`；注册表中的 `ip/port` 会保存并返回，但不能代替后端启动时的 slot 控制映射。

当前关键参数：`max_count=6`、`heartbeat_hz=5`、`command_repeat_count=5`、`lost_timeout_sec=10`、`arrival_threshold_m=1.0`、`assembly_timeout_sec=60`、`avoidance_radius_m=3.0`、`avoidance_lookahead_sec=2.0`、`assembly_safety_cylinder_m=2.0`、`low_battery_threshold=20`。

从仓库根目录启动时，注册数据路径是 `./Backend/data/drones.json`，日志是 `./logs/backend.log`，视频元数据根目录默认是 `D:/DroneData/metadata`。

### 3.2 UE

`Config/DefaultGame.ini` 当前默认连接：

- HTTP：`http://127.0.0.1:8080`
- WebSocket：`ws://127.0.0.1:8081/ws`
- 注册表轮询：3 秒
- 地理目标高程修正：`GeoidSeparationMeters=0.0`

`Config/DefaultEngine.ini` 当前在线 HTTP 代理为 `127.0.0.1:7890`，并把 `127.0.0.1/localhost/::1` 列入直连。代理端口是本机环境事实，换机器必须重新确认。

### 3.3 MediaMTX

`tools/MediaMTX/mediamtx.yml` 当前用于本地演示：RTSP `:8554`、WebRTC `:8889`、路径 `drone-1`/`drone-2`/`drone-3`。UE 视频窗口使用浏览器页面 `http://<mediamtx-host>:8889/drone-N`，不是 `/whep` 原始接口。

## 4. REST 路由事实

实现位置：`Backend/http/http_server.cpp`。成功/错误响应由后端直接生成；错误主体当前是 `{"detail":"..."}`，不能依据旧文档假设统一存在 `error` 字段。

正式路由：

| 方法 | 路径 | 作用 |
|---|---|---|
| GET | `/` | 返回端口和 debug 状态 |
| GET | `/api/drones` | 返回注册表数组及状态、遥测、任务快照 |
| POST | `/api/drones` | 注册无人机，分配 `dN` ID，校验 slot 唯一性 |
| PUT | `/api/drones/{id}` | 更新 name/model/ip/port/video_url，不更新 slot |
| DELETE | `/api/drones/{id}` | 删除注册记录和对应后端上下文 |
| GET | `/api/drones/{id}/anchor` | 读取有效 GPS 上电锚点；没有锚点时返回 425 |
| POST | `/api/drones/refresh` | 给 offline/lost 无人机发安全 hold 探测，不直接改为 online |
| POST | `/api/video-metadata/batch` | 幂等写入 Jetson 视频元数据 JSONL 批次 |
| POST | `/api/arrays` | 创建集结/阵列任务 |
| POST | `/api/arrays/{id}/stop` | 停止阵列和执行引擎 |

仅 `server.debug=true` 时启用的 debug 路由：

`GET /api/debug/drone/{id}/state`、`GET /api/debug/drone/{id}/queue`、`GET /api/debug/heartbeat/{id}`、`POST /api/debug/drone/{id}/inject`、`POST /api/debug/cmd/{id}/move`、`POST /api/debug/cmd/{id}/pause`、`POST /api/debug/cmd/{id}/resume`、`POST /api/debug/cmd/{id}/array`、`POST /api/debug/cmd/{id}/target`、`POST /api/debug/cmd/batch/array`、`GET /api/debug/arrays/{id}/state`。

## 5. WebSocket 事实

连接地址：`ws://<backend-host>:8081/ws`。

UE → 后端：

- 目标指令必须使用 `mode` 字段：`move`、`scout`、`patrol`、`attack`；字段为 `drone_id`、`x/y/z`、可选 `speed`、可选两个 coalesce 标志。
- `x/y/z` 是目标机本地上电锚点下的 UE 相对偏移，单位 cm。
- `speed` 是 3D 速度 m/s；0 或非有限值表示 legacy unlimited marker，有限值在后端归一化到 `[0.5,15]`。
- 暂停/恢复使用 `{type:"pause"|"resume", drone_id 或 drone_ids}`。
- 旧的 `{type:"move"}` 会收到错误，必须改用 `mode`。

后端 → UE：`telemetry`、`event`（`power_on`/`lost_connection`/`reconnect`）、`alert`、`assembling`、`assembly_complete`、`assembly_timeout`、`assignment_result`、`drone_task_state`、`command_ack`、`error`。

`command_ack` 只表示后端接受/入队；真正的机载应用确认来自 UDP YAML 的 `control_ack`。

## 6. UDP 事实

### 6.1 后端 → Jetson：JSON v1

控制报文根字段：`protocol=ue5_drone_control`、`version=1`、`type=control`、`session_id`、`command_id`、`sequence`、`drone_id`、`slot`、`mode`、`issued_at_unix_s`、`sent_at_unix_s`、`target`、`delivery`，有限速度时另有数值 `speed`，无限速时为字符串 `"NaN"`。

`target` 固定为：

```json
{"frame":"NED","reference":"power_on_origin","unit":"m","north":0,"east":0,"down":0}
```

`delivery.repeat_index` 从 1 开始，`repeat_total` 对 move 默认 5，对持续 hold 为 0。后端会话、命令 ID、sequence、slot、目标和重复索引共同参与 Jetson 侧的去重与防旧包判断。

### 6.2 Jetson → 后端：YAML

后端实际解析：`timestamp`、`position[N,E,D]`、`q[w,x,y,z]`、`velocity`、`angular_velocity`、`battery`、`gps_lat/gps_lon/gps_alt/gps_fix`、`local_position`、`local_position_valid`、`arming_state`、`nav_state`、`control_ack`。

当 `local_position_valid=true` 且 `local_position` 有效时，后端用它作为控制/到达判断的 NED 位置；不能用不在当前报文中的字段替代它。`control_ack` 包含 `session_id`、`command_id`、`sequence`、`mode`、`confirmed_packets`，可选 `applied_at_unix_s`。

## 7. 坐标与状态不变量

```text
UE 相对偏移(cm)  ──×0.01/符号转换──►  NED [North, East, Down](m)
NED [North, East, Down](m)  ────────►  UE [X, Y, Z](cm)
```

当前实现公式：

```text
N = UE_X × 0.01
E = UE_Y × 0.01
D = -UE_Z × 0.01
UE_X = N × 100
UE_Y = E × 100
UE_Z = -D × 100
```

每架无人机有独立的上电 NED 原点。GPS 锚点用于 UE/Cesium 地理定位和重建原点；`gps_*` 当前遥测不能直接替代本地 NED 位置。期望目标、实际遥测、hold 目标必须分开保存。

当前明确的阵列风险：`SequenceDispatchPanelWidget` 的部分阵列重映射仍可能把 Cesium/UE 世界坐标直接送入后端的“相对偏移”入口；多机还存在各自上电原点不同的问题。单机实时 move 的入口契约已明确，但阵列外场使用前必须完成每架机的原点补偿和专项验证。

## 8. 文档维护规则

- `架构设计.md`、`接口与通讯数据规范.md`、`坐标系转换说明.md` 是当前核心基线，改代码时必须同步检查。
- 路由、JSON/YAML 字段和端口以 `Backend/http/http_server.cpp`、`Backend/config.yaml`、`Source/.../DroneNetworkManager.cpp` 为准。
- `README.md`、历史测试手册、规划文档中的旧 Python 桥接、24 字节控制包和 `BackEnd` 路径不能覆盖当前 C++ 后端事实。
- “代码存在”只证明静态存在；真实 Jetson/PX4、UE Play Mode、WebRTC 和跨机网络仍需单独记录运行证据。
