# 真实无人机接口专项审计

2026-09-20。**找到了真实 UAV 的保留接入代码及历史网络接收记录；没有在本轮重连真实 UAV。** 用户确认早期真机联通过，与仓库中的历史记录相符；但未以此替代每个动作/视频链路的独立验收。

## 1. 接入点与当前状态

| 层 | 文件/类/接口 | 配置 | 当前状态 |
|---|---|---|---|
| PX4→Jetson | `Jetson/jetson_bridge.py::JetsonBridge`；rclpy、px4_msgs；VehicleOdometry/VehicleLocalPosition/VehicleGlobalPosition/VehicleStatus/BatteryStatus/可选 VehicleCommandAck | ROS_TOPIC_PREFIX、system ID 与部署 px4_msgs | LEGACY_PRESERVED；本轮未运行 |
| Jetson→Backend | `_send_telemetry` UDP YAML；`Backend/communication/udp_receiver.cpp::HandleReceive` | BACKEND_HOST=`<BACKEND_IP>`；TELEMETRY_PORT；slot | 历史有接收；本轮未验证 |
| Backend 状态 | `drone/drone_manager.cpp`、state_machine、gps_anchor_manager、heartbeat_manager | lost_timeout_sec/heartbeat_hz/port_map | 编入当前 Backend；Mock 主线不依赖现场数据 |
| Backend→Jetson | `udp_sender.cpp::Send` JSON v1；command_queue | port_map.<slot>.host/send_port；drones.json IP/port 可覆盖 | 保留；不是 MAVLink 直接 UDP 包 |
| Jetson→PX4 | OffboardControlMode、TrajectorySetpoint、VehicleCommand publishers | ROS2/DDS namespace、PX4 target_system | 保留，需设备端验证 |
| UE 网络入口 | `DroneNetworkManager`、`DroneHttpClient`、`DroneWebSocketClient`、Registry、RealTimeDroneReceiver | HTTP/WS endpoint、GeoidSeparationMeters | 当前三端复用网络层；真机域未重验 |
| 真视频 | `jetson_video_stream.py`、`tools/MediaMTX/mediamtx.yml`、`VideoShellWidget`/旧视频窗口 | RTSP/WebRTC URL、camera topic、video_url | 保留；当前 Saved 四个视频 URL 为空 |

结论：找到 PX4 和 MAVLink system ID/VehicleCommand 相关语义，但当前机载桥是 **ROS 2 px4_msgs 通道**。没有找到当前直接使用 MAVSDK/MAVROS/pymavlink 的主链，不能把名称含 MAVLink 等同于已经有通用 MAVLink 协议适配。Serial/TCP 等检索项不能仅因 socket 或注释出现就升级为飞控接入能力。

## 2. Telemetry 全字段映射

下表字段由 `_send_telemetry` 生成、UdpReceiver 接收。发送定时器配置 10 Hz；ROS 各 topic 的真实频率依设备，不由本轮证明。字段通常依 ROS 样本存在才发送，Backend 缺值用默认；GPS/local 有效性必须看标志，不能把默认零坐标当真数据。Source 为 `Backend/core/types.h::TelemetryData`、`udp_receiver.cpp`、`Jetson/jetson_bridge.py:786`。

