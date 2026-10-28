# UE5DroneControl 文档入口

> 当前主流程：UE 5.8 + C++17 `Backend` + Command / Map / Video，Stage 1 Mock 模拟。
> 2026-10-28 核对使用入口；协议、坐标和职责以对应源码为准。Jetson/PX4 与真实视频属于另行配置、验收的链路。

## 使用入口

1. [Windows 首次运行与配置](First-Run-Windows.md)：依赖、编译、Cesium 授权、代理和常见故障。
2. [使用与演示手册](Stage1/Stage1-Demo-Runbook.md)：方案创建/编辑/部署、Map 选机与框选、Mock 执行、接管移动、视频联动、关闭与恢复。
3. [Launcher 说明](../Launcher/README.md)：端口、本地配置、窗口管理和启动重置规则。
4. [地图启动与缓存](MarinaBayStartup.md)、[方案配置流程](PLAN-Configuration-Workflow.md)、[单机接管](ManualTakeover.md)：专项说明及证据边界。

从仓库根目录先运行 `SetupDroneSecurity.cmd` 编译，配置地图服务，再用 `CheckDroneSecurity.cmd` 检查和 `DroneSecurityLauncher.cmd` 启动。正常 Launcher 使用 **19880/19881**、`Saved/Stage1/data`，无需启动下面的独立后端。默认新建自有 Mock Backend 会执行编队重置；活动执行与预约的处理见使用手册。

## 开发者参考

1. [项目现状基线](PROJECT_FACTS.md)：从当前源码、配置和运行边界整理出的事实快照。
2. [系统架构](架构设计.md)：UE、后端、Jetson、PX4、MediaMTX 的职责和时序。
3. [接口与通讯数据规范](接口与通讯数据规范.md)：REST、WebSocket、UDP JSON/YAML 的当前字段。
4. [坐标系转换说明](坐标系转换说明.md)：UE cm、NED m、GPS/Cesium 锚点和阵列边界。
5. [后端开发文档](后端开发文档.md)：构建、模块、配置和后端测试。
6. [前端开发文档](前端开发文档.md)：UE 网络、Registry、路径/阵列、预演和视频窗口。
7. [后端无人机通讯外场测试清单](后端无人机通讯外场测试清单.md)：从本地模拟到真机的验收顺序。
8. [项目交接说明](PROJECT_HANDOFF.md)：交接启动、验证边界和已知风险。

## 其他文档

| 文档 | 定位 |
|---|---|
| `SequenceDiagrams.md` | 当前通信与任务时序图 |
| `UE5DroneControl需求文档.md` | 需求/完成情况/待验收项，不能替代代码事实 |
| `UE项目代码学习计划.md` | 面向新维护者的阅读路线 |
| `UE编译故障排查.md` | 编译排障参考；旧版本示例以当前 UE 5.8 和首次运行指南为准 |
| `Cesium离线地图需求技术文档阅读清单.md` | 离线瓦片/3D Tiles 的专题资料和待验证项 |
| `MediaMTX/README.md` | WebRTC/RTSP 视频演示与部署说明 |
| `MediaMTX/室内室外录制测试手册.md` | 视频录制专项测试手册 |
| `superpowers/` | 历史设计稿和阶段计划；实施前必须回看当前代码 |

部分旧的专项文档可能随工作区分支被删除或保留为历史资料。若文档中的 `BackEnd`、24 字节控制包、旧 Python bridge 与当前代码冲突，以 `PROJECT_FACTS.md` 和源码为准。

## 传统独立后端联调（开发用途）

以下使用 `Backend/config.yaml` 的 8080/8081，区别于普通 Launcher 的 19880/19881。从仓库根目录启动后端：

```powershell
cmake --build Backend\build --config Release --target DroneBackend
.\Backend\build\Release\DroneBackend.exe .\Backend\config.yaml
curl.exe http://127.0.0.1:8080/api/drones
```

UE 配置在 `Config/DefaultGame.ini`：

- HTTP：`http://127.0.0.1:8080`
- WebSocket：`ws://127.0.0.1:8081/ws`
- 注册表轮询：3 秒

本地视频演示：

```powershell
.\tools\MediaMTX\start_demo_streams.cmd
```

该脚本依赖本机 FFmpeg 和录像目录，不能作为跨机器的通用安装脚本。

## 事实与验收边界

- WS `command_ack` 表示后端接受，不表示 Jetson/PX4 已执行。
- UDP 发送成功不表示网络对端收到；只有回传 `control_ack` 才能证明 Jetson 达到应用确认阈值。
- 本地 debug、模拟遥测、静态编译检查不能替代真实 Jetson/PX4 或 UE Play Mode 验收。
- 当前项目仍有阵列多机 NED 原点补偿、真实视频窗口和跨机网络等专项待验证项。
