# UE5DroneControl 项目交接说明

本文用于将项目交给新的维护者。接手后先执行 `git status --short` 和 `git log -1 --oneline`；本文件或其他运行日志可能处于未提交工作区，不能把工作区状态误写成提交已验收。

## 1. 项目定位

这是一个基于 Unreal Engine、Cesium 和 C++ 后端的无人机集群指挥与预演项目：

- UE5 负责三维场景、无人机实体、航点/阵列编辑、预演、UI 和控制请求。
- `Backend/` 负责 HTTP、WebSocket、UDP、无人机注册、遥测、坐标转换、任务执行和基础避障。
- Jetson/PX4 负责机载控制链路和飞控执行。本地仓库不包含完整的 Jetson/PX4 运行环境。

当前工程目标版本是 UE 5.7，Windows + Visual Studio 2022。

## 2. 目录职责

| 目录 | 作用 |
|---|---|
| `Source/UE5DroneControl/` | UE 游戏模块、无人机、网络、UI、路径和任务系统 |
| `Backend/` | C++17 后端服务、CMake 工程、单元测试和测试指南 |
| `Docs/` | 项目需求、架构、协议、坐标、开发和故障排查文档 |
| `tools/` | 模拟器、集成测试、软著辅助脚本和本地演示工具 |
| `tools/MediaMTX/` | MediaMTX 配置、可执行文件和本地视频演示启动脚本 |
| `Config/` | UE 默认配置，包含后端地址、代理和项目运行参数 |

`Docs/` 是项目维护文档的主入口；根目录若存在 README，只作为仓库级入口，不替代 `Docs/` 中的协议与事实基线。

建议阅读顺序：

1. `Docs/README.md`
2. `Docs/架构设计.md`
3. `Docs/接口与通讯数据规范.md`
4. `Docs/坐标系转换说明.md`
5. `Backend/README.md`
6. `Backend/TEST_GUIDE.md`
7. `Docs/UE编译故障排查.md`

## 3. 当前本地联调配置

### UE 与后端

默认配置位于 `Config/DefaultGame.ini`：

- HTTP：`http://127.0.0.1:8080`
- WebSocket：`ws://127.0.0.1:8081/ws`

后端配置位于 `Backend/config.yaml`：

- HTTP 端口：8080
- WebSocket 端口：8081
- UDP 入站端口：8888、8890、8892、8894、8896、8898
- 持久化注册数据：`Backend/data/drones.json`
- 启动后端时应从仓库根目录执行，使 `./Backend/data/drones.json` 路径正确。

`Config/DefaultEngine.ini` 当前将在线请求代理设置为 `127.0.0.1:7890`，本机代理变化时需要同步调整；无人机 WebSocket 和本机服务保持直连。离线 Cesium tile server 的默认配置地址是 `127.0.0.1:8870`，不是旧文档中的 `8070`。

### MediaMTX 视频演示

- RTSP 发布端口：8554
- WebRTC 页面端口：8889
- WebRTC ICE 默认 UDP 端口：8189
- UE 使用浏览器页面地址，例如 `http://127.0.0.1:8889/drone-1`，不要直接填 RTSP/WHEP 地址。
- 启动脚本：`tools/MediaMTX/start_demo_streams.cmd`
- PowerShell 主脚本：`tools/MediaMTX/start_demo_streams.ps1`

脚本默认依赖：

