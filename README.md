# UE5DroneControl

## P5.3 / 新电脑首次运行

> **当前请使用 Git + Git LFS 克隆，不要用 Code → Download ZIP 启动项目。** 已实测 GitHub ZIP 中 CesiumWorld.umap 只有 130 字节、CesiumIonSaaS.uasset 只有 129 字节，均为 LFS 占位文本，无法由 Unreal 加载。
>
> ```powershell
> git lfs install
> git clone https://github.com/WangYu0611/drone_security.git C:\Projects\drone_security
> cd C:\Projects\drone_security
> git lfs pull
> ```
> 克隆目录必须是新的空目录；保留旧工程中的个人修改。下载资源后先运行 `CheckDroneSecurity.cmd`，再首次编译。

已加入闭合航线、安保方案整体移动与闭环 Mock 执行。完整交付边界见 [P5.3 说明](Docs/TASK-P5.3-PlanGeometryClosure.md)。

**新电脑必须额外安装 Cesium for Unreal 到 UE 5.8**，并通过 Git LFS 下载资源。只安装 UE 和 VS 尚不足以运行。

1. 阅读 [Windows 首次运行指南](Docs/First-Run-Windows.md)，安装 Cesium、VS C++ 工具、Python 3.10+ 和 Git LFS。
2. 双击 `SetupDroneSecurity.cmd` 编译后端和 UE 模块。
3. 在 Unreal Editor 中配置自己的 Cesium ion Token。
4. 双击 `CheckDroneSecurity.cmd` 检查，再运行 `DroneSecurityLauncher.cmd`。

启动器自动定位 UE，本地路径覆盖写入 `Launcher/config.local.json`。Stage 1 默认端口是 **19880/19881**；下方 8080/8081 属于传统独立后端流程。

UE5DroneControl 是一个基于 Unreal Engine 5.8、Cesium 和 C++17 后端的无人机安保指挥、三维态势展示与任务模拟演示项目。当前 Stage 1 采用 Command（指挥端）、Map（地图端）和 Video（视频端）三个独立客户端，由 DroneBackend 统一管理业务状态，并通过 HTTP / WebSocket 同步。

当前已实现安保方案创建、任务与无人机分配、地图航线编辑、方案部署、显式开始任务，以及 Mock 执行监控、暂停、继续、模拟返航和终止的业务闭环。部署只激活配置，任务执行需单独开始；当前执行能力属于 Mock / Simulation，不代表真实无人机飞行。

仓库保留 Jetson/PX4 控制与遥测接口，以及 MediaMTX RTSP / WebRTC 视频链路实现。它们尚未作为当前 Stage 1 主流程完成真机端到端复验；当前视频演示以布局和视频目标切换为主，实际播放需另行配置并验证视频源。

本文是仓库级入口。当前功能范围、数据接口和演示步骤以 [Stage 1 功能审计](Docs/Stage1/Stage1-Feature-Audit.md)、[汇报摘要](Docs/Stage1/Stage1-Presentation-Summary.md)、[演示手册](Docs/Stage1/Stage1-Demo-Runbook.md)及对应源码为依据；下文保留的传统控制与机载链路说明需结合这些文档区分当前主流程和历史能力。

> 当前工程基线：Windows + Unreal Engine 5.8（工程关联与已保存运行日志一致）+ Visual Studio 2022 / MSVC 工具链 + C++17 后端。
>
> 当前 Stage 1 已有自动化测试与原生客户端运行证据，可在文档约定的条件下进行 Mock 业务演示。真实 Jetson/PX4 飞行、跨机器 UDP、真实相机 WebRTC 播放，以及实际场景的 Cesium 地理对齐与资源加载仍需专项验证；已有证据不等于当前环境已重新验收。

## 1. 系统职责

### UE5 前端

UE5 负责 Cesium 地理场景、无人机世界坐标展示、注册表镜像、列表/告警/选择交互、单机实时控制、路径编辑、路径回放、多机阵列派发、集结交互、影子机预演和 MediaMTX 视频窗口。

UE 不拥有后端注册表、GPS 上电锚点、NED 控制目标或飞控执行事实。HTTP 2xx、WebSocket `command_ack` 或后端 UDP 发送成功，都不能单独证明 PX4 已执行。

