# Windows 首次运行与本机配置

这是源码工程，不是解压即用的打包游戏。Stage 1 是 Mock 模拟演示，包含 Backend、Command、Map、Video；不需要连接真实无人机或安装 ROS。

本手册于 2026-10-30 对照当前启动器和项目配置核对。已安装好依赖的开发电脑可以先运行 `CheckDroneSecurity.cmd`，再运行 `DroneSecurityLauncher.cmd`，选择“DEMO 启动（新加坡）”或“实时模式启动”。检查通过只确认依赖、资源和本机构建文件存在；Cesium 账号权限、地图网络及实际画面仍需启动后确认。

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
| `auto_start` | `false`；打开启动器后等待选择模式。明确设为 `true` 时按 `startup_mode` 自动启动 |
| `startup_mode` | `demo`；仅自动启动时使用，可设 `demo` 或 `realtime` |
| `mock_reset_on_start` | `true`；新建启动器所管理的 Mock Backend 会重新排队，详见第 5 节 |
| `realtime_backend_config` | `Backend/config.yaml`；实时 Backend 的既有注册表、UDP `port_map` 和存储配置 |
| `realtime_http_endpoint` / `realtime_ws_endpoint` | `http://127.0.0.1:8080` / `ws://127.0.0.1:8081/ws`；须与实时 Backend 配置一致 |
| `realtime_runtime_directory` | `Saved/Realtime`；启动器进程记录和日志，不是新建业务数据库 |
| `realtime_gps_timeout` / `realtime_gps_max_age` | `60` / `5.0` 秒；有真实无人机连接时等待 GPS 的时长 / 后端收到真实遥测的最大年龄 |
| `realtime_device_wait` | `8` 秒；发现真实无人机连接的等待窗口 |
| `realtime_computer_location_timeout` | `15` 秒；无真实无人机连接时查询 Windows 系统定位的最长时间 |

DEMO Backend 默认使用本机 `19880/19881`，实时模式使用 `8080/8081`；两种模式不得共用后端端口或启动器运行目录。启动器新建的 Backend 只支持本机运行；没有远程 Backend 部署向导。`config.local.json` 不是 UE 地图配置文件，也没有已实现的 `proxy` / `extra_args` 参数；代理按下一节配置。

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

直接从命令行启动 UE 时也可使用 `-httpproxy=127.0.0.1:7897`；目前双击启动器不会自动添加该参数，把它写进 `Launcher/config.local.json` 的自定义字段也不会传给 UE。仅启动代理程序不代表 UE 一定使用了该代理。可在 DEMO 的 `Saved/Stage1/Logs/map-*.log` 或实时模式的 `Saved/Realtime/Logs/map-*.log` 中检查 `HttpProxyAddress`，再确认地图有实际地形和建筑；不要把完整日志中的凭据公开。

### 已经配置好的地图与缓存

| 项目 | 当前默认 / 调整位置 |
| --- | --- |
| 地图模式 | 默认 3D；`Config/DefaultGame.ini` 的 `[CommandMap]` |
| DEMO 地图中心与相机 | The Meadow 草地，纬度 `1.279445`、经度 `103.862188`、椭球高 `15 m`，相机距离 `2400 m`；默认 `[MapStartup]` 与 DEMO fixture 对应，双按钮启动会传入明确原点 |
| 实时地图中心 | 优先无人机实时 GPS；没有真实无人机连接时使用 Windows 系统定位；条件见第 5 节 |
| Mock 初始化位置 | `Backend/mock_execution.json` 的 `startup_formation`，从同一草地中心向东按编号排列，间距 `3 m` |
| 磁盘请求缓存 | `Config/DefaultEngine.ini` 的 CesiumRuntimeSettings：`MaxCacheItems=131072`，`RequestsPerCachePrune=10000`；新 UE 进程读取 |
| 本地瓦片服务 | 默认 `UseLocalTileServer=false`，正常在线演示不需要另启本地瓦片服务器 |

