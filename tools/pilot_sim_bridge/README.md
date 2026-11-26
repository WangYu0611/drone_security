# 驾驶员 Mock 输入桥

输入桥把本机 UDP `stick` 转成现有 `/ws/pilot-control` 的驾驶员输入。Backend 保持对授权、会话、飞行动作和模拟位置的控制。UDP 仅监听 `127.0.0.1`，桥只连接本机 Backend 的 HTTP 和 WebSocket。输出只进入隔离 Mock 会话。

## 启动

需要 Node.js 24 或更新版本以及当前项目的 Backend。使用独立数据目录，给测试注册表配置 Mock 无人机，在同一目录放置包含 `"simulation": true` 的 `mock_execution.json`。注册表只使用 `127.0.0.1` 的测试条目，数据目录内不得有 `real_execution.json`。凭据文件在仓库之外，包含独立的 `Command` 和 `Pilot` 身份。凭据格式见 [驾驶员控制权 Mock](../../Backend/README.md#104-驾驶员控制权-mock)。

下面是隔离配置示例。端口可以替换为其他未占用端口。数据目录只放本次测试的注册表和 Mock 数据。

```yaml
server:
  http_port: 22782
  ws_port: 22783
  debug: false
port_map: {}
storage:
  path: ./Saved/PilotSim/drones.json
  video_metadata_path: ./Saved/PilotSim/metadata
log:
  file: ./Saved/PilotSim/backend.log
```

在启动 Backend 和输入桥的终端中都设置 `DRONE_PILOT_CONTROL_CREDENTIALS`，指向相应的凭据文件。Backend 使用上面的配置启动，`GET /` 返回 `pilot_sim_bridge` 能力后，在仓库根目录运行输入桥。

```powershell
$env:DRONE_PILOT_CONTROL_CREDENTIALS = 'C:\Private\pilot-sim-credentials.json'
node tools/pilot_sim_bridge/bridge.mjs --http-port 22782 --ws-port 22783 --listen-port 8899 --pilot-id mock-pilot --uav-id UAV-01 --activate
```

把 `mock-pilot` 改为凭据里的 Pilot `id`。`--activate` 显式启用本次会话。桥输出 `listening` 后，将测试输入发到显示的本机端口。若目标尚未分配，Command 在该 Pilot 上线后授权指定机号。授权和新的回中帧均到达后，Backend 启用会话，桥输出 `active`。回中帧的四轴均为 `0`，且 `action` 为 `none`。

## UDP 输入

每个数据报是最多 512 字节的 UTF-8 JSON。发送端在每次启动时生成新的 `session`，长度为 1–32 字节，`seq` 使用非负安全整数并逐帧递增。`t` 为 Unix 秒。下面是一帧示例，正常测试时持续发送新帧，动作只在按下的那一帧提交。

```json
{
  "type": "stick",
  "session": "mock-input-example",
  "seq": 1,
  "t": 1763000000.123,
  "heading_deg": 90,
  "forward": 0,
  "right": 0,
  "up": 0,
  "yaw": 0,
  "action": "none"
}
```

四轴范围均为 `[-1, 1]`。正向 `forward` 表示机头前方，正向 `right` 表示机身右侧。`up` 映射为 Backend 的 `vertical`，正值表示上升。可选 `heading_deg` 范围为 `[0, 360)`，仅作输入元数据，航向由 Backend 模拟状态决定。`yaw` 随帧传入，v1 Mock 的转向保持固定。可用动作包括 `none`、`takeoff`、`land`、`hold`、`gohome` 和 `estop`。

桥的转发频率上限为 20 Hz，保持启用时的 UDP 来源和输入会话，按帧转发新输入。停发时由 Backend 的 400 ms 新鲜度和 750 ms 会话期限停止推进并退出操控。撤销、断开或 Backend 重启后，重新运行输入桥并用当前授权和新的回中输入启用。

## 验证

在仓库根目录运行隔离协议验收。可用 `DRONE_BACKEND_EXE` 指向当前测试构建。脚本使用独立进程、端口、模拟数据和临时凭据，运行结果保存在 `Saved/PilotControlQA`。

```powershell
node Backend/tests/pilot_control_integration.mjs
```

该测试覆盖本机协议和 Mock 位移。当前工作站没有可运行的 PX4 SITL。PX4 软件在环、原生 Pilot 界面和真实飞机分别验收。