### C++ 后端

`Backend/` 构建为 `DroneBackend.exe`，负责 HTTP REST、WebSocket、UDP 遥测、心跳、注册表持久化、slot/端口映射、状态机、命令队列、GPS/NED 坐标转换、集结、路径任务、自动分配、执行引擎、基础避障和视频元数据落盘。

### Jetson/PX4

机载侧接收后端 JSON v1 控制报文，去重并做旧包判断，达到应用确认阈值后向 PX4 维持 Offboard setpoint，通过 YAML 遥测和 `control_ack` 回传。仓库中的 Jetson 脚本包含辅助/协议测试内容；外场必须使用与当前协议匹配的机载版本。

### MediaMTX

视频是独立链路：Jetson 或 FFmpeg 发布 RTSP，MediaMTX 转成 WebRTC 浏览器页面，UE WebBrowser 加载该页面。视频可播放不代表控制链路健康，控制链路健康也不代表视频可播放。

## 2. 总体架构

```text
┌──────────────────────────── Windows / UE5 ────────────────────────────┐
│ Cesium 场景、Registry、UI、路径/阵列编辑、影子机、视频窗口             │
│       │ HTTP REST :8080       │ WebSocket :8081/ws                   │
└───────┼───────────────────────┼───────────────────────────────────────┘
        ▼                       ▼
┌───────────────────────────────────────────────────────────────────────┐
│ DroneBackend.exe                                                       │
│ HTTP/WS │ DroneManager │ Heartbeat │ CommandQueue │ Assembly/Execution │
│ UDP YAML 接收 :8888,8890,... │ UDP JSON 控制 :8889,8891,...            │
│ Registry 持久化 │ GPS/NED 转换 │ 视频元数据 JSONL                    │
└───────────────────────────────┬───────────────────────────────────────┘
                                ▼ UDP
                    Jetson bridge / ROS 2 / PX4
                    control_ack + telemetry YAML

Jetson/FFmpeg ── RTSP :8554 ──► MediaMTX ── WebRTC :8889 ──► UE WebBrowser
```

| 组件 | 主要职责 | 不应被误认为 |
|---|---|---|
| UE5 | 交互、世界坐标、路径/阵列编辑、镜像、视频窗口 | 飞控执行确认来源 |
| DroneBackend | 注册表、状态机、坐标、队列、集结、任务执行 | PX4 本身 |
| Jetson | 控制去重、应用确认、Offboard、遥测回传 | UE 世界坐标拥有者 |
| PX4 | 最终飞控执行 | 业务注册表 |
| MediaMTX | RTSP/WebRTC 转发 | 控制或遥测服务 |

## 3. 目录结构

```text
UE5DroneControl/
├─ UE5DroneControl.uproject       UE5 工程入口
├─ Config/                        UE 默认连接、代理和项目配置
├─ Content/                       关卡、蓝图、Cesium 和游戏资产
├─ Source/UE5DroneControl/        UE C++ 模块
│  ├─ DroneOps/Network/            HTTP、WebSocket、网络管理器
│  ├─ DroneOps/Core/               Registry、坐标服务、地理类型
│  ├─ DroneOps/Drone/              显示、遥测、命令、地面投影
│  ├─ DroneOps/Control/            地图点击、选择和控制器
│  ├─ PathEditor/                  路径、航点、回放、冲突处理
│  ├─ UI/                          列表、阵列、路径、视频窗口
│  └─ TaskSystem/                  UE 侧任务管理
├─ Plugins/                       SimpleWebSocket、VisualStudioTools
├─ Backend/                       C++17 DroneBackend
│  ├─ communication/               UDP、WebSocket、延迟测试
│  ├─ conversion/                  坐标、四元数、GPS 锚点、分配
│  ├─ drone/                       状态机、心跳、队列、管理器
│  ├─ execution/                   集结控制器、规划器、执行引擎
│  ├─ http/                        HTTP API 与 WebSocket 处理
│  ├─ storage/                     视频元数据存储
│  ├─ tests/                       GoogleTest 单元测试
│  ├─ config.yaml                  后端权威配置
│  └─ data/drones.json              示例/运行时注册表
├─ Jetson/                         机载 bridge、协议和视频辅助脚本
├─ tools/MediaMTX/                 本地 RTSP/WebRTC 演示
├─ tools/mock_ue/                  Python 前端通讯模拟器
├─ tools/simulate_telemetry.py    遥测模拟
├─ Docs/                           当前维护文档和交接资料
└─ px4_msgs/, ue_px4_msgs/         PX4/ROS 消息相关本地资料
```

