# 驾驶员 Mock 输入桥

输入桥接收本机 UDP `stick`，通过认证的 `/ws/pilot-control` 进入隔离 Mock 会话。Backend 负责授权、会话和模拟位移。

## 一条命令验证

使用 Node.js 24 或更新版本。在仓库根目录运行，脚本会启动独立 Backend 和输入桥，生成测试数据及临时凭据，退出时清理自有进程。默认构建位置为 `Backend/build/Release/DroneBackend.exe`。使用其他当前构建时，将 `DRONE_BACKEND_EXE` 指向其绝对路径。

```powershell
node Backend/tests/pilot_control_integration.mjs
```

查看 `Saved/PilotControlQA/protocol-*/result.json` 和同目录的 `process.log`。该验收覆盖本机 UDP → 输入桥 → 认证会话 → Mock 位移。PX4 SITL 和原生界面各自有独立的验收项。 [Mock 桥接验证](../../Evidence/PilotSimBridge/2026-11-26/validation.md)

## 手动启动

### 1. 启动隔离 Backend

在 Backend、输入桥和 Command 的终端中，将 `DRONE_PILOT_CONTROL_CREDENTIALS` 指向同一份仓库外的测试凭据文件。下面示例使用 `mock-command` 和 `mock-pilot`，请按文件中的对应身份填写。 [Command 和 Pilot 凭据格式](../../Backend/README.md#104-驾驶员控制权-mock)

在仓库根目录执行。每次使用新数据目录，保留 `port_map: {}` 和 `server.debug: false`。输入桥和测试客户端使用 `127.0.0.1`。如需将 Backend 的通用 HTTP/WS 监听也限制在回环，使用现有 [Mock Backend 回环设置](../mock_mcp/README.md)。

```powershell
$env:DRONE_PILOT_CONTROL_CREDENTIALS = 'C:\Private\pilot-sim-credentials.json'
$runDir = "Saved/PilotSim/$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
Copy-Item Backend/mock_execution.json "$runDir/mock_execution.json"
'[{"id":"d1","slot":1,"name":"Mock UAV-01","model":"Mock","ip":"127.0.0.1","port":0,"video_url":""}]' |
    Set-Content -Encoding ascii "$runDir/drones.json"
@{
    server = @{ http_port = 22782; ws_port = 22783; debug = $false }
    port_map = @{}
    storage = @{ path = "$runDir/drones.json"; video_metadata_path = "$runDir/metadata" }
    log = @{ level = 'info'; file = "$runDir/backend.log" }
} | ConvertTo-Json -Depth 4 | Set-Content -Encoding ascii "$runDir/backend.json"
$backendExe = if ($env:DRONE_BACKEND_EXE) { $env:DRONE_BACKEND_EXE } else { './Backend/build/Release/DroneBackend.exe' }
& $backendExe "$runDir/backend.json"
```

在另一终端执行 `(Invoke-RestMethod 'http://127.0.0.1:22782/').capabilities`，确认包含 `pilot_sim_bridge` 和 `pilot_control_actions`。

### 2. 启动输入桥并分配

在仓库根目录执行。`--activate` 为本次会话显式启用，`--listen-port 0` 可分配空闲 UDP 端口。以 `listening` 事件中的端口为准。

```powershell
$env:DRONE_PILOT_CONTROL_CREDENTIALS = 'C:\Private\pilot-sim-credentials.json'
node tools/pilot_sim_bridge/bridge.mjs --http-port 22782 --ws-port 22783 --listen-port 8899 --pilot-id mock-pilot --uav-id UAV-01 --activate
```

使用 Command 界面时，在现有 UE 启动命令上指定 `-ClientRole=Command -PilotControlIdentity=mock-command -P1Http=http://127.0.0.1:22782 -P1Ws=ws://127.0.0.1:22783/ws`。在机队列表勾选 `UAV-01`，打开「驾驶员控制权」，选中在线的 `mock-pilot`，点击「分配控制权」。只做协议联调时，可在第三个终端用同一份测试凭据执行相同分配。

```powershell
$env:DRONE_PILOT_CONTROL_CREDENTIALS = 'C:\Private\pilot-sim-credentials.json'
$operator = (Get-Content -Raw -LiteralPath $env:DRONE_PILOT_CONTROL_CREDENTIALS | ConvertFrom-Json).credentials |
    Where-Object { $_.role -eq 'Command' -and $_.id -eq 'mock-command' }
if (-not $operator) { throw '缺少 mock-command 测试凭据' }
$headers = @{ Authorization = "Bearer $($operator.token)" }
$uri = 'http://127.0.0.1:22782/api/pilot-control'
$snapshot = Invoke-RestMethod $uri -Headers $headers
$aircraft = $snapshot.aircraft | Where-Object uav_id -eq 'UAV-01'
if (-not $aircraft -or -not ($snapshot.pilots | Where-Object { $_.pilot_id -eq 'mock-pilot' -and $_.online })) {
    throw '等待 UAV-01 可用且 mock-pilot 在线'
}
$body = @{
    action = 'grant'; request_id = [guid]::NewGuid().ToString(); pilot_id = 'mock-pilot'
    uav_ids = @('UAV-01'); expected_revisions = @{ 'UAV-01' = $aircraft.revision }
} | ConvertTo-Json -Depth 3
(Invoke-RestMethod $uri -Method Post -Headers $headers -ContentType 'application/json' -Body $body).results
```

向输出端口持续发送约 20 Hz 的回中帧，四轴为 `0`，`action` 为 `none`。看到 `active` 后再发动作。可用下面的本机回中探针验证启用和超时。端口取 `listening` 事件，探针退出后桥将随会话超时退出。

```powershell
$udpPort = 8899
$udp = [Net.Sockets.UdpClient]::new()
$session = [guid]::NewGuid().ToString('N')
try {
    foreach ($seq in 1..60) {
        $frame = @{
            type = 'stick'; session = $session; seq = $seq
            t = ([DateTimeOffset]::UtcNow).ToUnixTimeMilliseconds() / 1000
            forward = 0; right = 0; up = 0; yaw = 0; action = 'none'
        } | ConvertTo-Json -Compress
        $bytes = [Text.Encoding]::UTF8.GetBytes($frame)
        [void]$udp.Send($bytes, $bytes.Length, '127.0.0.1', $udpPort)
        Start-Sleep -Milliseconds 50
    }
} finally {
    $udp.Dispose()
}
```

### 3. 停止、恢复和排查

Backend 收到最后一帧有效输入后，输入超过 400 ms 或输入窗口过期即停止推进。750 ms 无有效输入后会话退出。正常结束时，可在桥接终端按 `Ctrl+C`，再停止本次启动的 Backend。重试时重新启动输入桥和发送端，以新的 `session` 回中启用。要同时清除模拟位置和已保存的授权，重新运行第 1 步，生成新的数据目录。

| 桥接事件 | 处理 |
| --- | --- |
| `INVALID_ARGUMENTS` / `INVALID_PILOT_CREDENTIALS` | 核对端口、机号、`--activate` 和测试凭据文件中的唯一 Pilot 身份。 |
| `MOCK_BRIDGE_START_FAILED` | 核对 HTTP 端口、当前构建和两个能力标记。确认 `mock_execution.json` 含 `"simulation": true`，同目录无 `real_execution.json`，`debug` 为 `false` 且 `port_map` 为空。 |
| `UDP_ERROR` | 检查 UDP 端口是否被占用。换空闲端口或设 `--listen-port 0`，同步更新发送端。 |
| 只有 `listening` | 等 Pilot 在线后核对分配的机号和身份。持续发回中帧，只保留一个发送端。 |
| `WEBSOCKET_ERROR` / `SUBSCRIPTION_TIMEOUT` / `ACTIVATION_REJECTED` | 检查 WebSocket 端口与 HTTP 是否属于同一个 Backend、Pilot 授权是否有效、目标是否空闲，再重启输入桥。 |
| `INPUT_TIMEOUT` | 停发后的正常结果。若非预期，检查是否仍在发送新鲜帧，且源端口、`session` 和递增 `seq` 都有效。 |
| `GRANT_CHANGED` / `SESSION_ENDED` / `CONTROL_WINDOW_LOST` | 在 Command 复核当前归属和 Backend 状态，恢复授权后重新启动输入桥。 |

## UDP 帧约定

发送 UTF-8 JSON，每个数据报最多 512 字节。`type` 为 `stick`。每次启动生成 1–32 字节的 `session`，使用非负安全整数 `seq` 并逐帧递增。`t` 为 Unix 秒。输入桥以第一帧有效报文锁定发送源端口和 `session`，每秒最多转发 20 个新帧。

四轴范围均为 `[-1, 1]`。`forward` 和 `right` 相对于机身，`up` 正值上升。可选 `heading_deg` 范围为 `[0, 360)`，用作输入元数据。Backend 使用模拟航向处理水平位移，v1 的 `yaw` 输入不改变航向。`action` 为 `none`、`takeoff`、`land`、`hold`、`gohome` 或 `estop`。动作只在按下帧提交一次。
