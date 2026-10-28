# Windows 首次运行与本机配置

这是源码工程，不是解压即用的打包游戏。Stage 1 是 Mock 模拟演示，包含 Backend、Command、Map、Video；不需要连接真实无人机或安装 ROS。

本手册于 2026-10-28 对照当前启动器和项目配置核对。已安装好依赖的开发电脑可以先运行 `CheckDroneSecurity.cmd`，再运行 `DroneSecurityLauncher.cmd`。检查通过只确认依赖、资源和本机构建文件存在；Cesium 账号权限、地图网络及实际画面仍需启动后确认。

## 1. 安装依赖

1. Unreal Engine **5.8**。
2. Visual Studio **2022 或 2026** 的“使用 C++ 的桌面开发”和“使用 C++ 的游戏开发”，包括 Windows SDK、MSVC、C++ CMake tools for Windows、vcpkg。首次构建脚本支持这两个版本的 CMake 生成器，不代表每种 MSVC / SDK 组合都已验收；仅安装 VS 编辑器本身不够。
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

已有克隆先用 `git status` 检查本地修改；保留自己的修改后，再执行 `git pull --ff-only` 和 `git lfs pull`。不要覆盖本机 Cesium 凭据资源或删除 `Saved/Stage1/data` 中的业务数据。
源码更新后仍需关闭正在运行的 Backend 和客户端，再运行 `SetupDroneSecurity.cmd`；拉取代码不会自动更新本机 EXE/DLL。启动器会检查后端是否支持当前 3D 集群移动，防止旧程序被当成可用服务复用。
2026-09-22 对修订 `7928ea3` 的 GitHub ZIP 检查发现 LFS 占位文件；本轮没有重新下载核验最新 ZIP，首次安装继续使用以上 Git + Git LFS 克隆方式。ZIP 解压目录没有 .git，不能直接在其中运行 git lfs pull。空的 `px4_msgs` 历史子模块不参与 Stage 1 构建，不需要递归初始化。

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

`Launcher/config.local.json` 只覆盖本机启动配置，仓库已忽略该文件。常用可选项如下；没有需要时保留默认值。

| 配置项 | 默认值 / 使用场景 |
| --- | --- |
| `ue_executable` | 空值时自动查找 UE 5.8；自动发现失败才填写完整 EXE 路径 |
| `backend_executable` | `Backend/build/Release/DroneBackend.exe`；正常首次构建产物 |
| `startup_timeout` | `180` 秒；首次着色器编译导致客户端就绪超时时可增大 |
| `max_fps` | `20`；三个客户端的帧率上限，可按硬件能力调整 |
| `auto_start` | `true`；设为 `false` 后打开启动器须点“启动” |
| `mock_reset_on_start` | `true`；新建启动器所管理的 Mock Backend 会重新排队，详见第 5 节 |

默认 Backend 地址为本机 `19880/19881`，通常不需要修改。Stage 1 启动器创建的 Backend 只支持本机运行；没有远程 Backend 部署向导。`config.local.json` 不是 UE 地图配置文件，也没有已实现的 `proxy` / `extra_args` 参数；代理按下一节配置。

## 4. 设置地图服务

首次打开 `UE5DroneControl.uproject`，在 Cesium 面板登录自己的 Cesium ion 账号，通过 Token 设置项目使用的凭据。公开仓库已清除开发者凭据，安装插件本身不会提供地图服务权限。
确认 CesiumWorld 关卡的地形/影像服务能加载；如果资产自身设置了 Token，也要更新该资产的配置。不要将自己的 Token 或包含 Token 的资产提交到公开仓库。
离线、代理、401/403 或网络限制可能造成黑地图，这类问题不会由编译解决。

当前默认是 `/Game/Level/CesiumWorld` 的在线 3D 场景，优先使用 Cesium ion 的 Google Photorealistic 3D Tiles（资产 `2275207`）。本项目这条访问路径使用 Cesium ion 授权；不需要另外填写 Google Maps API Key。账号必须有相应资产访问权限，服务额度及适用许可应以自己的 Cesium 账号为准。

### 网络需要代理时

先确认本机代理程序已启动，并确认它提供的 **HTTP 代理端口**。下面的 `127.0.0.1:7897` 只是本机示例，应改为自己的地址和端口。

关闭该工程的 UE 编辑器及 Command / Map / Video 客户端，在项目内创建或编辑 `Saved/Config/WindowsEditor/Engine.ini`，合并下面的键到 `[HTTP]` 节；保留文件中其他配置：