DEMO 按钮使用随项目提供的新加坡 fixture 原点；`[MapStartup]` 仍为没有明确启动参数时的地图展示默认值。实时模式不通过修改 DEMO fixture 切换地区。已有方案、路线和自定义 Home 不会跟着地图原点平移。`15 m` 是相对 WGS84 椭球的高度，不是离地 15 m，也不代表自动贴地。主菜单明确填写的地图坐标继续优先于默认中心。

缓存会随允许缓存的在线请求积累；当前启动方式在项目根目录使用 `cesium-request-cache.sqlite`。条目上限不是容量字节数，也不保证某区域已下载完整。缓存仍遵守响应头和过期规则，不能替代网络或账号权限；无需反复清空缓存来排查网络问题。详见 [地图位置、缓存及验证边界](MarinaBayStartup.md)。

## 5. 启动

双击 **CheckDroneSecurity.cmd** 检查环境，成功后双击 **DroneSecurityLauncher.cmd**。默认不自动启动，先选择模式：

| 按钮 | 位置与后端 |
| --- | --- |
| **DEMO 启动（新加坡）** | 新加坡 The Meadow；Mock Backend 使用 **19880/19881**，已有业务数据保留在 `Saved/Stage1/data` |
| **实时模式启动** | 优先无人机实时 GPS；无真实连接时用 Windows 系统定位。Backend 使用 **8080/8081** 和 `Backend/config.yaml`，沿用该配置的注册表、`port_map` 与业务数据 |

启动器等待 Backend、Command、Map、Video 全部 ONLINE。实时模式先取得有效位置，再打开三个 UE 客户端。
Map 初始显示按模式区分：**DEMO 保持新加坡 3D，实时模式先打开定位处的 2D 底图**。本次原生检查中，实时所在地的初始三维视图没有地表，而二维卫星影像正常加载，因此实时启动先使用现有通用底图；仍可通过工具栏手动切换 3D。底图配置、授权和来源署名沿用原设置，三维显示取决于当地数据与加载情况。
首次着色器编译可能较慢，可在本地配置中增加 `startup_timeout`；同时运行三个 UE 客户端需要足够内存和显存。

### DEMO 模式

默认 `mock_reset_on_start=true`：DEMO 每次新建自己管理的 Mock Backend，都先等待初始化回执，将符合条件的模拟无人机放回草地并按 `3 m` 排列。相关活动 Mock 执行会标记为 `ABORTED`，预约执行会取消，旧方案和路线保留；重置前数据保存在 `Saved/Stage1/data/mock-reset-history`。这是模拟会话重置，不是真机起飞或返航。复用已有 Backend，或只重启 Command / Map / Video，不会执行这个重置。

如果要在重启 Backend 后继续保留已确认的模拟位置，在 `Launcher/config.local.json` 中合并 `"mock_reset_on_start": false`，再重启启动器使配置生效。统一关闭暂停过的执行仍需手动“恢复”。

### 实时模式

使用无人机 GPS 时，先配置并注册现场无人机，确认 `Backend/config.yaml` 中的 UDP `port_map`、设备地址与存储位置符合现场；没有真实无人机连接时可以使用电脑定位分支。启动器从仓库根目录运行这个配置，保留其现有注册表和业务数据；默认注册表为 `Backend/data/drones.json`。如需本机专用配置，在不提交的 `Launcher/config.local.json` 中覆盖 `realtime_backend_config`，并让实时 HTTP / WS 地址与该配置端口一致。`Saved/Realtime` 保存启动器进程记录、启动器日志、Backend 标准输出和 UE 日志。Backend 自身文件日志沿用所选配置的 `log.file`，默认为 `logs/backend.log`。实时分支不生成 Mock fixture、不播种模拟无人机、不执行 Mock reset，也不创建或启用 `real_execution.json`、不自动开始飞行任务。