`Binaries/`、`DerivedDataCache/`、`Intermediate/`、`Saved/`、日志和视频输出是本地构建/运行产物，不是项目事实来源。

## 4. 运行前准备

| 依赖 | 用途 |
|---|---|
| Unreal Engine 5.8 | 打开、编译和运行 UE 工程 |
| Visual Studio 2022 | UE Editor 和后端编译 |
| CMake + vcpkg | Backend 配置、Boost/yaml-cpp/JSON/spdlog/GTest |
| PowerShell | 工具脚本和启动流程 |
| Python 3 | mock UE、模拟遥测、协议测试 |
| Node.js + `wscat` | 可选，手工 WebSocket 调试 |
| Jetson ROS 2/PX4/MicroXRCE | 真实机载链路，不能由本地 Python 依赖替代 |

后端依赖以 `Backend/vcpkg.json` 为准，包括 Boost System/JSON/Beast、yaml-cpp、nlohmann-json、spdlog 和 GoogleTest。根目录 `requirements.txt` 是历史/辅助 Python 依赖集合，不是完整 Jetson 环境安装器。

环境检测脚本会生成本地 `.env.local`，不会替代依赖安装：

```powershell
.\setup_environment.ps1 `
  -UnrealRoot "C:\Path\To\UE_5.8" `
  -VcpkgToolchain "C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

运行后请检查生成路径；不要把 `.env.local`、密码或密钥提交到仓库。

## 5. 编译与启动

### 5.1 编译后端

```powershell
cd D:\RedAlert\UE5DroneControl
.\Backend\build_simple.bat
```

手动构建：

```powershell
cmake -S Backend -B Backend\build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build Backend\build --config Release --target DroneBackend
```

输出通常是 `Backend\build\Release\DroneBackend.exe`。源码或协议更新后必须重新编译，旧可执行文件不能代表当前 JSON v1 协议。

### 5.2 启动和健康检查

必须从仓库根目录启动：

```powershell
.\Backend\build\Release\DroneBackend.exe .\Backend\config.yaml
curl.exe http://127.0.0.1:8080/
curl.exe http://127.0.0.1:8080/api/drones
```

| 服务/数据 | 默认值 |
|---|---|
| HTTP | `http://127.0.0.1:8080` |
| WebSocket | `ws://127.0.0.1:8081/ws` |
| 注册表 | `Backend/data/drones.json` |
| 后端日志 | `logs/backend.log` |
| 视频元数据 | `D:/DroneData/metadata` |

### 5.3 打开 UE5

1. 安装 UE 5.8 和 `.vsconfig` 所需 VS 组件。
2. 右键 `UE5DroneControl.uproject` 生成工程文件，或使用已有 `.sln`。
3. 编译 `UE5DroneControlEditor`。
4. 打开编辑器并进入目标关卡/预演场景。
5. 确认后端监听 `8080/8081` 后再测试注册表、控制和视频。

默认 `Config/DefaultGame.ini`：

```ini
[/Script/UE5DroneControl.DroneNetworkManager]
BackendBaseUrl="http://127.0.0.1:8080"
WebSocketUrl="ws://127.0.0.1:8081/ws"
PollIntervalSec=3.0
GeoidSeparationMeters=0.0
```

外场只修改配置中的后端地址，不要在 C++ 中硬编码新地址。`Config/DefaultEngine.ini` 的 `127.0.0.1:7890` 是当前机器的在线 HTTP 代理值，换机器必须复核；本机后端和 WebSocket 应保持直连。

## 6. 后端配置和端口

权威配置是 `Backend/config.yaml`，当前主要参数：

```yaml
server:
  http_port: 8080
  ws_port: 8081
  debug: true
drone:
  max_count: 6
  heartbeat_hz: 5
  command_repeat_count: 5
  lost_timeout_sec: 10
  arrival_threshold_m: 1.0
  assembly_timeout_sec: 60
  avoidance_radius_m: 3.0
  avoidance_lookahead_sec: 2.0
  assembly_safety_cylinder_m: 2.0
  low_battery_threshold: 20
```