```ini
[HTTP]
HttpProxyAddress=127.0.0.1:7897
HttpNoProxy=127.0.0.1,localhost,::1
```

重新启动 UE 客户端后生效。这里的路径适用于当前启动器使用的 `UnrealEditor.exe -game`，不是将来打包游戏的配置路径。`Saved` 不提交到仓库，代理只影响本机；不要把自己的地址写进共享 `Config/DefaultEngine.ini`。恢复直连时将 `HttpProxyAddress` 置空，或移除该键以恢复引擎的系统代理选择，然后重启客户端。

直接从命令行启动 UE 时也可使用 `-httpproxy=127.0.0.1:7897`；目前双击启动器不会自动添加该参数，把它写进 `Launcher/config.local.json` 的自定义字段也不会传给 UE。仅启动代理程序不代表 UE 一定使用了该代理。可在 `Saved/Stage1/Logs/map-*.log` 中检查 `HttpProxyAddress`，再确认地图有实际地形和建筑；不要把完整日志中的凭据公开。

### 已经配置好的地图与缓存

| 项目 | 当前默认 / 调整位置 |
| --- | --- |
| 地图模式 | 默认 3D；`Config/DefaultGame.ini` 的 `[CommandMap]` |
| 地图中心与相机 | The Meadow 草地，纬度 `1.279445`、经度 `103.862188`、椭球高 `15 m`，相机距离 `2400 m`；同文件的 `[MapStartup]` |
| Mock 初始化位置 | `Backend/mock_execution.json` 的 `startup_formation`，从同一草地中心向东按编号排列，间距 `3 m` |
| 磁盘请求缓存 | `Config/DefaultEngine.ini` 的 CesiumRuntimeSettings：`MaxCacheItems=131072`，`RequestsPerCachePrune=10000`；新 UE 进程读取 |
| 本地瓦片服务 | 默认 `UseLocalTileServer=false`，正常在线演示不需要另启本地瓦片服务器 |

换部署区域时，地图中心和 Mock 初始化位置需要分别调整；已有方案、路线和自定义 Home 不会跟着平移。`15 m` 是相对 WGS84 椭球的高度，不是离地 15 m，也不代表自动贴地。主菜单明确填写的地图坐标优先于默认中心。

缓存会随允许缓存的在线请求积累；当前启动方式在项目根目录使用 `cesium-request-cache.sqlite`。条目上限不是容量字节数，也不保证某区域已下载完整。缓存仍遵守响应头和过期规则，不能替代网络或账号权限；无需反复清空缓存来排查网络问题。详见 [地图位置、缓存及验证边界](MarinaBayStartup.md)。

## 5. 启动

双击 **CheckDroneSecurity.cmd** 检查环境，成功后双击 **DroneSecurityLauncher.cmd**。
启动器等待 Backend、Command、Map、Video 全部 ONLINE。默认本机端口为 **19880/19881**。
首次着色器编译可能较慢，可在本地配置中增加 `startup_timeout`；同时运行三个 UE 客户端需要足够内存和显存。

默认 `mock_reset_on_start=true`：启动器每次新建自己管理的 Mock Backend，都先等待初始化回执，将符合条件的模拟无人机放回草地并按 `3 m` 排列。相关活动 Mock 执行会标记为 `ABORTED`，预约执行会取消，旧方案和路线保留；重置前数据保存在 `Saved/Stage1/data/mock-reset-history`。这是模拟会话重置，不是真机起飞或返航。复用已有 Backend，或只重启 Command / Map / Video，不会执行这个重置。

如果要在重启 Backend 后继续保留已确认的模拟位置，在 `Launcher/config.local.json` 中合并 `"mock_reset_on_start": false`，再重启启动器使配置生效。统一关闭暂停过的执行仍需手动“恢复”。

正常退出使用 **Shutdown System / 关闭系统**：有活动执行时会提示；对启动器拥有且可连接的 Backend，会先请求并等待 Mock 暂停回执，再关闭其拥有的进程。单项 **Stop / Restart** 用于组件维护，不会走整机暂停流程；有活动执行时先在业务界面处理暂停或终止。尤其重启 Backend 会在默认配置下触发上述 Mock 重置。启动器不会终止或自动暂停它复用的外部 Backend。

Mock 演示不要求填写真实无人机连接或视频地址。需要视频流时，可参考 [MediaMTX 安装说明](../tools/MediaMTX/INSTALL.md)；[本地视频演示记录](MediaMTX/README.md) 中的旧工作站路径和录像文件须按本机实际情况调整。真实摄像头接入需单独配置和验证，有视频画面不代表已完成真实无人机飞行验收。