有效无人机 GPS 可立即初始化；未取得时，实时启动提供默认 **8 秒**的真实连接发现窗口（`realtime_device_wait`），之后才考虑电脑定位。优先位置来自 `/api/drones` 的真实遥测：必须为 `online`、`gps_fix=true`、`simulation=false`、`telemetry_source=received`，经纬高完整且有效，后端记录的 `telemetry_received_at` 默认不超过 **5 秒**。符合条件时优先当前 active UAV，否则采用最小 slot 的无人机。GPS 海拔按 `椭球高 = AMSL + GeoidSeparationMeters` 转换；启动器与 UE 读取同一 `Config/DefaultGame.ini`，并允许本机 `Saved/Config/WindowsEditor/Game.ini` 覆盖这个区域高程修正。

只有注册表条目或发出的连接探测不算真实连接，连接判断同时要求后端实际收到非模拟遥测。存在这类真实 `online` / `connecting` 无人机但其 GPS 无效时，继续等待该类 GPS，默认上限 **60 秒**（`realtime_gps_timeout`），不会用电脑位置替代已连接飞机。只有完全没有真实无人机连接时，才查询 **Windows 系统定位**，最长 **15 秒**（`realtime_computer_location_timeout`）。Windows 未提供高度时以 **0 m 估计高程**作为地图相机锚点，并在状态中标记估计高程；不把它当作无人机位置、Home 或飞行高度。

系统定位被禁用、拒绝、不可用或超时，会显示明确错误。启动器不自动修改 Windows 定位权限，不用 IP 猜测城市，也不退回新加坡。定位失败只清理启动器自己的进程，不打开任何 UE 角色。初始化后不会因新 GPS 或定位来源变化自动切镜头；重启 Map 会重新按相同优先级定位。旧 Backend 若缺少 `telemetry_receive_time` 能力，会提示重新运行 `SetupDroneSecurity.cmd`；不能通过不断 GET 让旧 GPS 变新。实时模式拒绝复用 DEMO 后端，外部 Backend 的生命周期仍由其启动者管理。

### 关闭和切换模式

正常退出使用 **Shutdown System / 关闭系统**，关闭当前会话后再选择另一个模式。有活动执行时会提示；DEMO 对启动器拥有且可连接的 Backend，先请求并等待 Mock 暂停回执，再关闭其拥有的进程。实时关闭不调用 Mock 暂停，也不会自动暂停真实执行，停止会话前通过现有业务控制处理活动任务。单项 **Stop / Restart** 用于组件维护，不会走整机暂停流程；有活动执行时先处理暂停或终止。尤其 DEMO 重启 Backend 会在默认配置下触发上述 Mock 重置。启动器不会终止或自动暂停它复用的外部 Backend。

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
| 有真实连接但实时模式一直等待定位 | 检查注册表、UDP `port_map`、GPS fix 与最近接收时间；不会用电脑位置代替已连接飞机的无效 GPS |
| 没有真实连接且电脑定位失败 | 检查 Windows 系统定位的启用、权限与可用状态；启动器不自动改权限、不按 IP 猜测城市 |
| 电脑定位状态显示估计高程 | 系统未给出高度，使用 0 m 仅作相机锚点；不作为飞行高度 |
| 实时模式提示不能复用 DEMO 后端 | 核对实时配置和独立端口，不要把 `19880/19881` 的 Mock 会话改名当实时使用 |
| 实时模式提示缺少定位时间信息 | 停止旧 Backend，运行 `SetupDroneSecurity.cmd` 重新编译，再重试 |
| 端口占用或 Launcher already running | 回到已运行启动器；关闭自己启动的冲突实例后重试 |

报告问题时提供 `Saved/environment-check.json`、`Saved/first-run-build.log`（如编译失败）、所选模式 `Saved/Stage1/Logs` 或 `Saved/Realtime/Logs` 下对应客户端日志。分享前移除日志中的凭据。

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
