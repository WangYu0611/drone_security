# 配置部署：单机四端与局域网四机

同一份部署 JSON 支持两种用法：一台 Windows 电脑选择 **All**，同时运行 Backend、Command、Map、Video；或四台电脑分别选择一个角色，通过同一个 Backend 的 HTTP/WebSocket 同步业务状态。每台保留自己的工程和运行文件。

启动器使用实时配置。[2026-11-17 四端实际联通记录](../Evidence/LANDeployment/20261117-live/validation.json) 保存了共享配置的历史实测。[此前 128 项功能测试](../Evidence/LANDeployment/20261117/validation.json) 和 [此前被系统拦截的启动记录](../Evidence/LANDeployment/20261117-local/validation.json) 也保留为历史证据。组件测试、原生界面检查、实际进程启动和现场四机联网是不同证据，不能相互替代。

## 0. 只有一台电脑时，先这样启动

以下以工程目录 `D:\CodexPublish\drone_security` 为例；安装在其他目录时对应替换路径。

1. 按 [首次运行指南](First-Run-Windows.md) 准备 Python、UE 5.8、Cesium 和构建工具。关闭旧 Backend/UE 进程后运行 `D:\CodexPublish\drone_security\SetupDroneSecurity.cmd`，确认默认 Backend EXE 与 UE DLL 都已按当前源码构建。仅拉取源码不会替换它们；已经与当前源码一致的产物不必每次启动重编译。
2. 只使用本机默认配置时，打开启动器，选择 **All**，共享配置留空，点击 **启动系统**。如需指定共享配置，可将 `D:\CodexPublish\drone_security\Launcher\deployment.local.example.json` 复制为同目录 `deployment.json`，再在启动器选用它。已有文件可按需编辑。本机模板内容如下。

```json
{
  "schema_version": 1,
  "mode": "realtime",
  "hosts": {
    "Backend": "127.0.0.1",
    "Command": "127.0.0.1",
    "Map": "127.0.0.1",
    "Video": "127.0.0.1"
  },
  "http_port": 8080,
  "ws_port": 8081
}
```

3. 启动器先启动 Backend，再启动三个 UE 客户端。等待四项 ONLINE/就绪。可在启动前输入经纬度或选用常用地点，直接打开指定的地图位置。
4. 使用 **关闭系统** 结束本次会话。本机默认配置的运行记录位于 `Saved/Realtime`，采用共享配置的 All 会话记录位于 `Saved/Deployment/realtime/All`。两者使用各自配置指定的 Backend 业务存储。

`127.0.0.1` 始终表示当前电脑，因此这个文件用于同机测试。四个角色也可以都填写本机实际网卡 IPv4。选择 **All** 时，四项 IP 必须全部属于本机；某项指向其他电脑会被拒绝。将来分到四台电脑时，按后文把四项改成各机器的实际局域网 IPv4，每台只选自己的单个角色。

曾使用 DEMO 共享配置时，可将 `mode` 改为 `realtime`，并把端口核对为实时部署使用的 HTTP/WS 端口。单机默认值为 `8080/8081`。如果旧文件含 `hosts.Pilot`，使用示例中的四个角色字段准备实时配置。

若双击入口找不到 Python，但本机已有带 Tk 的 Python 3.10+，可创建 `D:\CodexPublish\drone_security\Launcher\python.path`，仅写一行实际解释器绝对路径，例如 `C:\Python313\python.exe`，不加引号、不写 JSON。它只影响本机启动脚本，不改系统 PATH，也不提交 Git。`py` 或 `python` 已正常可用时无需此文件。

## 1. 准备四台电脑

使用同一项目版本，并记录每台电脑接入同一局域网的实际 IPv4 地址。不同物理电脑应有各自的稳定地址；可使用固定地址或 DHCP 地址保留。同一电脑承载多个角色时，对应字段允许使用相同 IP。地址改变后需要同步更新部署文件。

