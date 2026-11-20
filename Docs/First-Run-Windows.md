# Windows 首次运行与本机配置

这是需要本机编译的源码工程，启动器运行实时版的 Backend、Command、Map 和 Video。此前 Stage 1 的 Mock 模拟演示记录保留在 [历史演示手册](Stage1/Stage1-Demo-Runbook.md)。

已安装好依赖的开发电脑可以先运行 `CheckDroneSecurity.cmd`，再运行 `DroneSecurityLauncher.cmd` 并点击 **启动系统**。检查通过只确认依赖、资源和本机构建文件存在。Cesium 账号权限、地图网络及实际画面仍需启动后确认。

单机默认选择 **All**，共享配置留空即可启动实时版。需要 [共享配置部署](LANDeployment.md) 时，可选用 `Launcher/deployment.local.example.json`，它使用四项 `127.0.0.1`、`mode=realtime` 和 `8080/8081`。四机部署时把四项换成实际局域网 IP，每台分别选 Backend、Command、Map 或 Video。只运行 Backend 的电脑使用 `SetupDroneBackend.cmd`，无需安装 UE/Cesium。`CheckDroneSecurity.cmd` 检查完整单机环境，启动器按本机角色检查所需依赖。

## 1. 安装依赖

1. Unreal Engine **5.8**。
2. Visual Studio **2022 或 2026** 的“使用 C++ 的桌面开发”和“使用 C++ 的游戏开发”，包括 Windows SDK、MSVC、C++ CMake tools for Windows、vcpkg。首次构建脚本支持这两个版本的 CMake 生成器，不代表每种 MSVC / SDK 组合都已验收；仅安装 VS 编辑器本身不够。
3. **Cesium for Unreal**：在 Epic Games Launcher 的 Fab 库中获取并安装到 **UE 5.8**。安装到其他引擎版本不会生效。插件不随本仓库分发，不要直接关闭插件来绕过报错，工程源码依赖 CesiumRuntime。
4. Python **3.10 或更高版本**，安装时包括 Tcl/Tk、Python Launcher，或将 Python 加入 PATH。Stage 1 启动器只使用标准库，不需要安装根目录的全部历史 requirements。
5. Git 和 Git LFS。

Cesium 官方安装和 Token 配置说明：<https://cesium.com/learn/unreal/unreal-quickstart/>。

Python 已安装但启动脚本找不到 `py`/`python` 时，可在工程的 `Launcher/python.path` 文件中仅写一行解释器绝对路径，例如 `C:\Python313\python.exe`。路径不加引号，不写成 JSON；解释器必须是带 Tcl/Tk 的 Python 3.10+。这个文件只作用于本机入口，不修改系统 PATH，也不提交到 Git。正常安装的 `py` 或 `python` 可用时，无需创建它。

## 2. 获取完整源码和资源

建议使用一个固定目录，例如 `C:\Projects\drone_security`，后续直接在该目录更新。

```powershell
git lfs install
git clone https://github.com/WangYu0611/drone_security.git C:\Projects\drone_security
cd C:\Projects\drone_security
git lfs pull
```

