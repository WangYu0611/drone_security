# UE5DroneControl 需求基线与实现状态

> 更新时间：2026-09-09
> 本文把历史需求整理为当前可执行的边界。它不是替代协议的自由文本；接口字段以《接口与通讯数据规范》、行为以源码和配置为准。
> 状态含义：`代码存在` 只代表静态实现；`待运行验收` 需要 UE 5.7 Play Mode、后端运行日志或外部 Jetson/PX4 证据。

## 1. 产品目标

构建一个基于 UE 5.7 + Cesium 的无人机集群指挥与预演工具：用户在 UE 中查看无人机、编辑航点/阵列、进行本地预演，并通过 HTTP/WebSocket 请求后端控制。后端负责注册表、状态、坐标、队列、集结和任务执行；Jetson/PX4 负责机载控制和飞控执行。

职责不能混淆：

- UE 负责交互、世界坐标、可视化、影子机/镜像机和视频窗口。
- C++ 后端负责事实、持久化、协议转换和完成状态。
- Jetson/PX4 不在当前仓库中；真实飞行是外部依赖和单独验收项。

## 2. 当前功能需求

### 2.1 注册和状态

- 后端支持 `GET/POST/PUT/DELETE /api/drones`。
- 注册时 slot 为 1～6 且唯一，后端分配 `dN` 标识。
- 列表应包含注册字段、连接/电量/位置/任务摘要。
- UE 通过 `UDroneNetworkManager` 每 3 秒轮询并同步 `UDroneRegistrySubsystem`。
- `POST /api/drones/refresh` 只对 offline/lost 发送安全 hold 探测，不直接伪造在线状态。

### 2.2 实时控制

- UE 发送 WebSocket `mode` 目标：`move/scout/patrol/attack`。
- 目标坐标为目标机上电原点下的 UE 相对偏移，单位 cm。
- 后端转换为 NED m，发送 UDP JSON v1；move 默认 5 次重发。
- pause/resume 支持一个或多个 `drone_ids`。
- 后端广播 `command_ack` 仅表示后端接受；Jetson YAML `control_ack` 才是机载应用确认。

### 2.3 遥测和状态

- Jetson 通过 UDP YAML 上报本地位置、姿态、电量、GPS、arming/offboard 和 `control_ack`。
- 后端根据有效 `local_position` 做控制/到达判断，并转换为 UE 相对遥测。
- 首个有效 GPS 产生 `power_on` 锚点；重连可产生新锚点并推送 `reconnect`。
- 10 秒无遥测进入失联；具体超时和告警以 `Backend/config.yaml` 为准。

### 2.4 阵列任务

- `POST /api/arrays` 接受 `scout/patrol/attack`、路径、waypoint、闭环、速度和等待时间。
- 后端先集结，广播 `assembling`；全部满足阈值后广播 `assembly_complete` 并执行。
- 超过默认 60 秒广播 `assembly_timeout`，任务状态转 error。
- `auto_assign=true` 时后端计算 path→drone 并广播 `assignment_result`。
- `drone_task_state` 是 UE 显示当前任务/航点/错误的实时状态。
- `/api/arrays/{id}/stop` 停止后端阵列；UE 同时清理本地播放对象。

### 2.5 UE 预演

- `ADronePathActor`、`ADronePlaybackManager` 支持路径保存、加载、回放、循环和停止清理。
- 在线真机路径应等待集结完成；离线/mock 路径可以本地立即预演。
- 严格本地预演会停止轮询、断开 WS、取消 HTTP 并阻止所有后端写请求。
- 本地预演结果不能被描述为真实飞控执行结果。

### 2.6 Cesium 和离线地图

