# UE5 编译故障排查指南

> 更新时间：2026-09-09
> 当前工程：UE 5.7，Windows + Visual Studio 2022；工程文件：`UE5DroneControl.uproject`。
> 本文只描述可复现的检查路径；“命令返回成功”仍不等同于编辑器 Play Mode 验收。

## 1. 先确认环境

- [ ] Epic/源码引擎版本为 5.7，不能用旧 5.3/5.4 生成的中间文件判断当前代码。
- [ ] 安装 Visual Studio 2022 的 C++ 游戏开发、Windows SDK 和 MSVC 工具链。
- [ ] 启用工程需要的 CesiumForUnreal、WebBrowserWidget、StateTree、ProceduralMeshComponent 等插件。
- [ ] 保留用户改动；清理前先记录 `git status --short`，不要用 `git reset --hard`。

## 2. 推荐重新生成和编译

1. 关闭 UE 编辑器和 Visual Studio。
2. 在资源管理器中右键 `UE5DroneControl.uproject`，选择生成 Visual Studio 项目文件。
3. 打开生成的解决方案，目标选择 `UE5DroneControlEditor`、`Development Editor`、`Win64`。
4. 编译；如只想验证后端，不要把后端构建结果当作 UE 编译结果。

命令行可使用已安装 UE 5.7 的 `Build.bat`，示例：

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UE5DroneControlEditor Win64 Development `
  -Project='D:\RedAlert\UE5DroneControl\UE5DroneControl.uproject' `
  -WaitMutex -FromMsBuild -architecture=x64
```

若引擎安装位置不同，替换路径；不要因为该示例路径不存在就修改工程文件。

## 3. C1076 / C3859：PCH 或虚拟内存不足

表现为 `compiler is out of heap space`、`C1076`、`C3859` 或预编译头创建失败。常见原因是并行编译、内存压力或系统页面文件太小。

按顺序处理：

1. 关闭其他 UE/VS/编译进程，重试一次。
2. 在 Visual Studio 中降低并行项目数，或让 UBT 使用更少的并行编译进程。
3. 增大系统页面文件并重启；不要为了“通过编译”删除大范围源码。
4. 仅在确认中间文件与当前 UE 5.7 一致后，再删除项目的 `Binaries/`、`Intermediate/`、`.vs/` 并重新生成。先备份/确认这些目录未含用户生成内容。
5. 记录完整首个错误；后续大量 `generated.h` 错误通常是连锁结果。

## 4. UHT / generated.h 错误

常见原因：头文件路径、`UCLASS/USTRUCT/UFUNCTION` 宏、反射字段类型或模块依赖不匹配。

- 先看第一个 UHT 错误，不要从最后一条 C++ 错误倒推。
- 检查新头文件是否位于 `Source/UE5DroneControl/`，并包含自己的 `.generated.h`。
- 检查 `Source/UE5DroneControl/UE5DroneControl.Build.cs` 的模块依赖；网络代码涉及 HTTP、WebSockets、Json 等模块。
- 修改 UHT 头文件后重新生成工程文件再编译。
- `#include` 使用项目实际大小写和相对模块路径，避免只在 Windows 上偶然通过。

## 5. 链接错误

- `unresolved external symbol`：检查声明/实现签名、模块依赖、实现文件是否加入工程源码目录。
- Cesium 符号缺失：确认 CesiumForUnreal 插件和对应 UE 5.7 版本已安装，且 `.uproject` 中启用。
- WebSocket/HTTP 符号缺失：检查 Build.cs 的 `WebSockets`、`HTTP`、`Json`/`JsonUtilities` 依赖以及插件启用情况。
- 不能用后端 `DroneBackend.exe` 的成功构建替代 UE 模块链接成功。

## 6. 运行时日志优先级

启动编辑器后，先看 `Saved/Logs/UE5DroneControl.log` 的第一个 `Error`/`Warning`，再看 UI。网络功能重点搜索：

```text
[DroneNetworkManager]
[DroneWS]
[DroneHttpClient]
Strict Local Preview Isolation
```

检查项：

- `BackendBaseUrl` 是否来自 `Config/DefaultGame.ini` 的实际值。
- WebSocket 是否连接到 `/ws`，而不是旧端口或旧路径。
- 严格本地预演是否按预期阻止轮询、WS 和写请求。
- Cesium tile server 的配置是否来自 `Config/DefaultEngine.ini`；当前默认地址是 `127.0.0.1:8870`，并非旧文档中的 `8070`。

## 7. 网络代理导致的假故障

当前 `DefaultEngine.ini` 的在线 HTTP 代理是本机环境值 `127.0.0.1:7890`。`DroneWebSocketClient` 会在创建本地 WebSocket 上下文时临时清除代理，恢复后仍供在线 HTTP 使用。

若 WS 仍失败：

1. 先确认后端 `GET /` 可达。
2. 用 `wscat -c ws://127.0.0.1:8081/ws` 单独验证服务端。
3. 检查 UE 实际日志中的 URL 和错误。
4. 再检查代理、防火墙和跨机路由；不要先修改协议字段或坐标代码。

## 8. 验证边界

```powershell
git status --short
```

静态编译/日志可证明：代码编译、模块加载、配置读取、请求是否发起。它们不能单独证明：Cesium 地理对齐、WebRTC 视频、跨机器网络、Jetson `control_ack`、PX4 模式/解锁或真实飞行运动。后者按 `Docs/后端无人机通讯外场测试清单.md` 逐项记录。