- FFmpeg/FFprobe：`C:\ffmpeg-8.0.1-essentials_build\ffmpeg-8.0.1-essentials_build\bin\`
- 录像目录：`D:\DroneData\recordings\drone-4`
- 录像文件：脚本中的 `drone-1`、`drone-2`、`drone-3` 对应输入文件。

如果接手机器路径不同，应先修改脚本中的 `$ffmpeg`、`$ffprobe` 和 `$recordings`，不要直接修改 `video_url` 伪造运行状态。

## 4. 最小启动流程

### 4.1 编译后端

需要 Visual Studio 2022、CMake 和 vcpkg。可以优先使用：

```powershell
cd D:\RedAlert\UE5DroneControl
.\Backend\build_simple.bat
```

或者按本机 vcpkg 路径执行：

```powershell
cmake -S Backend -B Backend\build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build Backend\build --config Release --target DroneBackend
```

### 4.2 启动后端

必须从仓库根目录启动：

```powershell
.\Backend\build\Release\DroneBackend.exe .\Backend\config.yaml
```

健康检查：

```powershell
curl.exe http://127.0.0.1:8080/api/drones
```

预期能看到注册表 JSON；后端日志应出现 HTTP `:8080` 和 WS `:8081` 监听信息。

### 4.3 启动 UE

1. 安装 UE 5.7 和 `.vsconfig` 中列出的 VS 组件。
2. 右键 `UE5DroneControl.uproject`，生成 Visual Studio 工程文件。
3. 使用 `UE5DroneControlEditor` 编译并打开编辑器。
4. 启动前确认后端已监听 8080/8081。

### 4.4 启动视频演示（可选）

```powershell
.\tools\MediaMTX\start_demo_streams.cmd
```

脚本会启动 MediaMTX 和 3 路循环推流，并检查 RTSP 是否可读。视频窗口功能仍需要在 UE 中实际进入对应场景验证。

## 6. 验证状态

已完成的静态核对：

- `Docs/` 已整理为项目维护文档主入口；根目录 README（如存在）只做仓库级导航。
- 后端和文档中的主要路径引用已同步调整。
- `git diff --check` 无空白错误。
- 历史运行日志显示后端曾成功监听 8080/8081，并从 `Backend/data/drones.json` 加载注册数据。

尚未由本次交接提交证明的内容：

- 未把 UE 5.7 编辑器完整编译和打包结果作为本次提交的验收证据。
- 未在真实 Jetson/PX4 上验证新的限速控制行为。
- 未完成真实无人机、真实 WebRTC 多窗口和跨机器网络验收。
- 不能把历史 `logs/backend.log` 或 FFmpeg 日志当作当前机器的实时健康证明。

建议接手后的最小验收顺序：

1. 编译并运行后端 C++ 单元测试：

   ```powershell
   cmake --build Backend\build --config Release --target DroneBackend_tests
   .\Backend\build\Release\DroneBackend_tests.exe
   ```

2. 启动后端，执行 `GET /api/drones`，确认注册表和端口。
3. 使用 debug move 请求验证 `speed` 被接受，并检查队列/日志中的速度。
4. 编译 UE 5.7 Editor，进入预演场景，验证路径材质刷新、视频窗口和本地后端连接。
5. 需要实机时，再按 `Backend/TEST_GUIDE.md` 和 Jetson 侧协议文档做 UDP、ACK、坐标和断联验证。

## 7. 已知边界与接手建议

- 多机阵列的不同上电 NED 原点仍是重点风险；详见 `Docs/坐标系转换说明.md` 和 `Backend/README.md` 的“已知边界”。
- 基础避障是执行层保护，不等同于完整全局路径规划。
- `Backend/config.yaml`、`Backend/data/drones.json` 和视频脚本包含本机地址/路径，换机器前必须检查。
- 当前分支是 `main`；提交前本地已有若干领先 `origin/main` 的提交。提交后是否 push 由项目负责人决定。
- 工作区中的运行日志和 FFmpeg 输出属于本机运行产物，除非专门需要，不要提交。

## 8. 常用排障入口

- UE 编译：`Docs/UE编译故障排查.md`
- 后端构建、接口和模拟遥测：`Backend/README.md`、`Backend/TEST_GUIDE.md`
- 系统职责和数据流：`Docs/架构设计.md`
- HTTP/WebSocket/UDP 字段：`Docs/接口与通讯数据规范.md`
- 坐标和原点：`Docs/坐标系转换说明.md`
- UE 前端、预演和视频窗口：`Docs/前端开发文档.md`、`Docs/MediaMTX/README.md`