| slot | 后端接收遥测 | 后端发送控制 |
|---:|---:|---:|
| 1 | 8888 | 8889 |
| 2 | 8890 | 8891 |
| 3 | 8892 | 8893 |
| 4 | 8894 | 8895 |
| 5 | 8896 | 8897 |
| 6 | 8898 | 8899 |

`port_map.<slot>.host` 是实际控制目标地址。注册表中的 `ip/port` 会保存并返回，但不能替代后端启动时的 slot 控制映射。跨机器部署还需放行 Windows TCP `8080/8081`、后端 UDP `8888/8890/...` 和 Jetson 控制端口 `8889/8891/...`。

## 7. 通讯协议速查

完整字段见 [接口与通讯数据规范](Docs/接口与通讯数据规范.md)。

### 7.1 HTTP REST

| 方法 | 路径 | 作用 |
|---|---|---|
| GET | `/` | 服务信息和端口状态 |
| GET | `/api/drones` | 注册表、连接状态、遥测和任务快照 |
| POST | `/api/drones` | 注册无人机并分配 `dN` ID |
| PUT | `/api/drones/{id}` | 更新名称、型号、IP、端口或视频地址 |
| DELETE | `/api/drones/{id}` | 删除注册记录和后端上下文 |
| GET | `/api/drones/{id}/anchor` | 获取有效 GPS 上电锚点 |
| POST | `/api/drones/refresh` | 对 offline/lost 无人机发安全 hold 探测 |
| POST | `/api/video-metadata/batch` | 幂等写入视频元数据 JSONL 批次 |
| POST | `/api/arrays` | 创建集结/阵列任务 |
| POST | `/api/arrays/{id}/stop` | 停止阵列和执行引擎 |

`server.debug=true` 时还可用：`/api/debug/drone/{id}/state`、`queue`、`/api/debug/heartbeat/{id}`、`/api/debug/drone/{id}/inject`、`/api/debug/cmd/{id}/move`、`pause`、`resume`、`array`、`target`、`/api/debug/cmd/batch/array`、`/api/debug/arrays/{id}/state`。

PowerShell 中使用 `curl.exe`，不要使用 `curl` 别名：

```powershell
curl.exe -X POST http://127.0.0.1:8080/api/drones `
  -H "Content-Type: application/json" `
  -d '{"name":"UAV1","model":"PX4","slot":1}'
curl.exe http://127.0.0.1:8080/api/drones
```

### 7.2 WebSocket

连接地址：`ws://<backend-host>:8081/ws`。目标指令必须使用 `mode`，支持 `move`、`scout`、`patrol`、`attack`：

```json
{"mode":"move","drone_id":"d1","x":1000,"y":2000,"z":500,"speed":1.0}
{"type":"pause","drone_ids":["d1","d2"]}
{"type":"resume","drone_ids":["d1","d2"]}
```

后端向 UE 推送 `telemetry`、`event`、`alert`、`assembling`、`assembly_complete`、`assembly_timeout`、`assignment_result`、`drone_task_state`、`command_ack` 和 `error`。`command_ack` 只表示后端接受/入队，不是飞控执行确认。

### 7.3 UDP JSON v1/YAML

后端发送给 Jetson 的 JSON 根字段包括 `protocol=ue5_drone_control`、`version=1`、`type=control`、`session_id`、`command_id`、`sequence`、`drone_id`、`slot`、`mode`、时间戳、`target` 和 `delivery`。同一条 move 默认使用同一个 `command_id` 重发 5 次，通过 `repeat_index/repeat_total` 去重。

```json
{"frame":"NED","reference":"power_on_origin","unit":"m","north":0,"east":0,"down":0}
```

Jetson 回传 YAML 遥测，后端解析位置、姿态、速度、电量、GPS、local position、arming/nav 状态和 `control_ack`。只有匹配当前会话、命令、序列、模式和确认计数的 `control_ack` 才能作为机载应用确认。

## 8. 坐标系统与重要不变量

单机实时控制的 `x/y/z` 是该机上电 NED 原点下的 UE 相对偏移，单位 cm，不是 Cesium 世界坐标：