| 现象 | 处理 |
|---|---|
| 找不到 CesiumForUnreal / CesiumRuntime | 将 Cesium 安装到 UE 5.8，再首次编译 |
| 缺少模块 / 由不同引擎版本构建 | 用本机 UE 5.8 重新运行 SetupDroneSecurity.cmd |
| 缺少 DroneBackend.exe | 先运行 SetupDroneSecurity.cmd，检查 VS C++ / vcpkg |
| 后端版本过旧；集群移动提示此客户端无权执行 | 关闭旧 Backend 和客户端，运行 SetupDroneSecurity.cmd，再从 DroneSecurityLauncher.cmd 启动。正常路径是 Backend/build/Release/DroneBackend.exe |
| 提示 Python 或 tkinter 缺失 | 修改 Python 安装，启用 Tcl/Tk 和 Launcher |
| 地图文件极小或 LFS 占位 | `git lfs pull` |
| 地图黑屏、401/403 | 检查自己的 Cesium ion Token、资产权限及网络 |
| 地图模糊、瓦片持续超时 | 检查地图客户端日志、代理是否实际生效及服务可达性；增加缓存不能解决当前请求超时 |
| 只重启 Map 后无人机没有重新排队 | 符合当前规则；只有新建启动器管理的 Mock Backend 才应用初始化重置 |
| 重启 Backend 后任务中止、位置回到草地 | 默认 Mock 会话重置；需要保留时提前设置 `mock_reset_on_start=false` |
| 端口占用或 Launcher already running | 回到已运行启动器；关闭自己启动的冲突实例后重试 |

报告问题时提供 `Saved/environment-check.json`、`Saved/first-run-build.log`（如编译失败）、`Saved/Stage1/Logs` 下对应客户端日志。分享前移除日志中的凭据。

## 验证边界

本轮手册核对依据当前项目配置、启动器和本机 UE 5.8 源码，未在另一台电脑现场重做首次安装。首次运行脚本已有缺失 Cesium/LFS/二进制/错误引擎等自动化场景和启动器回归；安装好依赖后仍需实际启动确认。P5.3 的原始业务验收记录见 `TASK-P5.3-PlanGeometryClosure.md`。

## 历史问题与排查记录

### 2026-09-22 下载安全问题修正

启动入口现直接调用本机 Python，不再通过 PowerShell 绕过执行策略。源码分发不再包含 MediaMTX EXE/ZIP；需要真实视频服务时按 `tools/MediaMTX/INSTALL.md` 单独安装官方发行版。Mock 流程不需要 MediaMTX。
这些是发布内容与启动实现的改进；历史 Defender 报警只定位到整个 ZIP，尚未证实具体触发文件。不要恢复被隔离的旧下载包或关闭防护。

### CesiumIonSaaS 提示 only been partially loaded

此错误表示资产加载不完整，当前内存中的对象不能正常保存。它不是单纯的 Token 无权限提示；需要查看启动日志中更早的 LogLinker/LoadErrors 才能确认原因。字体缺字警告另行处理。

仓库现提供经 UE 5.8 + Cesium 2.28.0 正式清空凭据、保存并用新进程重载验证的 `Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset`。原公开资产本机也能加载，因此此更新不代表已复现并修复所有环境的加载失败。

遇到保存弹窗先选 Continue 保存其他资源，然后关闭编辑器和客户端。把旧 CesiumIonSaaS.uasset 备份到 Content 目录外，再用仓库中的新文件替换；不要在 UE 打开时替换。重新打开工程，配置自己的 Cesium ion Token 并保存。此次替换会清空该资产中的旧 Token，需要重新设置。无需清空 Content、Saved 或整个工程。

若仍报同样错误，请保留完整启动日志并核对插件版本；不要反复 Retry。验证记录见 `Cesium-Asset-Validation.json`。

### 2026-09-22 ZIP 内容实测

检查 GitHub 为修订 `7928ea3` 生成的实际 ZIP：CesiumWorld.umap = 130 字节、CesiumIonSaaS.uasset = 129 字节，二者第一行均为 `version https://git-lfs.github.com/spec/v1`。真正地图为 47,098 字节。因此 map appears to be an asset file 和配置资产 partially loaded 应先检查 LFS 资源是否真实存在，而非先改 Token 或认定二进制损坏。

ZIP 中所有 LFS 资源均需补齐，单独替换一个配置文件不够。管理员可在 GitHub Settings → General → Archives 开启 Include Git LFS objects in archives 后，再验证实际 ZIP 内容；未验证前继续采用 Git LFS 克隆。