已有克隆先用 `git status` 检查本地修改；保留自己的修改后，再执行 `git pull --ff-only` 和 `git lfs pull`。不要覆盖本机 Cesium 凭据资源或删除 `Saved/Stage1/data` 中的业务数据。
源码更新后仍需关闭正在运行的 Backend 和客户端，再运行 `SetupDroneSecurity.cmd`（Backend-only 电脑用 `SetupDroneBackend.cmd`）；拉取代码不会自动更新本机 EXE/DLL。默认实际启动产物为 `Backend/build/Release/DroneBackend.exe` 与 `Binaries/Win64/UnrealEditor-UE5DroneControl.dll`，二者必须与当前源码匹配；在其他测试构建目录编译成功不会自动更新这两个文件。若本机覆盖了 EXE 路径，确认该路径指向的实际文件也是当前版本。启动器会检查后端是否支持当前 3D 集群移动，防止旧程序被当成可用服务复用。
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
| `auto_start` | `false`，打开启动器后等待点击。设为 `true` 时使用已保存的实时共享配置，或使用单机默认配置 |
| `realtime_backend_config` | `Backend/config.yaml`；实时 Backend 的既有注册表、UDP `port_map` 和存储配置 |
| `realtime_http_endpoint` / `realtime_ws_endpoint` | `http://127.0.0.1:8080` / `ws://127.0.0.1:8081/ws`；须与实时 Backend 配置一致 |
| `realtime_runtime_directory` | `Saved/Realtime`；启动器进程记录和日志，不是新建业务数据库 |
| `realtime_gps_timeout` / `realtime_gps_max_age` | `60` / `5.0` 秒；有真实无人机连接时等待 GPS 的时长 / 后端收到真实遥测的最大年龄 |
| `realtime_device_wait` | `8` 秒；发现真实无人机连接的等待窗口 |
| `realtime_computer_location_timeout` | `15` 秒；无真实无人机连接时查询 Windows 系统定位的最长时间 |

