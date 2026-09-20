# UE5DroneControl — 当前系统时序图

> 更新时间：2026-09-09
> 图示以当前 `UDroneNetworkManager`、`Backend/http/http_server.cpp` 和 JSON v1/YAML 协议为准。图中“发送成功”不等同于飞控执行成功；执行证据是回传的 `control_ack`。

## 1. 网络管理器初始化

```mermaid
sequenceDiagram
    participant GI as GameInstance
    participant NM as UDroneNetworkManager
    participant HTTP as UDroneHttpClient
    participant WS as UDroneWebSocketClient
    participant BE as DroneBackend
    participant REG as DroneRegistrySubsystem

    GI->>NM: Initialize()
    NM->>NM: 读取 DefaultGame.ini
    NM->>HTTP: BaseUrl=http://host:8080
    NM->>WS: ServerUrl=ws://host:8081/ws
    NM->>BE: Connect WebSocket
    NM->>HTTP: GET /api/drones（首个轮询约 0.5s 后）
    BE-->>HTTP: 顶层数组
    HTTP-->>NM: OnDroneListResponse
    NM->>REG: 同步身份/遥测/任务快照
    loop 每 PollIntervalSec=3s
        NM->>HTTP: GET /api/drones
    end
```

## 2. 单机目标点控制

```mermaid
sequenceDiagram
    participant User as 用户
    participant PC as DroneOpsPlayerController
    participant NM as UDroneNetworkManager
    participant WS as UDroneWebSocketClient
    participant BE as DroneBackend
    participant J as Jetson bridge
    participant PX4 as PX4

    User->>PC: 点击地图
    PC->>PC: 世界坐标 - 目标机 AnchorWorldLocation
    PC->>NM: SendMoveCommand(DroneId, UE相对cm, Mode, Speed)
    NM->>WS: {mode,drone_id,x,y,z,speed}
    WS->>BE: WebSocket
    BE->>BE: 校验并转为 NED m，入队
    BE-->>WS: command_ack（后端接受/入队）
    BE->>J: UDP JSON v1（可重发 5 次）
    J->>J: 去重、序号/会话校验、达到 3-of-5
    J->>PX4: 更新 TrajectorySetpoint
    J-->>BE: UDP YAML control_ack
```

## 3. 遥测和锚点回传

```mermaid
sequenceDiagram
    participant PX4 as PX4
    participant J as Jetson bridge
    participant BE as DroneBackend
    participant WS as UDroneWebSocketClient
    participant NM as UDroneNetworkManager
    participant REG as DroneRegistrySubsystem
    participant Actor as Mirror actor

    PX4->>J: local position / attitude / battery
    J->>BE: UDP YAML
    BE->>BE: 解析 local_position、GPS、ACK
    alt 首个有效 GPS 遥测
        BE->>BE: 保存 power_on anchor
        BE-->>WS: event power_on + gps_lat/lon/alt
    end
    BE-->>WS: telemetry（UE 相对 cm、姿态、电量、GPS）
    WS-->>NM: OnMessage
    NM->>REG: 更新 telemetry cache
    REG-->>Actor: OnTelemetryUpdated
    Actor->>Actor: 锚点 + 相对偏移，插值更新世界位置
```

## 4. 暂停、恢复和失联

```mermaid
sequenceDiagram
    participant UI as UE UI
    participant NM as NetworkManager
    participant BE as Backend
    participant J as Jetson

    UI->>NM: SendPauseCommand(DroneIds,true)
    NM->>BE: WS {type:pause,drone_ids}
    BE->>BE: 暂停队列，用最新实测位置建立 hold
    BE->>J: JSON hold
    J-->>BE: YAML control_ack（若链路可用）
    UI->>NM: SendPauseCommand(DroneIds,false)
    NM->>BE: WS {type:resume,drone_ids}
    BE->>BE: 恢复队列
    BE-->>NM: command_ack / error
    Note over BE: 超过 lost_timeout_sec=10s 未收到遥测时推送 lost_connection
```

## 5. 阵列任务、自动分配和集结

```mermaid
sequenceDiagram
    participant UI as SequenceDispatchPanelWidget
    participant NM as UDroneNetworkManager
    participant HTTP as UDroneHttpClient
    participant BE as DroneBackend
    participant WS as UDroneWebSocketClient
    participant Actors as Shadow/Mirror actors

    UI->>NM: SendArrayTaskFromData(paths, mode, auto_assign)
    NM->>HTTP: POST /api/arrays
    HTTP->>BE: {mode,auto_assign,paths[]}
    BE-->>HTTP: 201 {array_id,status:assembling}
    HTTP-->>UI: 提交成功
    alt auto_assign=true
        BE-->>WS: assignment_result(path_id -> drone_id)
        WS-->>NM: OnAssignmentResult
        NM-->>UI: 同步槽位/路径映射
    end
    loop 集结中
        BE-->>WS: assembling {ready_count,total_count}
        WS-->>Actors: 影子机跟随镜像机/更新进度
    end
    alt 全部到位
        BE-->>WS: assembly_complete
        WS-->>Actors: 放行在线真机的影子路径
        BE->>BE: StartTasks()
    else 超时
        BE-->>WS: assembly_timeout
        WS-->>UI: 显示失败并清理待执行路径
    end
```

## 6. 严格本地预演隔离

```mermaid
sequenceDiagram
    participant UI as MainMenu/Preview UI
    participant NM as NetworkManager
    participant HTTP as HttpClient
    participant WS as WebSocketClient

    UI->>NM: SetStrictLocalPreviewIsolation(true)
    NM->>NM: 停止轮询
    NM->>HTTP: CancelAllPendingRequests()
    NM->>WS: Disconnect()，关闭自动重连
    UI->>NM: SendMove / SendArray / Refresh
    NM-->>UI: 阻止请求并显示隔离提示
    UI->>NM: SetStrictLocalPreviewIsolation(false)
    NM->>WS: 恢复自动重连并 Connect()
    NM->>HTTP: 立即 GET /api/drones
```

## 7. 视频窗口链路

```mermaid
sequenceDiagram
    participant UE as UE DroneVideoWindowWidget
    participant BE as DroneBackend
    participant M as MediaMTX
    participant Browser as UE WebBrowser

    UE->>BE: GET /api/drones
    BE-->>UE: video_url（浏览器播放页）
    UE->>Browser: Navigate(video_url)
    Browser->>M: HTTP WebRTC page :8889/drone-N
    Browser->>M: WebRTC ICE（默认 UDP :8189）
    M-->>Browser: 视频帧
    UE->>Browser: 关闭窗口时 Navigate(about:blank)
```

## 8. 总体边界

```mermaid
flowchart LR
    UE[UE5: 交互/世界坐标/预演/UI]
    BE[DroneBackend: 事实/队列/任务/协议转换]
    J[Jetson: 3-of-5/Offboard setpoint]
    PX4[PX4: 飞控执行]
    M[MediaMTX: 视频]
    UE -->|HTTP/WS| BE
    BE -->|UDP JSON v1| J
    J -->|UDP YAML + control_ack| BE
    J --> PX4
    UE -->|video_url browser page| M
```