```text
UE_X(cm) × 0.01 = N(m)       UE_Y(cm) × 0.01 = E(m)
-UE_Z(cm) × 0.01 = D(m)
N(m) × 100 = UE_X(cm)        E(m) × 100 = UE_Y(cm)
-D(m) × 100 = UE_Z(cm)
```

每架无人机有独立的上电 NED 原点。GPS 锚点负责地理定位和重建原点；GPS 经纬高不能直接替代 `local_position`。期望目标、实际遥测和 hold 目标必须分开保存。多机阵列必须为每架执行机分别完成原点补偿；当前路径重映射仍存在把 Cesium/UE 世界坐标直接送入相对偏移入口的风险，外场前必须专项验证。

## 9. 主要模块

- `DroneNetworkManager`：HTTP、WebSocket 和 3 秒注册表轮询。
- `DroneHttpClient` / `DroneWebSocketClient`：后端 REST/实时消息封装。
- `DroneRegistrySubsystem`：身份、遥测、连接、任务和选择状态镜像。
- `RealTimeDroneReceiver` / `MultiDroneCharacter`：真机镜像/预演显示。
- `DronePathActor`、`DroneWaypointActor`、`DronePlaybackManager`：路径和回放。
- `SequenceDispatchPanelWidget`：路径映射、速度/等待和阵列派发。
- `AssemblyController`、`AssemblyPlanner`、`ExecutionEngine`：后端集结、自动分配和执行。
- `CesiumCoordinateService` / `SimpleCoordinateService`：地理坐标与 UE 世界坐标。
- `DroneVideoWindowWidget` / `DroneVideoWindowManager`：MediaMTX 浏览器视频页。

启用 `bStrictLocalPreview=true` 或调用 `SetStrictLocalPreviewIsolation(true)` 后，UE 会停止轮询、取消 HTTP、断开 WS、禁止自动重连，并阻止注册/refresh/move/pause/resume/array/stop 等后端写请求；关闭隔离后恢复 WS、轮询并立即同步注册表。

## 10. MediaMTX 视频演示

说明见 [Docs/MediaMTX/README.md](Docs/MediaMTX/README.md)，配置位于 `tools/MediaMTX/mediamtx.yml`：

| 用途 | 默认值 |
|---|---:|
| RTSP 发布 | `:8554` |
| WebRTC 浏览器页面 | `:8889` |
| WebRTC ICE 默认 UDP | `:8189` |
| 演示路径 | `drone-1`、`drone-2`、`drone-3` |

启动：

```powershell
.\tools\MediaMTX\start_demo_streams.cmd
```

UE 使用 `http://127.0.0.1:8889/drone-1` 这类浏览器页面，不要直接填 RTSP 或原始 WHEP 地址。脚本依赖本机 FFmpeg/FFprobe 和录像目录，换机器前必须修改路径并确认 MediaMTX 可被 UE 访问。

## 11. Mock UE、模拟遥测和测试

### C++ 单元测试

```powershell
cmake --build Backend\build --config Release --target DroneBackend_tests
.\Backend\build\Release\DroneBackend_tests.exe
```

覆盖坐标转换、四元数、GPS 锚点、命令队列、状态机、集结规划和视频元数据存储。测试通过不等于 UE Play Mode 或真机验收通过。

### Jetson 协议测试

```powershell
python .\Jetson\test_jetson_bridge_protocol.py
```

该测试检查 JSON v1 的会话、序列、重复包、冲突目标和确认门控；真实网络和 PX4 仍需在 Jetson 验证。

### Mock UE

`tools/mock_ue/` 使用正式 REST/WebSocket 接口维护本地注册表、锚点、遥测、告警和阵列状态，不调用后端 debug 接口冒充 UE 内部模块：

```powershell
cd tools\mock_ue
pip install -r requirements.txt
python -m mock_ue.main shell
```

常用命令：`status`、`list`、`register ...`、`move d1 1000 2000 500`、`pause d1`、`resume d1`、`array samples/recon_two_drones.json`、`stop a1`。它不做 Cesium GPS 到 UE 世界坐标转换，也不模拟 UE 影子机渲染。

### 后端最小联调