| 字段 | Type / Unit | 来源 | 消费者/映射 | 接收频率/协议 | 状态 |
|---|---|---|---|---|---|
| timestamp | uint64 / μs | ROS clock nanoseconds/1000 | TelemetryData.timestamp；WS未统一透传此时间戳 | 10Hz目标/UDP YAML | 保留 |
| position[3] | double[3] / m | Odometry；有效 LocalPosition 时替换 | position_ned；N,E,D | 同上 | local_valid 优先 |
| q[4] | double[4] / 无量纲 | VehicleOdometry.q，w,x,y,z | QuaternionUtils→roll/pitch/yaw（degree） | 同上 | 保留；姿态 frame 要现场确认 |
| velocity[3] | double[3] / m/s | VehicleOdometry.velocity | 模长 speed；WS只输出 speed | 同上 | 保留 |
| angular_velocity[3] | double[3] / PX4角速度约定 | VehicleOdometry.angular_velocity | TelemetryData 接收；未见 WS 原值透传 | 同上 | 单位最终需匹配设备 px4_msgs |
| local_position[3] | double[3] / NED m | VehicleLocalPosition.x/y/z | 估计器有效且新鲜才取；匹配 TrajectorySetpoint frame | 同上 | 保留 |
| local_position_valid | bool | local sample validity/freshness | Backend/local控制前提 | 同上 | 保留 |
| local_velocity[3] | double[3] / m/s | VehicleLocalPosition.vx/vy/vz | Jetson发送，当前 UdpReceiver 不消费 | 同上 | ORPHANED 字段 |
| gps_lat/gps_lon | double / degree | VehicleGlobalPosition.lat/lon | Backend→WS/HTTP→Registry | 同上 | WGS84 语义，合法范围检查 |
| gps_alt | double / m AMSL约定 | VehicleGlobalPosition.alt | Backend→UE geoid补偿→Cesium | 同上 | 实机消息定义待核对 |
| gps_fix | bool | global/local 新鲜度、lat_lon_valid、alt_valid、时间差/finite | GPS anchor/地理派发守卫 | 同上 | 不代表RTK精度等级 |
| battery | int / %；-1未知 | BatteryStatus.remaining×100 | Backend低电告警、Registry | 同上 | 历史样本 bat=-1，不可宣称电量实测正常 |
| arming_state/nav_state | uint8 | VehicleStatus | 2→armed；14→offboard | 同上 | 遥测状态，不是操作 ACK |
| control_ack.session_id/command_id | string | Jetson已应用命令缓存 | 队列ACK/会话去重 | 随遥测 | 未证明所有历史命令均确认 |
| control_ack.sequence/mode/confirmed_packets | uint64/string/uint32 | 达确认阈值后的命令 | 防旧序/重复/确认统计 | 随遥测 | ACK证明 setpoint缓存应用，非到达目标 |

Heading 来自 q→yaw，不存在独立 GPS heading 传输键；pitch/roll 同理。Health 不是一个通用遥测对象，目前只能从 connection/battery/valid flags/arming state 推导局部健康状态。

脱敏、值示意的真实字段集：

```yaml
timestamp: 1000000
position: [1.0, 2.0, -3.0]
q: [1.0, 0.0, 0.0, 0.0]
velocity: [0.0, 0.0, 0.0]
angular_velocity: [0.0, 0.0, 0.0]
local_position: [1.0, 2.0, -3.0]
local_position_valid: true
gps_lat: 39.9
gps_lon: 116.3
gps_alt: 50.0
gps_fix: true
battery: 85
arming_state: 1
nav_state: 0
```

这是协议字段示例，不是本轮真实位置或电量采样。

## 3. 控制能力逐项结论

| 能力 | 代码证据 | 结论/边界 |
|---|---|---|
| Arm | `_offboard_loop` 在键盘触发和 warmup 后 VehicleCommand ARM_DISARM param1=1 | 存在；不等于 Stage1 Start 会解锁 |
| Disarm | bridge main 的 shutdown handler 发送 ARM_DISARM param1=0 | 存在于退出处理；不是 Command 独立按钮 |
| Offboard | VehicleCommand DO_SET_MODE 与 OffboardControlMode | 存在；50Hz持续发布 |
| Position Target | JSON move/hold→TrajectorySetpoint | 存在；NED m、power_on_origin |
| Velocity Target | `_speed_limited_velocity` 从位置误差生成速度限幅，TrajectorySetpoint.velocity | 存在速度限幅路径；不是任意外部 vx/vy/vz API |
| Hold / heartbeat | Backend mode=hold、5Hz；Jetson安全初始hold | 存在；需有效local frame |
| Waypoint Mission | `/api/arrays`→AssemblyController→ExecutionEngine→队列逐点 move | 存在旧航线执行；不同于 PX4 Mission upload 协议 |
| Pause / Resume | 旧 WS type pause/resume 和 DroneManager/ExecutionEngine | 存在；不同于 Mock execution action |
| Start Mission | 旧 arrays Start；Stage1 execution_start只模拟 | 没有证据证明 Stage1 Start 已接飞控 |
| Upload Mission 到飞控 | 未找到 MissionItem/mission-upload 飞控交换实现 | UNKNOWN/未找到；Backend 路线不等于飞控上传 |
| Takeoff / Land / RTL 专用命令 | 已检索桥与 Backend 控制源，未见对应公开动作处理 | 不宣称支持；Mock Return不等于RTL |
| Gimbal | 未找到明确云台控制通道 | UNKNOWN/未找到 |
| Camera | 取流/编码/元数据实现 | 不等于拍照、变焦或云台控制 API |