默认 **All** 使用本机实时端口 `8080/8081`。[共享部署 JSON](LANDeployment.md#2-准备一份共享配置) 可覆盖四个角色 IP 和两个 TCP 端口。同一台电脑的角色可以使用相同 IP 或 `127.0.0.1`，远程电脑使用实际局域网 IP。选择 **All** 时四项 IP 都必须属于本机。共享部署运行记录位于 `Saved/Deployment/realtime/{role}`。四机部署先启动 Backend 机器，再启动三个客户端。客户端启动要求远程 Backend 可达。`config.local.json` 保存本机程序路径和 `realtime_backend_config`，代理按下一节配置。

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

重新启动 UE 客户端后生效。多机部署时还要把 Backend 的实际局域网 IP 加入 `HttpNoProxy`，使内网请求直连。这里的路径适用于当前启动器使用的 `UnrealEditor.exe -game`，不是将来打包游戏的配置路径。`Saved` 不提交到仓库，代理只影响本机；不要把自己的地址写进共享 `Config/DefaultEngine.ini`。恢复直连时将 `HttpProxyAddress` 置空，或移除该键以恢复引擎的系统代理选择，然后重启客户端。

直接从命令行启动 UE 时也可使用 `-httpproxy=127.0.0.1:7897`。通过启动器运行时，请使用上面所述的 `Engine.ini` 配置。可在 `Saved/Realtime/Logs/map-*.log` 或共享部署的 `Saved/Deployment/realtime/{role}/Logs/map-*.log` 中检查 `HttpProxyAddress`，再确认地图有实际地形和建筑。分享前移除日志中的凭据。

### 已经配置好的地图与缓存

| 项目 | 当前默认 / 调整位置 |
| --- | --- |
| 地图模式 | 从启动器打开时先显示 2D 底图，工具栏可切换 3D |
| 地图中心 | 启动器输入的经纬度优先。留空时使用下文所述的自动定位 |
| 磁盘请求缓存 | `Config/DefaultEngine.ini` 的 CesiumRuntimeSettings：`MaxCacheItems=131072`，`RequestsPerCachePrune=10000`；新 UE 进程读取 |
| 本地瓦片服务 | 默认 `UseLocalTileServer=false`，正常在线演示不需要另启本地瓦片服务器 |

缓存会随允许缓存的在线请求积累；当前启动方式在项目根目录使用 `cesium-request-cache.sqlite`。条目上限不是容量字节数，也不保证某区域已下载完整。缓存仍遵守响应头和过期规则，不能替代网络或账号权限；无需反复清空缓存来排查网络问题。详见 [地图位置、缓存及验证边界](MarinaBayStartup.md)。

## 5. 启动

双击 **CheckDroneSecurity.cmd** 检查完整单机环境，成功后运行 **DroneSecurityLauncher.cmd**。默认 **All** 使用本机配置。选用实时共享 JSON 时，同样点击 **启动系统** 并等待各角色就绪。默认本机 Backend 使用 **8080/8081** 和 `Backend/config.yaml`。具体步骤见 [单机与共享部署](LANDeployment.md#0-只有一台电脑时先这样启动)。

启动器在定位后打开 Map。Map 初始显示 2D 底图，可通过工具栏切换 3D。三维显示取决于当地数据、Cesium 授权和网络加载情况。
首次着色器编译可能较慢，可在本地配置中增加 `startup_timeout`；同时运行三个 UE 客户端需要足够内存和显存。

### 地图位置设置

1. 在地图位置区域填写十进制纬度和经度。纬度的支持范围是 `±85.05112878°`，经度是 `±180°`。北纬、东经用正值，南纬、西经用负值。
2. 点击 **新增常用地点** 保存。名称可选，留空时显示经纬度。选择列表项会填入对应经纬度，点击 **删除选中地点** 可移除该项。
3. 启动前填写坐标，可跳过 GPS 和 Windows 定位等待。Map 正在运行时，点击 **切换地点** 应用坐标。启动器仅重启它管理的 Map 进程。

常用地点保存在本机 `Saved/Launcher/map-locations.json`。输入坐标仅设置 Map 视图地点，无人机位置、Home 和航点使用各自的业务坐标。

### 实时模式

使用无人机 GPS 时，先配置并注册现场无人机，确认 `Backend/config.yaml` 中的 UDP `port_map`、设备地址与存储位置符合现场；没有真实无人机连接时可以使用电脑定位分支。启动器从仓库根目录运行这个配置，保留其现有注册表和业务数据；默认注册表为 `Backend/data/drones.json`。如需本机专用配置，在不提交的 `Launcher/config.local.json` 中覆盖 `realtime_backend_config`，并让实时 HTTP / WS 地址与该配置端口一致。`Saved/Realtime` 保存启动器进程记录、启动器日志、Backend 标准输出和 UE 日志。Backend 自身文件日志沿用所选配置的 `log.file`，默认为 `logs/backend.log`。实时分支不生成 Mock fixture、不播种模拟无人机、不执行 Mock reset，也不创建或启用 `real_execution.json`、不自动开始飞行任务。

手动坐标留空时，有效无人机 GPS 可立即初始化。未取得时，默认等待 **8 秒**的真实连接发现窗口（`realtime_device_wait`），之后才考虑电脑定位。优先位置来自 `/api/drones` 的真实遥测，要求 `online`、`gps_fix=true`、`simulation=false`、`telemetry_source=received`、经纬高有效，且 `telemetry_received_at` 默认不超过 **5 秒**。符合条件时优先当前 active UAV，否则采用最小 slot 的无人机。GPS 海拔按 `椭球高 = AMSL + GeoidSeparationMeters` 转换。启动器与 UE 读取同一 `Config/DefaultGame.ini`，并允许 `Saved/Config/WindowsEditor/Game.ini` 覆盖高程修正。

只有注册表条目或发出的连接探测不算真实连接，连接判断同时要求后端实际收到非模拟遥测。存在这类真实 `online` / `connecting` 无人机但其 GPS 无效时，继续等待该类 GPS，默认上限 **60 秒**（`realtime_gps_timeout`），不会用电脑位置替代已连接飞机。只有完全没有真实无人机连接时，才查询 **Windows 系统定位**，最长 **15 秒**（`realtime_computer_location_timeout`）。Windows 未提供高度时以 **0 m 估计高程**作为地图相机锚点，并在状态中标记估计高程；不把它当作无人机位置、Home 或飞行高度。

系统定位被禁用、拒绝、不可用或超时，会显示错误。可填写经纬度再启动。自动定位失败时，启动器清理自己创建的启动进程。新收到的 GPS 数据供实时状态使用，视图切换由操作者发起。旧 Backend 若缺少 `telemetry_receive_time` 能力，会提示重新运行 `SetupDroneSecurity.cmd`。实时启动仅接受实时 Backend，外部进程仍由其启动者管理。

### 关闭

正常退出使用 **Shutdown System / 关闭系统**。有活动执行时，先通过业务界面处理任务，再停止会话。单项 **Stop / Restart** 用于维护启动器管理的组件。外部 Backend 的生命周期由其启动者管理。

Mock 演示不要求填写真实无人机连接或视频地址。需要视频流时，可参考 [MediaMTX 安装说明](../tools/MediaMTX/INSTALL.md)；[本地视频演示记录](MediaMTX/README.md) 中的旧工作站路径和录像文件须按本机实际情况调整。真实摄像头接入需单独配置和验证，有视频画面不代表已完成真实无人机飞行验收。

| 现象 | 处理 |
|---|---|
| 找不到 CesiumForUnreal / CesiumRuntime | 将 Cesium 安装到 UE 5.8，再首次编译 |
| 缺少模块 / 由不同引擎版本构建 | 用本机 UE 5.8 重新运行 SetupDroneSecurity.cmd |
| 缺少 DroneBackend.exe | 先运行 SetupDroneSecurity.cmd，检查 VS C++ / vcpkg |
| 后端版本过旧；集群移动提示此客户端无权执行 | 关闭旧 Backend 和客户端，运行 SetupDroneSecurity.cmd，再从 DroneSecurityLauncher.cmd 启动。正常路径是 Backend/build/Release/DroneBackend.exe |
| 提示 Python 或 tkinter 缺失 | 安装带 Tcl/Tk 的 Python 3.10+；已安装但入口找不到时，在 Launcher/python.path 写实际解释器绝对路径 |
| 地图文件极小或 LFS 占位 | `git lfs pull` |
| 地图黑屏、401/403 | 检查自己的 Cesium ion Token、资产权限及网络 |
| 地图模糊、瓦片持续超时 | 检查地图客户端日志、代理是否实际生效及服务可达性；增加缓存不能解决当前请求超时 |
| 有真实连接但一直等待定位 | 检查注册表、UDP `port_map`、GPS fix 与最近接收时间，或手动填写地图经纬度 |
| 没有真实连接且电脑定位失败 | 检查 Windows 系统定位的启用、权限与可用状态；启动器不自动改权限、不按 IP 猜测城市 |
| 电脑定位状态显示估计高程 | 系统未给出高度，使用 0 m 仅作相机锚点；不作为飞行高度 |
| 旧共享配置显示模式无效 | 使用 `mode=realtime` 的共享文件，并核对实时端口及四个角色 IP |
| 启动提示 Backend 为 Mock | 核对所填 HTTP/WS 端口对应实时 Backend |
| 实时模式提示缺少定位时间信息 | 停止旧 Backend，运行 `SetupDroneSecurity.cmd` 重新编译，再重试 |
| 端口占用或 Launcher already running | 回到已运行启动器；关闭自己启动的冲突实例后重试 |

报告问题时提供 `Saved/environment-check.json`、`Saved/first-run-build.log`（如编译失败），以及 `Saved/Realtime/Logs` 或 `Saved/Deployment/realtime/{role}/Logs` 下对应的客户端日志。分享前移除日志中的凭据。

## 验证边界

本轮手册核对依据当前项目配置、启动器和本机 UE 5.8 源码，未在另一台电脑现场重做首次安装。首次运行脚本已有缺失 Cesium/LFS/二进制/错误引擎等自动化场景和启动器回归；安装好依赖后仍需实际启动确认。P5.3 的原始业务验收记录见 `TASK-P5.3-PlanGeometryClosure.md`。

2026-11-17 [此前部署验证](../Evidence/LANDeployment/20261117/validation.json) 中的 128 项是共享配置 **All** 支持之前的结果；新增 All、重复/回环 IP、Python 路径入口的验证与本机实际启动结果见 [单机联测记录](../Evidence/LANDeployment/20261117-local/validation.json)。单机测试不等于现场四机联网、视频源或真机飞行验收。

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