- 地理目标使用 WGS84/Cesium Georeference 转 UE 世界坐标。
- `GeoidSeparationMeters` 用于正高到椭球高的配置修正。
- 离线地图配置位于 `Config/DefaultEngine.ini` 的 `[CesiumTileServer]`；当前默认本地 tile server 地址是 `127.0.0.1:8870`。
- `DroneOpsGameMode` 支持运行时切换 URL raster overlay，并可按配置创建离线栅格平面。
- tile 数据、覆盖范围、XYZ/TMS、GCJ02/WGS84 和视觉对齐仍需按《Cesium 离线地图阅读清单》验收。

### 2.7 视频和元数据

- 后端注册字段中的 `video_url` 是 MediaMTX 浏览器 WebRTC 播放页。
- UE WebBrowser 加载 `http://<host>:8889/drone-N`；不是 RTSP 或原生 `/whep` 地址。
- MediaMTX 视频链路与控制/遥测链路独立。
- 视频元数据通过 `POST /api/video-metadata/batch` 保存到后端配置目录，不能把视频播放成功当作飞控证据。

## 3. 关键接口最小契约

```text
HTTP :8080
WS   :8081/ws
UDP telemetry recv: 8888, 8890, 8892, 8894, 8896, 8898
UDP control send:   8889, 8891, 8893, 8895, 8897, 8899
```

实时目标示例：

```json
{"mode":"move","drone_id":"d1","x":1000,"y":2000,"z":500,"speed":3.0}
```

后端控制示例：

```json
{
  "protocol":"ue5_drone_control",
  "version":1,
  "type":"control",
  "session_id":"...",
  "command_id":"...",
  "sequence":123,
  "drone_id":1,
  "slot":1,
  "mode":"move",
  "target":{"frame":"NED","reference":"power_on_origin","unit":"m","north":10,"east":20,"down":-5},
  "delivery":{"repeat_index":1,"repeat_total":5}
}
```

## 4. 验收分层

| 层级 | 可证明 | 不可证明 |
|---|---|---|
| 静态源码/配置 | 类、路由、字段、默认值存在 | 运行时行为 |
| C++ 单元/集成测试 | 后端逻辑、解析、状态和协议模拟 | 真实链路 |
| HTTP/WS | 服务可达、请求被接受、消息被推送 | Jetson/PX4 执行 |
| UDP send / `command_ack` | 后端尝试发送、后端入队 | 对端收到和应用 |
| YAML `control_ack` | Jetson 通过确认阈值并更新 setpoint 缓存 | PX4 一定解锁/运动 |
| UE Play Mode | UE 场景、UI、Cesium、视频的运行行为 | 外场网络/飞行 |
| 外场 PX4 证据 | 受控环境下的机载/飞控结果 | 未测场景的泛化 |

## 5. 明确边界和待验收项

1. 多机阵列必须分别使用各自 GPS/上电 NED 原点；公共地理目标不能复制参考机偏移。
2. 基础避障不是全局路径规划，也不替代人工安全决策。
3. 失联重连后旧目标不能在新原点下重放；必须记录新锚点和重新派发结果。
4. 本仓库不包含完整 Jetson/PX4 bridge；协议升级必须同步外部部署版本。
5. 当前工作区的 UE 5.7 编辑器编译、真实 WebRTC 多窗口、跨机网络和真实飞控运动需另行提供证据。

## 6. 需求变更规则

- 新增/修改路由、字段、单位或端口时，先改 `Docs/接口与通讯数据规范.md` 和源码，再更新本文件。
- 坐标行为先修改后端 `CoordinateConverter`、UE 锚点调用链和测试，不用 prompt、重试或城市特例掩盖原点错误。
- 任何“已完成”标记必须附验证环境、命令/操作、时间和原始证据；否则使用“代码存在”或“待运行验收”。

## 7. 关联文件

- `Docs/架构设计.md`
- `Docs/接口与通讯数据规范.md`
- `Docs/坐标系转换说明.md`
- `Docs/后端无人机通讯外场测试清单.md`
- `Docs/PROJECT_FACTS.md`
- `Backend/README.md`
