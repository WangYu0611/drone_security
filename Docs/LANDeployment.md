# 局域网四机部署

四台 Windows 电脑分别运行 **Backend、Command、Map、Video**。每台保留自己的工程和运行文件，通过同一个 Backend 的 HTTP/WebSocket 同步业务状态。启动器的默认角色 **All** 保留原来的单机四端流程。

本说明对应 2026-11-17 的实现。配置、组件测试、隔离后端测试和原生 UI 检查分别记录在 [验证记录](../Evidence/LANDeployment/20261117/validation.json)；这些记录不等于已完成现场四台电脑的联网验收。

## 1. 准备四台电脑

使用同一项目版本，并为四台电脑配置不同、稳定的 IPv4 地址。可使用固定地址或 DHCP 地址保留；地址改变后需要同步更新部署文件。

| 本机角色 | 运行内容 | 本机构建与依赖 |
| --- | --- | --- |
| Backend | 仅 `DroneBackend.exe` | Python 3.10+ 含 Tk；从源码构建需要 VS C++ 桌面开发、Windows SDK、CMake、vcpkg。运行 `SetupDroneBackend.cmd`，不要求安装 UE/Cesium |
| Command | 仅指挥端 UE 进程 | Python 3.10+ 含 Tk、UE 5.8、Cesium、完整 LFS 资源及已编译 UE 模块 |
| Map | 仅地图端 UE 进程 | 与 Command 相同；按 [首次运行指南](First-Run-Windows.md#4-设置地图服务) 配置地图授权与网络 |
| Video | 仅视频端 UE 进程 | 与 Command 相同；需要播放时另外配置可达的视频源 |

三个 UE 角色仍使用同一个完整 UE 工程。可沿用 `SetupDroneSecurity.cmd` 完成首次编译；运行时的角色预检不会因为客户端机器没有 Backend EXE 或 Backend YAML 而阻止启动。`CheckDroneSecurity.cmd` 仍执行完整单机环境检查；部署角色的实际预检在点击“启动本机角色”时进行。

Backend 单独构建入口 `SetupDroneBackend.cmd` 调用 `Launcher/Start.cmd --build-backend`，产物为 `Backend/build/Release/DroneBackend.exe`。它使用本机 `realtime_backend_config` 指向的既有 YAML，不生成飞行策略。构建日志为 `Saved/first-run-build.log`。更新源码后应重新构建相应 EXE/DLL；拉取代码不会替换正在运行的旧程序。

## 2. 准备一份共享配置

以 [deployment.example.json](../Launcher/deployment.example.json) 为模板，首次创建本地 `Launcher/deployment.json`，把四个地址改成各机器的实际网卡 IPv4。该本地文件被 Git 忽略；已有配置直接编辑，不要覆盖掉自己的地址。

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

- 顶层必须正好包含示例中的五个字段，`schema_version` 为整数 `1`，`mode` 为 `realtime` 或 `demo`。
- `hosts` 必须正好包含大小写一致的四个角色；四个 IPv4 地址唯一，不能是回环、未指定、组播或保留地址。域名、IPv6、带端口或网段后缀的地址不受支持。
- 两个端口必须是 `1–65535` 的不同整数。未知字段、重复 JSON 字段或拼错的角色会报错。

将**同一份配置内容复制到四台机器**。各台可以使用不同的本地文件路径；在启动器中选择各自那份文件即可。这里的“共享配置”不要求共享盘，也不共享 `Saved`、构建缓存、注册表或业务数据目录。

四个 IP 的用途不同：`Backend` IP 与两个端口共同生成所有角色使用的 `http://Backend-IP:端口` 和 `ws://Backend-IP:端口/ws`；其他三个 IP 用于核对“这台电脑是否选了正确角色”。启动前启动器会短暂绑定本机角色 IP 的一个临时 UDP 端口并立即关闭，不发送数据。IP 不属于本机则停止启动并显示角色和地址。

共享文件决定部署模式、HTTP/WS 地址和端口，以及各角色运行目录。`Launcher/config.local.json` 中旧的单机网络字段不能覆盖它；本机 EXE 路径、实时 Backend YAML 路径、帧率和定位等待设置仍来自本机配置。

## 3. 按顺序启动

1. 在 Backend 电脑打开 `DroneSecurityLauncher.cmd`，将“本机角色 / Local role”从 `All` 改成 `Backend`，选择共享 JSON，点击“启动本机角色 / Start selected role”。等待 Backend 就绪。
2. 在 Command 电脑选择 `Command` 和同一份配置内容，点击“启动本机角色”。
3. 在 Map 电脑选择 `Map`，然后在 Video 电脑选择 `Video`，各自启动。两个角色也可以在 Backend 就绪后分别启动。
4. 检查各启动器显示的角色、Backend 地址和本机/远程标识。每台只管理自己选择的角色；其他角色的在线状态来自 Backend。

客户端机器连接不到 Backend 时会明确失败，不会自行创建另一个 Backend。同角色已在线时拒绝重复启动；已在线的其他角色不会阻止本机启动。兼容的既有 Backend 可以被复用，启动器不会取得它的停止或重启权限。

每台的 `READY` 要求 Backend 可用，并且本机客户端进程及其订阅、加载完成状态和新鲜心跳正常；Backend 角色只要求 Backend。其他机器未启动不会使本机就绪状态降级，也不能用远程同角色进程替代本机进程。请让四台机器的系统时间保持同步，心跳与遥测新鲜度检查依赖时间戳。

实时部署只有 **Map** 角色执行原有 GPS/Windows 定位初始化。Command、Video 和 Backend 不因本机无定位权限而等待地图位置。Map 有真实连接时优先有效 UAV GPS；没有真实连接才尝试 Map 电脑的 Windows 定位。定位失败不会自动回落到 DEMO。

本机选择仅以 `role` 和共享文件 `path` 保存到 `Saved/Launcher/deployment-selection.json`，不重复保存 IP。下次打开可恢复该选择。默认 `auto_start=false`；如本机明确开启自动启动，已保存的部署角色按共享文件的 `mode` 启动，`All` 仍按本机 `startup_mode` 启动。

## 4. 网络、数据和停止行为

Backend 主机的网络与防火墙需要允许其他三台机器访问共享配置指定的 **HTTP TCP 端口和 WebSocket TCP 端口**。客户端需要能连接这两个端口；只通 HTTP 还不够。仅为所需局域网配置访问范围，启动器不会修改 Windows 防火墙或网卡配置。若客户端使用 HTTP 代理，需要为 Backend 地址配置直连，避免内网 API 被发往代理。

部署日志和进程记录位于各机 `Saved/Deployment/{mode}/{role}`。实时 Backend 仍以项目根目录为工作目录，沿用 `realtime_backend_config` 中的 `storage.path`、UDP `port_map`、注册表、执行策略和 `log.file`。共享端口通过启动参数覆盖本次进程的 HTTP/WS 监听端口，不改写该 YAML，也不创建或启用 `real_execution.json`。启动部署不自动开始飞行任务。

DEMO Backend 使用 `Saved/Deployment/demo/Backend/data`，按原有 Mock 规则初始化或重置；客户端不播种自己的业务机队。部署流程不会把 `Saved/Stage1/data` 的旧方案搬迁到新目录，也不会把业务状态复制到三台客户端。重启自有 DEMO Backend 是否重置仍由本机 `mock_reset_on_start` 决定。

“Stop / Restart”只操作本机拥有的角色，远程维护按钮不可用。客户端关闭不会停止、暂停或重启远程 Backend，也不会影响其他客户端进程。Backend 机器关闭自己启动的 Backend 会让其他角色断开；实时模式不会因此自动暂停真实执行，应先通过现有业务控制处理活动任务。

配置不热更新。修改共享 IP、端口或模式时，先处理活动任务并停止受影响会话，把相同修改复制到四机，再按 Backend 优先的顺序启动；改变 Backend 地址、端口或模式时四机都需重启对应会话。切回单机使用时选择 `All`，再使用原来的 DEMO/实时启动按钮。

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
| Map 等待或定位失败 | UAV GPS 新鲜度；没有真实连接时检查 Map 电脑的 Windows 定位设置 |
| 视频无画面但其他状态正常 | 实际 `video_url`、MediaMTX、视频信令/媒体端口；不要把 Video 角色 IP 当成流服务器 |
| 后端能力过旧或启动参数不支持 | 停止旧 EXE，更新同一版本并重新运行 `SetupDroneBackend.cmd`；UE 客户端同步更新模块 |

自动化测试覆盖共享配置验证、单角色启动与进程所有权、角色预检、端口覆盖和失败清理。实际证据项目及结果以 [validation.json](../Evidence/LANDeployment/20261117/validation.json) 为准；未完成的现场四机 HTTP/WS、视频、地图服务和真机飞行检查不能由这些组件测试替代。
