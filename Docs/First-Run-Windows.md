# P5.3：在另一台 Windows 电脑首次运行

这是源码工程，不是解压即用的打包游戏。Stage 1 是 Mock 模拟演示，包含 Backend、Command、Map、Video；不需要连接真实无人机或安装 ROS。

## 1. 安装依赖

1. Unreal Engine **5.8**。
2. Visual Studio 的“使用 C++ 的桌面开发”和“使用 C++ 的游戏开发”，包括 Windows SDK、MSVC、C++ CMake tools for Windows、vcpkg。仅安装 VS 编辑器本身不够。
3. **Cesium for Unreal**：在 Epic Games Launcher 的 Fab 库中获取并安装到 **UE 5.8**。安装到其他引擎版本不会生效。插件不随本仓库分发，不要直接关闭插件来绕过报错，工程源码依赖 CesiumRuntime。
4. Python **3.10 或更高版本**，安装时包括 Tcl/Tk、Python Launcher，或将 Python 加入 PATH。Stage 1 启动器只使用标准库，不需要安装根目录的全部历史 requirements。
5. Git 和 Git LFS。

Cesium 官方安装和 Token 配置说明：<https://cesium.com/learn/unreal/unreal-quickstart/>。

## 2. 获取完整源码和资源

建议使用一个固定目录，例如 `C:\Projects\drone_security`，后续直接在该目录更新。

```powershell
git lfs install
git clone https://github.com/WangYu0611/drone_security.git C:\Projects\drone_security
cd C:\Projects\drone_security
git lfs pull
```

已有克隆可执行 `git pull --ff-only` 和 `git lfs pull`。如有本地修改，先保留自己的修改再更新。
GitHub Download ZIP 可能包含 LFS 占位文件；资源缺失时请改用以上克隆方式。空的 `px4_msgs` 历史子模块不参与 Stage 1 构建，不需要递归初始化。

## 3. 首次编译

先关闭该工程的 UE 编辑器和三个客户端，再双击根目录 **SetupDroneSecurity.cmd**。

- 自动查找 UE 5.8、VS C++ 工具、CMake、vcpkg。
- 先检查 Cesium 和地图资源，再构建 `Backend/build/Release/DroneBackend.exe` 及 UE Development Editor 模块。
- vcpkg 首次下载依赖需要联网，耗时取决于网络和电脑性能。
- 不删除旧构建目录，不覆盖 `Saved/Stage1` 方案数据。
- 构建失败看 `Saved/first-run-build.log`；CMake 缓存若来自另一台机器，应重新生成本机构建目录，不要把另一台电脑的缓存复制过来。

自动发现失败时，可设置 `UE5DRONE_UE_ROOT` 为引擎根目录、`VCPKG_ROOT` 为 vcpkg 根目录，或者 `UE5DRONE_VCPKG_TOOLCHAIN` 为工具链文件完整路径。
可创建不受 Git 跟踪的 `Launcher/config.local.json`：

```json
{
  "ue_executable": "D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe",
  "backend_executable": "Backend/build/Release/DroneBackend.exe"
}
```

路径使用 `/`，不用复制开发者的用户名、盘符或 `build-p53` 路径。

## 4. 设置地图服务

首次打开 `UE5DroneControl.uproject`，在 Cesium 面板登录自己的 Cesium ion 账号，通过 Token 设置项目使用的凭据。公开仓库已清除开发者凭据，安装插件本身不会提供地图服务权限。
确认 CesiumWorld 关卡的地形/影像服务能加载；如果资产自身设置了 Token，也要更新该资产的配置。不要将自己的 Token 或包含 Token 的资产提交到公开仓库。
离线、代理、401/403 或网络限制可能造成黑地图，这类问题不会由编译解决。

## 5. 启动

双击 **CheckDroneSecurity.cmd** 检查环境，成功后双击 **DroneSecurityLauncher.cmd**。
启动器等待 Backend、Command、Map、Video 全部 ONLINE。默认本机端口为 **19880/19881**。
首次着色器编译可能较慢，可在本地配置中增加 `startup_timeout`；同时运行三个 UE 客户端需要足够内存和显存。

| 现象 | 处理 |
|---|---|
| 找不到 CesiumForUnreal / CesiumRuntime | 将 Cesium 安装到 UE 5.8，再首次编译 |
| 缺少模块 / 由不同引擎版本构建 | 用本机 UE 5.8 重新运行 SetupDroneSecurity.cmd |
| 缺少 DroneBackend.exe | 先运行 SetupDroneSecurity.cmd，检查 VS C++ / vcpkg |
| 提示 Python 或 tkinter 缺失 | 修改 Python 安装，启用 Tcl/Tk 和 Launcher |
| 地图文件极小或 LFS 占位 | `git lfs pull` |
| 地图黑屏、401/403 | 检查自己的 Cesium ion Token、资产权限及网络 |
| 端口占用或 Launcher already running | 回到已运行启动器；关闭自己启动的冲突实例后重试 |

报告问题时提供 `Saved/environment-check.json`、`Saved/first-run-build.log`（如编译失败）、`Saved/Stage1/Logs` 下对应客户端日志。分享前移除日志中的凭据。

## 验证边界

本次验证包含本机依赖探测、缺失 Cesium/LFS/二进制/错误引擎等自动化场景及启动器回归。另一台电脑尚未现场验证；安装好依赖后仍需实际启动确认。P5.3 的原始业务验收记录见 `TASK-P5.3-PlanGeometryClosure.md`。