```powershell
curl.exe http://127.0.0.1:8080/api/drones
curl.exe -X POST http://127.0.0.1:8080/api/debug/drone/1/inject `
  -H "Content-Type: application/json" `
  -d '{"position":[0,0,0],"q":[1,0,0,0],"velocity":[0,0,0],"battery":90,"gps_lat":39.9042,"gps_lon":116.4074,"gps_alt":50}'
curl.exe -X POST http://127.0.0.1:8080/api/debug/cmd/1/move `
  -H "Content-Type: application/json" `
  -d '{"x":1000,"y":2000,"z":500,"speed":1.0}'
curl.exe http://127.0.0.1:8080/api/debug/drone/1/state
curl.exe http://127.0.0.1:8080/api/debug/drone/1/queue
curl.exe http://127.0.0.1:8080/api/debug/heartbeat/1
```

这些 debug 接口必须保持在受控测试环境；生产/外场应关闭不需要的 `server.debug` 接口，并按外场清单使用真实遥测和 `control_ack`。

## 12. 文档导航

建议阅读顺序：

1. [项目事实基线](Docs/PROJECT_FACTS.md)
2. [系统架构设计](Docs/架构设计.md)
3. [接口与通讯数据规范](Docs/接口与通讯数据规范.md)
4. [坐标系转换说明](Docs/坐标系转换说明.md)
5. [后端开发文档](Docs/后端开发文档.md)
6. [前端开发文档](Docs/前端开发文档.md)
7. [后端无人机通讯外场测试清单](Docs/后端无人机通讯外场测试清单.md)
8. [项目交接说明](Docs/PROJECT_HANDOFF.md)
9. [UE 编译故障排查](Docs/UE编译故障排查.md)
10. [Cesium 离线地图资料清单](Docs/Cesium离线地图需求技术文档阅读清单.md)
11. [MediaMTX 说明](Docs/MediaMTX/README.md)
12. [当前通信时序图](Docs/SequenceDiagrams.md)

`Docs/UE5DroneControl需求文档.md` 用于需求/完成情况追踪；`Docs/superpowers/` 保存历史计划和设计稿，不能替代当前源码、配置和 `PROJECT_FACTS.md`。

## 13. 已知边界与验收顺序

- 单机实时 move 契约已明确，但每架无人机不同上电原点下的多机阵列仍需专项外场验证。
- 后端状态、WS `command_ack`、UDP 发送成功和 Jetson `control_ack` 是不同层级事实，不能混写成“已执行”。
- 本地注册表、mock UE、模拟遥测和历史日志只能证明开发链路，不能证明真实无人机在线。
- UE C++ 编译成功不能证明 UI 绑定、Cesium 对齐、WebRTC 页面或 Play Mode 时序正确。
- `Backend/config.yaml`、视频脚本和示例数据包含本机 IP/路径，换机器前必须检查。

推荐顺序：

1. 编译并运行后端单元测试。
2. 启动后端，检查 `/`、`/api/drones` 和端口监听。
3. 用 debug 或 mock UE 验证注册、遥测、队列、速度和暂停/恢复。
4. 编译 UE 5.8 Editor，验证 Registry、锚点、路径和严格本地预演。
5. 单独验证 MediaMTX 视频窗口。
6. 使用真实 Jetson/PX4 验证 UDP、ACK、坐标、断联和安全停止。
7. 最后验证多机阵列的原点补偿、集结、自动分配和任务停止。

## 14. 维护规则

- 代码/配置优先于说明文档：路由以 `Backend/http/http_server.cpp` 为准，端口以 `Backend/config.yaml` 为准，UE 网络默认值以 `Config/DefaultGame.ini` 为准。
- 修改协议时同步更新 `Docs/接口与通讯数据规范.md`、`Docs/PROJECT_FACTS.md`、后端、UE 客户端和 Jetson bridge。
- 修改坐标或阵列语义时同步检查 `Docs/坐标系转换说明.md`、`DroneNetworkManager`、`DroneOpsPlayerController`、`SequenceDispatchPanelWidget` 和后端转换器。
- 不提交本地运行日志、FFmpeg 输出、构建目录、缓存、秘密配置或个人路径。
- 提交前至少执行：

```powershell
git status --short
git diff --check
```