JSON v1 示例由 `UdpSender::Send` 结构脱敏生成，仅供文档阅读：

```json
{"protocol":"ue5_drone_control","version":1,"type":"control","session_id":"backend-demo","command_id":"backend-demo-d1-s1","sequence":1,"drone_id":1,"slot":1,"mode":"move","issued_at_unix_s":1.0,"sent_at_unix_s":1.1,"target":{"frame":"NED","reference":"power_on_origin","unit":"m","north":10.0,"east":20.0,"down":-5.0},"delivery":{"repeat_index":1,"repeat_total":5},"speed":2.0}
```

相同 command_id 有限重发；Jetson 确认窗口/计数与 session/sequence 防旧包。默认 move 5次、3包确认。speed 有限值进入限幅处理；旧 unlimited 标记序列化为字符串 `"NaN"`，不是合法 JSON 数字 NaN。不要套用旧 `ue_px4_msgs/packets.py` 的 `<dfffI` 24-byte 或 `<dfffIII` 32-byte 格式；当前 parse_control_packet 只接受 JSON v1。

## 4. 接回所需配置与映射

这里只给出定位和契约，**本轮未改配置、未启服务、未发送控制**。

| 项目 | 配置位置/字段 | 映射与检查 |
|---|---|---|
| Backend 服务 | Backend/config.yaml::server.http_port/ws_port | 常规8080/8081；不要混用Stage1 Launcher 19780/19781配置 |
| UAV地址 | port_map.<slot>.host；兼容 jetson.host；持久 drones.json ip/port | 文档用 `<REAL_UAV_IP>`；更新API只存描述符，运行目标重配需明确流程 |
| UDP遥测 | port_map.<slot>.recv_port；Jetson TELEMETRY_PORT | slot1 8888，后续+2；Jetson发到 `<BACKEND_IP>` |
| UDP控制 | port_map.<slot>.send_port；Jetson CONTROL_PORT | slot1 8889，后续+2；这是UDP，不与MediaMTX TCP播放端口等同 |
| ROS namespace | port_map ros_topic_prefix、Jetson ROS_TOPIC_PREFIX | 两侧配置必须和真实PX4 namespace一致；当前slot1可为空 |
| system ID | bridge MAVLINK_SYSTEM_ID（默认slot）与 VehicleCommand.target_system | 范围1..255；不能只凭UAV-xx显示名推断设备身份 |
| Telemetry Mapping | Jetson→TelemetryData→HttpServer→DroneNetworkManager→Registry | numeric drone_id、d1、slot→UAV-01；核实唯一对应，非字符串强转 |
| Coordinate Mapping | WGS84 AMSL、local NED、GeoidSeparationMeters、CesiumGeoreference | 见坐标专章；current local_position不可用Odometry任意frame代替 |
| Command Mapping | 旧WS mode/arrays→UE cm offset→NED m→JSON v1→ROS2 | 当前Plan/Execution尚未自动映射到这条链 |
| Video Mapping | Jetson --mediamtx-host/--rtsp-port/--webrtc-port/--stream-path/--image-topic | RTSP `rtsp://<MEDIA_HOST>:8554/drone-1`；浏览器 `http://<MEDIA_HOST>:8889/drone-1`；注册到video_url |
| Metadata | --backend-* CLI 与 Backend storage.video_metadata_path/max_batch | POST /api/video-metadata/batch；mission_id是录制会话，不自动等于Stage1 mission |

ROS topics 在 bridge create_publisher/create_subscription 块逐项可见：OffboardControlMode/TrajectorySetpoint/VehicleCommand 的 `fmu/in/*` 与 odometry/local/global/status/battery/command_ack 的 `fmu/out/*`，含版本后缀选择。实际名必须以当前脚本与目标 px4_msgs 为准，不保证另一固件版本兼容。

当前脚本精确后缀：`/fmu/in/offboard_control_mode`、`/fmu/in/trajectory_setpoint`、`/fmu/in/vehicle_command`、`/fmu/out/vehicle_odometry`、`/fmu/out/vehicle_status_v1`、`/fmu/out/vehicle_local_position_v1`、`/fmu/out/vehicle_global_position`、`/fmu/out/battery_status_v1`、`/fmu/out/vehicle_command_ack`（类型可导入时）。前方统一加 ROS_TOPIC_PREFIX。有效性配置还包括 `MAX_TELEMETRY_SAMPLE_AGE_SEC=1.0`、`MAX_GEOGRAPHIC_SAMPLE_SKEW_SEC=0.25`、`COMMAND_CONFIRM_COUNT=3`、`COMMAND_CONFIRM_WINDOW_SEC=2.5`。