| 本机角色 | 运行内容 | 本机构建与依赖 |
| --- | --- | --- |
| Backend | 仅 `DroneBackend.exe` | Python 3.10+ 含 Tk；从源码构建需要 VS C++ 桌面开发、Windows SDK、CMake、vcpkg。运行 `SetupDroneBackend.cmd`，不要求安装 UE/Cesium |
| Command | 仅指挥端 UE 进程 | Python 3.10+ 含 Tk、UE 5.8、Cesium、完整 LFS 资源及已编译 UE 模块 |
| Map | 仅地图端 UE 进程 | 与 Command 相同；按 [首次运行指南](First-Run-Windows.md#4-设置地图服务) 配置地图授权与网络 |
| Video | 仅视频端 UE 进程 | 与 Command 相同；需要播放时另外配置可达的视频源 |

三个 UE 角色仍使用同一个完整 UE 工程。可沿用 `SetupDroneSecurity.cmd` 完成首次编译；运行时的角色预检不会因为客户端机器没有 Backend EXE 或 Backend YAML 而阻止启动。`CheckDroneSecurity.cmd` 仍执行完整单机环境检查。点击 **启动系统** 时，启动器检查所选角色的依赖。

Backend 单独构建入口 `SetupDroneBackend.cmd` 调用 `Launcher/Start.cmd --build-backend`，产物为 `Backend/build/Release/DroneBackend.exe`。它使用本机 `realtime_backend_config` 指向的既有 YAML，不生成飞行策略。构建日志为 `Saved/first-run-build.log`。更新源码后应重新构建相应 EXE/DLL；拉取代码不会替换正在运行的旧程序。启动器默认读取这个 Backend 路径和 `Binaries/Win64/UnrealEditor-UE5DroneControl.dll`；仅在其他测试构建目录生成新文件，不会更新默认启动产物。若本机配置覆盖了 EXE 路径，也须确认被选中的实际文件与当前源码匹配。

## 2. 准备一份共享配置

四机使用 [deployment.example.json](../Launcher/deployment.example.json) 为模板，首次创建本地 `Launcher/deployment.json`，把四个地址改成各机器的实际网卡 IPv4；单机则使用 [deployment.local.example.json](../Launcher/deployment.local.example.json)。实际 `deployment.json` 被 Git 忽略；已有配置直接编辑，不要覆盖掉自己的地址。

```json
{
  "schema_version": 1,
  "mode": "realtime",
  "hosts": {
    "Backend": "192.168.10.10",
    "Command": "192.168.10.11",
    "Map": "192.168.10.12",
    "Video": "192.168.10.13"
  },
  "http_port": 8080,
  "ws_port": 8081
}
```

以上是示例地址。规则如下：

- 顶层必须正好包含示例中的五个字段，`schema_version` 为整数 `1`，`mode` 为 `realtime`。
- `hosts` 必须正好包含大小写一致的四个角色，不能增加 `All` 字段。同机角色允许相同地址或 `127.0.0.1`；多机互连使用各机真实局域网 IPv4，不能用回环地址指代远程电脑。未指定地址、`0.0.0.0/8`、组播和保留地址无效；域名、IPv6、带端口或网段后缀的地址不受支持。
- 两个端口必须是 `1–65535` 的不同整数。未知字段、重复 JSON 字段或拼错的角色会报错。

将**同一份配置内容复制到四台机器**。各台可以使用不同的本地文件路径；在启动器中选择各自那份文件即可。这里的“共享配置”不要求共享盘，也不共享 `Saved`、构建缓存、注册表或业务数据目录。

四个 IP 的用途不同：`Backend` IP 与两个端口共同生成所有角色使用的 `http://Backend-IP:端口` 和 `ws://Backend-IP:端口/ws`；其他三个 IP 用于核对“这台电脑是否选了正确角色”。启动前启动器会短暂绑定本机角色 IP 的一个临时 UDP 端口并立即关闭，不发送数据；**All** 会逐项检查四个角色 IP。任一所选角色 IP 不属于本机则停止启动并显示角色和地址。

共享文件指定 HTTP/WS 地址、端口和各角色的运行目录。本机 EXE 路径、实时 Backend YAML 路径、帧率和定位等待设置来自 `Launcher/config.local.json` 及其项目默认值。

## 3. 按顺序启动

1. 在 Backend 电脑打开 `DroneSecurityLauncher.cmd`，将本机角色从 **All** 改为 **Backend**，选择共享 JSON，点击 **启动系统**，等待 Backend 就绪。
2. 在 Command 电脑选择 **Command** 和同一份配置内容，点击 **启动系统**。
3. 在 Map 电脑选择 `Map`，然后在 Video 电脑选择 `Video`，各自启动。两个角色也可以在 Backend 就绪后分别启动。
4. 检查各启动器显示的角色、Backend 地址和本机/远程标识。每台只管理自己选择的角色；其他角色的在线状态来自 Backend。

客户端机器连接不到 Backend 时会明确失败，不会自行创建另一个 Backend。同角色已在线时拒绝重复启动；已在线的其他角色不会阻止本机启动。兼容的既有 Backend 可以被复用，启动器不会取得它的停止或重启权限。

每台的 `READY` 要求 Backend 可用，并且本机客户端进程及其订阅、加载完成状态和新鲜心跳正常；Backend 角色只要求 Backend；**All** 要求本机四端全部正常。单角色部署时，其他机器未启动不会使本机就绪状态降级，也不能用远程同角色进程替代本机进程。请让四台机器的系统时间保持同步，心跳与遥测新鲜度检查依赖时间戳。

**Map** 或 **All** 启动时优先使用输入的经纬度。留空时默认使用 Map 电脑的 Windows 系统定位，无人机已连接时也适用。需要优先使用 UAV GPS 时，可在 Map 电脑的 `Launcher/config.local.json` 中将 `realtime_map_location_source` 设为 `uav`。单独启动 Command、Video 或 Backend 无需初始化地图位置。常用地点保存在运行 Map 的电脑的 `Saved/Launcher/map-locations.json`。

本机选择以 `role` 和共享文件 `path` 保存到 `Saved/Launcher/deployment-selection.json`，下次打开可恢复。默认 `auto_start=false`。开启后，启动器使用已保存的共享配置；没有保存的共享配置时，使用 **All** 和本机配置。

## 4. 网络、数据和停止行为

Backend 主机的网络与防火墙需要允许其他三台机器访问共享配置指定的 **HTTP TCP 端口和 WebSocket TCP 端口**。客户端需要能连接这两个端口；只通 HTTP 还不够。仅为所需局域网配置访问范围，启动器不会修改 Windows 防火墙或网卡配置。若客户端使用 HTTP 代理，需要为 Backend 地址配置直连，避免内网 API 被发往代理。

部署日志和进程记录位于各机 `Saved/Deployment/realtime/{role}`。实时 Backend 仍以项目根目录为工作目录，沿用 `realtime_backend_config` 中的 `storage.path`、UDP `port_map`、注册表、执行策略和 `log.file`。共享端口通过启动参数覆盖本次进程的 HTTP/WS 监听端口，不改写该 YAML，也不创建或启用 `real_execution.json`。启动部署不自动开始飞行任务。

每个 UE 客户端的用户目录通过 `-UserDir` 隔离到运行目录下的 `Clients/{客户端角色}/{实例 ID}`，包括其中的内置浏览器 CEF 缓存。三个角色并发启动时使用独立缓存，每次启动会产生新的实例目录。本机默认配置也使用同样的隔离方式，位于 `Saved/Realtime`。业务状态由 Backend 统一保存。

“Stop / Restart”只操作本机拥有的角色，远程维护按钮不可用。客户端关闭不会停止、暂停或重启远程 Backend，也不会影响其他客户端进程。Backend 机器关闭自己启动的 Backend 会让其他角色断开；实时模式不会因此自动暂停真实执行，应先通过现有业务控制处理活动任务。

修改共享 IP 或端口时，先处理活动任务并停止受影响会话，把相同修改复制到四机，再按 Backend 优先的顺序启动。改变 Backend 地址或端口时，四机都需重启对应会话。切回本机默认配置时，选择 **All** 并清空共享配置路径。只切换地图地点时，可在 Map 电脑上输入坐标并应用，启动器仅重启它管理的 Map 窗口。

## 5. 视频与机载链路独立配置

`hosts.Video` 代表运行 Video UI 的电脑，**不代表摄像头或 MediaMTX 服务器**。无人机注册表中的 `video_url`、MediaMTX 地址、WebRTC 页面及其媒体传输端口仍按实际部署设置。媒体服务器可以与 Backend、Video 同机，也可以单独部署；启动器不会自动替换视频 URL 中的 `localhost` 或把流地址改成 `hosts.Video`。

从 Video 电脑验证视频 URL 可达，并按 [MediaMTX 说明](MediaMTX/README.md) 检查信令和媒体连接。HTTP/WS 业务状态同步成功不代表视频已经可播；反过来也一样。Jetson/PX4 的 UDP 地址、端口和飞行标定继续由原有机载/Backend 配置负责，不属于这份四端 JSON。

## 6. 排查与验证范围

| 现象 | 检查 |
| --- | --- |
| 提示本机不能绑定角色 IP | 选中的角色、该电脑网卡 IPv4、DHCP 是否改变地址 |
| 提示先启动 Backend | Backend 机器是否已就绪、共享文件是否一致、HTTP TCP 端口与防火墙 |
| HTTP 正常但 WebSocket 不可达 | 共享 `ws_port`、Backend WS 监听、客户端到该端口的网络路径 |
| 提示同角色已在线 | 是否已有该角色窗口/另一台机器误选相同角色；先在拥有它的电脑关闭 |
| Map 等待或定位失败 | 检查 Map 电脑的 Windows 定位设置。选择 `uav` 时另查 GPS 新鲜度 |
| 视频无画面但其他状态正常 | 实际 `video_url`、MediaMTX、视频信令/媒体端口；不要把 Video 角色 IP 当成流服务器 |
| 后端能力过旧或启动参数不支持 | 停止旧 EXE，更新同一版本并重新运行 `SetupDroneBackend.cmd`；UE 客户端同步更新模块 |

自动化测试覆盖共享配置验证、角色启动与进程所有权、角色预检、端口覆盖和失败清理。2026-11-17 解除本机程序拦截后，修复了同机 UE 客户端共享 CEF 缓存的启动冲突，并完成 [本机四端实际联通记录](../Evidence/LANDeployment/20261117-live/validation.json)：147 项启动器测试、212 项后端测试通过；四个实际进程就绪，三个客户端 HTTP/WS 状态加载与持续心跳正常，原生界面双向语言同步及 Video 独立重启通过。原生 UI 截图与组件/接口证据在记录中分别列出。

这是单台电脑使用 `127.0.0.1` 的 DEMO 实测，不代表四台物理机器的网络、防火墙、实际视频流或真机飞行已验证。[此前被系统拦截的单机记录](../Evidence/LANDeployment/20261117-local/validation.json) 和 [此前 128 项记录](../Evidence/LANDeployment/20261117/validation.json) 保留为历史证据。