HTTP UAV 输出示例来自 `ApiListDrones` 字段构造，以下为脱敏示意，不能代表当前设备在线：

```json
{"id":1,"id_str":"d1","name":"Demo UAV","model":"Example","slot":1,"slot_number":1,"ip":"<REAL_UAV_IP>","port":8889,"video_url":"http://<MEDIA_HOST>:8889/drone-1","status":"offline","battery":-1,"x":0,"y":0,"z":0,"yaw":0,"speed":0,"gps_lat":0,"gps_lon":0,"gps_alt":0,"gps_fix":false,"armed":false,"offboard":false,"task_mode":"move","task_state":"standby","task_array_id":"","task_current_wp":0,"task_waypoint_count":0,"task_detail":"","task_updated_at":0,"ue_receive_port":8888,"topic_prefix":"","bit_index":0,"mavlink_system_id":1}
```

## 5. 视频与元数据历史能力

`jetson_video_stream.py` 订阅 ROS 图像/camera_info，GStreamer 编码并用 rtspclientsink 推送；支持tcp/udp传输选择及本地分段录制。MediaMTX把 RTSP 发布流供 WebRTC 页面播放；UE UWebBrowser 加载页面。FFmpeg demo/工具脚本存在，但不是UE当前直接RTSP解码器。没有将 MediaPlayer/MediaSource 搜索名称直接写成当前播放主链。

历史 `Jetson/视频流调试对话整理.md` 明确说 YUYV→rgb8 相机链成功，但当时“下一目标”为 MediaMTX/浏览器/UE。因此该文档只支持相机取图局部成功，不能证明当时或现在相机→UE全链路完成。旧 V1/V2 的 MP4/native播放证据属于合成源。

元数据包含逐帧信息、相机信息和录制会话；Backend只强校验批次身份/序号/frames为object数组，不强制所有帧拥有GPS。SfM采集基础保留，不等于已经交付SfM重建算法或成果质量验收。

## 6. 历史证据与证据缺口

| 证据 | 能证明什么 | 不能证明什么 |
|---|---|---|
| 用户确认早期真实无人机联通 | 项目历史背景 | 每个控制动作/固件版本/本轮飞行通过 |
| `eb44db4` 的 `logs/backend.log` 2026-07-24记录 | 4368条UDP recv匹配、GPS anchor、Offline→Online→Lost→Online；225条move发送匹配 | 发出不等于收到；收到遥测不等于飞行任务完成；无法仅靠日志独立认证发送端硬件身份 |
| `Evidence/STAGE1-AUDIT/historical-telemetry.txt` | 脱敏原行号片段，可回到历史源 | 不是新采集 |
| `Jetson/视频流调试对话整理.md` | 历史对话确认相机彩色取图 | RTSP到UE、SfM完成 |
| `eb44db4` field test updates、`dff08c0` 高度修正 | 接口演进/现场问题修订线索 | 提交标题本身不是测试PASS |
| `ue_px4_msgs` 与旧配置 | 曾有二进制控制/ENU辅助模型 | 与当前JSON桥自动兼容 |
| 历史删除 UI `8941c55^:...LocalPreviewIsolationToggleWidget.h` | HISTORICAL_ONLY 的旧隔离开关实现 | 不能据此说整个隔离机制已删除；NetworkManager仍有隔离代码 |

现有历史片段未找到足以单项闭环证明 Arm、Takeoff、Land、RTL、完整任务执行、真实相机到UE、每条control_ack的全部证据。结果为 **LEGACY_PRESERVED + HISTORICAL_EVIDENCE_ONLY / UNVERIFIED**，不是把未知判作失败，也不是把历史变成本轮PASS。

## 7. 接回前需要恢复的技术资料

需要补齐实际固件/px4_msgs版本、各slot/IP/端口/system ID对应、GPS高度基准/geoid、local frame/origin策略、命令确认与真实到达判据、媒体源/编码/转流配置，以及新版业务Execution与旧控制的适配设计。现有代码可复用，但“只换IP即可恢复当前Stage1真机任务闭环”没有证据支持。
