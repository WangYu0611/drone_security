# Stage 1 数据接口目录

2026-09-20。依据 `Backend/http/http_server.cpp::HandleHttp/HandleWsCommand`、storage、Jetson 和 UE 消费者逐项扫描。**33 个 HTTP method/path 组合**（不含通配 OPTIONS）；**19 个服务端 WS type、4 个客户端合法 type，共 23 个不同 type**，另有 mode 命令分支。不是 23 个 Stage 1 活跃事件，不能将业务 event_type、HTTP action 再算作 WS 接口。

Backend 持久业务/配置族 7 个；UE 另有 2 个本地 legacy 数据族。主要坐标转换接口 12 个，视频链路 5 组，真实 UAV 接口 5 组；这是明确列项的统计口径，不是物理连接或当前在线设备数。

完整扫描：[http-routes.json](../../Evidence/STAGE1-AUDIT/http-routes.json)、[带原行号 HTTP 源码](../../Evidence/STAGE1-AUDIT/http-source.txt)、[字段附录](Stage1-Schema-Fields.md)、[Registry 完整声明](../../Evidence/STAGE1-AUDIT/registry-structs.txt)。本轮只读取本地文件，没有向以下端点发请求。

## 1. HTTP 全量表

公共约定：Stage 1 Launcher 地址 `http://127.0.0.1:19780`；直接运行 Backend 默认 8080。无请求正文写“—”；ID 占位 `{id}` 旧 Drone 接受 `1`/`d1`，不是 `UAV-01`。Source 表中的函数全部位于 `Backend/http/http_server.cpp`，逐函数完整实现见带行号附录。

验证：S=有 Stage 1 保存的自动化/协议/原生证据；L=保留源码与旧测试参考，本轮未复测；D=debug/测试用途。S 也不是本轮重验。所有 HTTP 响应均由 Backend 产生，表中 Producer 是请求发起方，Consumer 是响应使用方。

| Method / Path | Purpose | Request | Response | Producer → Consumer | Persistence | 当前用途/Verification | Source |
|---|---|---|---|---|---|---|---|
| GET `/` | 服务基本状态 | — | status,http_port,ws_port,debug | 工具/Launcher→工具 | 无 | 可用，不能独立证明三端 READY；L | HandleHttp |
| GET `/api/context` | 上下文快照 | — | OperationalContext | 三端/Launcher→三端 | 读 context | 当前 S | HandleHttp |
| PATCH `/api/context` | 更新共享选择 | instance_id,patch | OperationalContext | 已订阅客户端→三端 | 是 | 当前 S；角色规则/ID 检查 | UpdateContext |
| GET `/api/context/clients` | 客户端健康列表 | — | ClientPresence[] | 三端/Launcher→三端 | 否，内存 | 当前 S | ContextClients |
| POST `/api/context/clients` | 注册身份 | client_id,client_role,instance_id,hostname,app_version | ClientPresence，初始 OFFLINE | 三端→注册者 | 否 | 当前 S；注册不等于订阅成功 | RegisterContextClient |
| POST `/api/context/debug/alert` | 注入告警 | drone_id:int | alert_id,type,drone_id,alert_type,value | QA→Command | 否 | D；debug 开关 | HandleHttp |
| GET `/api/security-plans` | 全业务快照 | — | version/plans/missions/paths/deployments/executions/events 等 | 三端/Launcher→三端 | 读 security_plans | 当前 S | HandleHttp |
| POST `/api/security-plans` | Plan/Mission/Route/Deploy/Execution | instance_id,action，加下表 action 字段 | state + plan_id/mission_id/execution_id/context 等，依 action | Command/Map→三端 | 通常是；select 独立 context | 当前 S；Video 写禁止 | PlanRequest / SecurityPlanStore::transact |
| GET `/api/ui-preferences` | 语言 | — | language,ui_preferences_version,updated_at,updated_by | 三端→三端 | 读 preferences | 当前 S | SharedViewStore::snapshot |
| PATCH `/api/ui-preferences` | 设置语言 | instance_id,language:en/zh-Hans | 同 GET | 显式语言按钮→三端 | 是 | 当前 S | ViewRequest |
| GET `/api/video-view` | 独立视频目标 | — | video_target_uav_id,video_view_version,updated_at,updated_by | 三端→三端 | 读 video_view | 当前 S | SharedViewStore::snapshot |
| PATCH `/api/video-view` | 设置视频目标 | instance_id,video_target_uav_id:string/null | 同 GET | View Video/feed→Video/其他状态展示 | 是 | 当前 S | ViewRequest |
| POST `/api/mock-executions/pause-all` | 统一关机前持久暂停 | simulation:true,request_id | state,execution_id（全暂停可为空） | Launcher→Launcher | 是 | 当前 S；不是飞控 pause-all | HandleHttp / execution_shutdown |
| GET `/api/drones` | Registry 初始/轮询 | — | Backend UAV[]，见模型 | 三端网络层→Registry/UI | 身份读盘；遥测内存 | 当前 S；Mock 执行另域 | ApiListDrones |
| POST `/api/drones` | 注册 UAV | name/model/slot 或 slot_number/ip/port/video_url；slot 必须在 port_map | id:int,id_str,slot,name | 旧注册 UI/工具→Registry | drones.json | L；Launcher seed 的空 port_map 不支持新增 slot | ApiRegisterDrone |
| PUT `/api/drones/{id}` | 更新描述符 | 可选 name,model,ip,port,video_url | id,id_str,updated,name | 旧 UI/视频注册脚本→Registry | 是 | L；运行目标更新缺口见风险 | ApiUpdateDrone |
| DELETE `/api/drones/{id}` | 删除注册 | — | id,id_str,deleted:true | 旧 UI/工具→Registry | 是 | L；不是删除 Plan | ApiDeleteDrone |
| GET `/api/drones/{id}/anchor` | GPS 锚点 | — | drone_id,drone_id_str,valid,gps_lat,gps_lon,gps_alt；无锚点425 | 旧网络/诊断→UE | 无 | L | ApiGetAnchor |
| POST `/api/drones/refresh` | 重探测失联 UAV | — | ok,message,refreshed_drone_ids | 旧 UI→状态更新 | 无 | L；会发 hold probe，非纯读 | ApiRefreshDrones |
| POST `/api/video-metadata/batch` | 元数据批次存档 | mission_id,drone_id,batch_sequence,frames；可选 final/schema_version/相机信息 | ok,mission_id,drone_id,batch_sequence,accepted_frames,duplicate,completed,relative_batch_path | Jetson→采集端 | JSONL/session 文件 | L；当前 Mock 不调用 | ApiStoreVideoMetadataBatch |
| POST `/api/arrays` | 旧路径集结/派发 | array_id,mode,auto_assign,paths[{drone_id,waypoints}] | array_id,status:assembling | Sequence UI/旧客户端→Backend/UE | 执行态内存 | L；可能下发真机命令 | ApiCreateArray |
| POST `/api/arrays/{id}/stop` | 停旧集结/执行 | — | array_id,status | 旧 UI→UE | 无 | L；不同于 execution_abort | ApiStopArray |
| GET `/api/debug/drone/{id}/state` | 诊断状态 | — | 位置/电量/队列/heartbeat/anchor/ACK 字段 | 诊断→工具 | 无 | D | DebugDroneState |
| GET `/api/debug/drone/{id}/queue` | 队列 | — | id,id_str,queue_size,paused,commands[{sequence,timestamp,slot,x,y,z,mode,coordinate_frame,reference,unit}] | 诊断→工具 | 无 | D；队列坐标 NED m | DebugDroneQueue |
| GET `/api/debug/heartbeat/{id}` | 心跳统计 | — | id,id_str,running,last_sent_time,sent_count,send_failed_count,last_ned_x/y/z,active_sequence,active_mode,repeat_index/total | 诊断→工具 | 无 | D | DebugHeartbeat |
| POST `/api/debug/drone/{id}/inject` | 合成遥测 | position[3],q[4],velocity[3],battery,gps_lat/lon/alt/fix,arming_state,nav_state | injected | QA→Backend/UE | 无 | D；不能当真机证据 | DebugInjectTelemetry |
| POST `/api/debug/cmd/{id}/move` | 移动请求 | x,y,z（UE-format cm）,speed（m/s） | moved,x,y,z | 诊断→控制链 | 无 | D；有控制副作用 | DebugMove |
| POST `/api/debug/cmd/{id}/pause` | 暂停旧队列 | — | id,paused:true | 诊断→控制链 | 无 | D | DebugPause |
| POST `/api/debug/cmd/{id}/resume` | 恢复旧队列 | — | id,paused:false | 诊断→控制链 | 无 | D | DebugPause |
| POST `/api/debug/cmd/batch/array` | 批量旧路线 | 数组，每项 drone_id,pathId/path_id,bClosedLoop/closed_loop,waypoints；实现固定scout/debug_batch | array_id,status,path_count | QA→控制链 | 无 | D | DebugBatchArray |
| POST `/api/debug/cmd/{id}/array` | 兼容数组转发 | 同 ApiCreateArray 对象 | 同 ApiCreateArray | QA→控制链 | 无 | D；id 参数不自动注入 drone_id | DebugSingleArray |
| POST `/api/debug/cmd/{id}/target` | 向旧执行引擎注入目标 | x,y,z | target_injected,x,y,z | QA→ExecutionEngine::InjectTarget | 无 | D；不是DebugMove别名 | DebugTarget |
| GET `/api/debug/arrays/{id}/state` | 当前集结统计 | — | array_id,ready_count,total_count,state:int | QA→工具 | 无 | D；实现读全局当前集结，不按 id 历史查询 | DebugArrayState |

无独立 `/api/health`、`/api/telemetry`、`/api/alerts`、`/api/events`、`/api/missions`、`/api/deployments`、`/api/executions` CRUD 路由。Mission/Route/Deployment/Execution 通过 security-plans action 与快照；遥测/告警走 WS，历史业务 Event 在 security_plans.events。OPTIONS 通配返回 204。普通错误 `{detail}`；Plan 冲突可返回 409 `{code,params,message,state}`；偏好校验返回 400 `{code,params,message}`。不能假设所有响应统一 envelope。

## 2. Security Plan action 契约

公共请求必需：instance_id 对应已在线订阅的 Command/Map、action。客户端通常附 expected_version；后端对提供的值检查，deploy 强制要求。执行分支先分派到 executionRequest，依赖 request_id/control_version 而非统一 Plan version。

| Actions | 额外字段/必填约束 | Producer | Persistence / 返回 |
|---|---|---|---|
| create_plan | name 非空≤160 bytes；description 可选；default_mission_name 可选 | Command | state,plan_id,mission_id,context |
| update_plan | plan_id,name；description | Command | 内容修订增加 |
| open_plan / select | plan_id；mission_id 可选但必须属于方案 | Command；select 也可 Map | open 有事件；select 更新 Context |
| copy_plan | plan_id；name 可选；非 deployed 需 allow_draft:true | Command | 新 plan/mission/route IDs；context |
| delete_plan | plan_id；只允许 DRAFT 且无 edit_session | Command | 删除关联 mission/path，事件保留 |
| add_mission / rename_mission / delete_mission | plan_id；rename/delete 要 mission_id；add/rename 要 name | Command | 业务集合与 revision |
| assign | plan_id,mission_id,assigned_uav_id；空串解除 | Command | 同方案 UAV 不可重复分配 |
| set_workflow_step | plan_id,workflow_step:BasicInfo/TaskConfig | Command | 工作流状态，不改内容 revision |
| request_map_route_edit | plan_id,mission_id；需在线 Map | Command | Context + WS MapRouteEditRequested；非直接存本地 draft |
| begin_route_edit | plan_id,mission_id,content_revision；workflow 可选 | Map | edit_session_id、owner、20 秒 lease |
| mark_route_dirty / discard_route_edit / finish_route_edit | plan_id,mission_id,edit_session_id 且 owner 一致 | Map | Finish 要非 DIRTY 且保存≥2 点；移除 session |
| save_route | plan_id,mission_id,edit_session_id,path；keep_editing 可选 | Map | revision +1；keep_editing 保持 EDITING |
| validate | plan_id | Command/Map | validation ready/issues/counters；READY/DRAFT |
| review / deploy | plan_id；必须当前有效 validation；deploy 还需 review 与 expected_version | Command | deploy 返回冻结部署；不会执行 |
| execution_start | plan_id,mission_id,request_id；在线 Map/Mock/UAV 无冲突 | Command | execution_id/state；同 request_id 幂等 |
| execution_pause/resume/return/abort | execution_id,control_version,request_id | Command | 状态约束；同 request_id 不可换内容 |
| execution_shutdown | request_id | Command；Launcher pause-all 封装 | 所有活跃执行持久化 PAUSED，记 resume_state |

Route 必须有 waypoints 数组，≤1000 点。每点必需 finite 数字 latitude/longitude/altitude/segmentSpeed/waitTime，sequence 从 1 连续；经纬度范围 ±90/±180，speed/wait 非负。location/pathId/bClosedLoop 由 UE 保存，Backend checkPath 不强制这些兼容字段。空数组可保存为清空路线，Finish/Deploy/Start 至少 2 点。Mock 不根据 bClosedLoop 无限循环。

## 3. WebSocket 全部类型

URL：Launcher `ws://127.0.0.1:19781/ws`；普通 Backend 8081。源码服务分端口接受 upgrade，不能把 `/ws` 当作一套额外 HTTP REST 资源。JSON 没有统一 envelope；旧消息多数字段在顶层，新域用 payload。没有所有消息通用的 sequence/version/timestamp。

| type（方向） | Producer → Consumer / 触发频率 | envelope / version / payload | 状态与 Source |
|---|---|---|---|
| telemetry（下行） | DroneManager→UE Network→Registry；随遥测，Jetson 配置10 Hz | 顶层 drone_id/drone_id_str,x/y/z,yaw/pitch/roll,speed,battery,armed/offboard,gps_*,local_position_valid,anchor_*；不带统一 seq/ts | Legacy；HttpServer telemetry callback |
| event（下行） | 状态机→UE；状态变化 | event、drone IDs、gps 锚点字段 | Legacy power_on/reconnect/lost_connection；callback |
| alert（下行） | 低电/失联等→CommandAlertStore；事件触发 | alert_id,drone_id,alert/alert_type,value | callback / debug 路由；非持久 alert service |
| assembling（下行） | AssemblyController→UE；集结进度 | array_id,ready_count,total_count | Legacy |
| assembly_complete（下行） | AssemblyController→旧预演/状态消费者 | array_id | Legacy；不是 Mock mission completed |
| assembly_timeout（下行） | AssemblyController→UE | array_id,ready_count,total_count | Legacy |
| assignment_result（下行） | auto assign→UE | array_id,assignments | Legacy |
| drone_task_state（下行） | 旧执行/DroneManager→UE Registry | drone_id/drone_id_str,array_id,mode,state,current_wp,waypoint_count,detail,updated_at | Legacy task，不是 P5.2 Execution |
| command_ack（下行） | WS 命令处理→请求者 | command/drone_id 等按操作分支 | 只证明 Backend 接受；不是 PX4 ACK |
| error（下行） | Backend→请求者 | code,message；过期心跳例外只有 message | 通用，须容错 |
| OperationalContextChanged（下行） | context 更新→三端 | type,event_type,event_id,version,timestamp,source_client,changes,payload | 当前；context_version 单调 |
| client_connected（下行） | 订阅/heartbeat→三端/Launcher | payload ClientPresence；首次有 event_id/timestamp，心跳简化 | 当前；client_version |
| client_disconnected（下行） | socket 断开/过期→三端 | payload ClientPresence；事件元数据 | 当前；client_version |
| context_subscribed（下行） | 订阅确认→订阅者 | payload context + clients[] | 当前；快照版本 |
| context_pong（下行） | 心跳应答→订阅者 | 仅 type | 当前 |
| SecurityPlansChanged（下行） | 事务、lease、Mock tick→三端 | payload 全业务文档；version + execution_version 分域 | 当前；不是单独 EXECUTION_UPDATE |
| UIPreferencesChanged（下行） | shared preference→三端 | payload language/ui_preferences_version/updated_* | 当前 |
| VideoViewChanged（下行） | 显式 video target→三端 | payload video_target_uav_id/video_view_version/updated_* | 当前 |
| MapRouteEditRequested（下行） | Command 请求经 Backend→Map | payload request_id,plan_id,mission_id,requested_by,timestamp | 当前；不是编辑完成事件 |
| subscribe_context（上行） | 三端→Backend，连接一次 | type,instance_id；先 HTTP 注册 | 当前 |
| context_ping（上行） | 三端→Backend，约3秒 | type,hydrated:boolean | 当前；12秒超时；lease 刷新 |
| pause / resume（上行，各一类） | 旧客户端→Backend，按操作 | type,drone_id 或 drone_ids[]；可选request_id以获取ACK | Legacy；不是 execution_pause/resume |

另一个**无 type 必需**的旧 WS 请求使用 `mode:move/scout/patrol/attack`，字段 drone_id,x,y,z,speed 及可选 coalesce_queued_vertical_targets/coalesce_queued_manual_targets；走 ProcessMoveCommand，坐标为 UE-format cm。`type:move` 明确返回错误。这些旧 mode 不证明 AI 识别/攻击系统已实装。

业务事件名不是 WS 消息类型。完整源码扫描与保存样本见附录；主要为 PLAN_CREATED/OPENED/COPIED/VERSION_CREATED/DELETED/UPDATED/VALIDATED/REVIEW_READY/REVIEWED/DEPLOYED、MISSION_CREATED/UPDATED/DELETED/CONFIGURED、UAV_ASSIGNED/UNASSIGNED、ROUTE_EDIT_STARTED/COMPLETED、ROUTE_CREATED/UPDATED/CLEARED/SAVED、DEPLOYMENT_CREATED，以及 EXECUTION_CREATED、PREFLIGHT_STARTED/PASSED、MISSION_STARTING/STARTED/PAUSED/RESUMED/RETURNING/ABORTED/COMPLETED/FAILED、WAYPOINT_REACHED。以 `security_plan_store.h::event`、`mock_execution.inl::executionEvent` 的实际分支为准。

## 4. 核心 Schema 与 Required 解释

Plan/Mission/Route/Deployment/Execution/Event 的逐字段并集及样本类型见 [字段附录](Stage1-Schema-Fields.md)。Source 所有字段可直接定位到 constructor/transact/serialization。附录的 UNKNOWN Required 不能解释为全部字段可省略：请求必填见第2节，下面给出输出构造/条件字段。

| 对象 | 输出恒有字段/主要约束 | 条件字段 | Persistent / Version |
|---|---|---|---|
| SecurityPlan | id,name,description,status,content_revision,review,version,created_at,updated_at,mission_ids；id由 Backend 生成 | validation、workflow_step/workflow_mission_id、deployment_id、source_plan_id/source_deployment_id、复制兼容 deployment:null | 是；doc version + plan version/content_revision |
| Mission | id,plan_id,name,assigned_uav_id:null/string,route_id:null/string,status | 依关系解析 Execution，Mission 不嵌通用 Flight 对象 | 是；doc version / plan revision |
| Route/Waypoint | route_id/revision 由 Backend 补；waypoints 规则见第2节 | pathId,bClosedLoop,location{x,y,z} 兼容 UE | 是；route revision |
| Deployment | id,plan_id,content_revision,deployed_at/deployed_by,reviewed_at/reviewed_by,snapshot{plan,missions,paths} | 不含“已开始飞行”推断 | 是；冻结快照 |
| Execution | 字段附录全部创建字段；started_at/completed_at 起初 null；simulation=true | resume_state 仅统一暂停时 | 是；execution_version + control_version |
| Event | sequence,timestamp,event_type,params,category,target_id,message,source | execution_snapshot{state,position,progress,current_waypoint} 仅执行事件 | 是；doc内列表；非永久容量策略承诺 |

### Backend UAV 与 UE Registry

Backend persistent DroneRecord 的字段是 id:string（d1）、name:string、model:string、slot:int、ip:string、port:int、video_url:string；全部 SaveDrones 序列化，LoadDrones 有默认值，不含 telemetry/Home/Health。注册请求的 slot 必须匹配 port_map；身份主键、slot、UAV-xx 不能无条件混用。

GET /api/drones 每项输出全量字段（类型/单位）：

| Field | Type / Unit | Required / Meaning | Source | Persistent / Versioned |
|---|---|---|---|---|
| id,id_str,name,model,slot,slot_number,ip,port,video_url | int,string,string,string,int,int,string,int,string | 输出恒有；slot_number=slot | ApiListDrones / DroneRecord | 描述符持久；无对象 version |
| status,battery | string,int %（-1未知） | 输出恒有；offline/connecting/online/lost 与电量 | DroneManager | 内存；无版本 |
| x,y,z | number cm | 输出恒有；NED 转换的 offset，非任意 Cesium world | CoordinateConverter / GetStatus | 内存 |
| yaw,speed | number degree,number m/s | 输出恒有；由 quaternion/velocity 求得 | QuaternionUtils | 内存 |
| gps_lat,gps_lon,gps_alt,gps_fix | number degree,degree,m AMSL,bool | 输出恒有，valid 决定可信度 | TelemetryData | 内存 |
| armed,offboard | bool | 输出恒有；arming_state==2/nav_state==14 | TelemetryData | 内存 |
| task_mode,task_state,task_array_id,task_detail | string | 输出恒有；旧任务状态 | task_states_ | 内存 |
| task_current_wp,task_waypoint_count,task_updated_at | int,int,number Unix s | 输出恒有；旧 task 非 Execution | PublishTaskState | 内存 |
| ue_receive_port,topic_prefix,bit_index,mavlink_system_id | int,string,int,int | 输出恒有；端口/ROS/topic/slot派生 | port_map / rec.slot | 配置派生，无版本 |

WS telemetry 另外给 pitch/roll、local_position_valid、anchor_valid/anchor_gps_*。TelemetryData 接收 q/velocity/angular_velocity，但 HTTP 列表没有原始 q/angular_velocity 字段；local_velocity 由 Jetson 发出，UdpReceiver 未映射该键，不能宣称完整透传。

UE FDroneDescriptor/FDroneTelemetrySnapshot 完整声明见 Registry 附录。前者另有 ThemeColor、bIsEnemyTarget、EnemyInitialLocation 等本地字段；后者有 WorldLocation、NedLocation、GeographicLocation（注释 Lat,Lon,Alt）、Velocity、Attitude、Altitude、Battery/BatteryPercent、bArmed/bOffboard/bGpsFix/bLocalPositionValid、LastUpdateTime、GpsLatitude/Longitude/Altitude、TaskMode/State/ErrorDetail、CurrentWaypointIndex/TotalWaypoints、LocalState。这些是 C++ 输出成员，有默认值，不是 HTTP 必填字段。业务 Execution 按 uav_id 映射覆盖 Map/展示，Home 在 Mock 配置/Execution，不能凭 UI 拼出一个不存在的统一 UAV Health schema。

### Context、Preference、VideoSource、Alert

| 对象/Field | Type | Required | Unit / Meaning | Source | Persistent | Versioned |
|---|---|---|---|---|---|---|
| Context.active_uav_id/active_alert_id/active_mission_id/active_security_plan_id/active_area_id | string/null | 输出恒有；patch可选 | 当前共享焦点；canonical UAV-xx | operational_context.h | 是 | context_version |
| Context.operation_mode | string | 输出恒有 | MONITOR/PLAN_EDIT/MISSION_EXECUTION/ALERT_RESPONSE | 同上 | 是 | 同上 |
| Context.context_version/updated_at/updated_by | int/number/string | 输出恒有 | 单调版本/Unix s/来源 | 同上 | 是 | 自身 |
| SharedPreference.language/ui_preferences_version/updated_at/updated_by | string/int/number/string | 输出恒有；更新须 language | en或zh-Hans/版本/Unix s/来源 | shared_view_store.h | 是 | ui_preferences_version |
| VideoView.video_target_uav_id/video_view_version/updated_at/updated_by | string或null/int/number/string | 输出恒有；更新须 target | 独立目标 | shared_view_store.h | 是 | video_view_version |
| VideoSource.video_url | string | 描述符输出恒有，可空 | 浏览器可播页面 URL；无独立 VideoSource REST 对象 | DroneRecord/FDroneDescriptor | 是 | 无 |
| ClientPresence.client_id/client_role/instance_id/hostname/app_version | string | 注册必需，长度1..128 | 身份/Command Map Video；非认证凭据 | RegisterContextClient | 否 | client_version |
| ClientPresence.connected_at/last_seen/hydrated/state/client_version | number或null/number/bool/string/int | 输出恒有 | Unix s/秒/补齐快照/ONLINE OFFLINE/版本 | ContextClients | 否 | client_version |
| FCommandAlert.Id/SharedId/ReceivedAt/DroneId/Type/Message/bHandled | int/string/FDateTime/int/string/string/bool | C++成员有默认/创建赋值 | 收到时间、本地操作与跨端ID | CommandAlertStore.h | 否，最近200条 | 无 |
| FOperationalEvent.Sequence/Timestamp/Category/Level/Source/EventType/TargetId/Message/Params | uint64/FDateTime/enum/enum/string×4/JSON object | 成员/投影创建 | Category SYSTEM OPERATION ALERT MISSION PLAN；Level INFO WARNING CRITICAL | OperationalEventStore.h/.cpp | UE投影不持久，Backend事件另存 | 本地Sequence+LastPlanSequence |

Alert wire 并没有通用 level 字段，不能虚构后端 Alert Level schema。普通 lost_connection/low_battery 通过 Backend callback；同步启用时由 OperationalContextSubsystem 的 WS 分支导入 SharedAlert，旧 Network callback 在此模式下退出以免重复。execution_failed 则由 MISSION_FAILED 事件投影产生。清本地 Log/标记 Alert handled 不是删除 Backend 事件，也没有相应 HTTP 删除接口。

## 5. 脱敏 Example Payload

下面是依据真实构造/消费代码制作的 schema 示例（值替换、并非本轮请求回包）；原生保存的完整 Plan/Route/Deployment/Execution/Event 样本见 Evidence/STAGE1-AUDIT/example-*.json。

注册后还必须 WS 订阅；不能直接以示例 instance_id 写业务：

```json
{"client_id":"DEMO-COMMAND","client_role":"Command","instance_id":"DEMO-INSTANCE","hostname":"DEMO-PC","app_version":"P5-Stage1"}
```

```json
{"type":"subscribe_context","instance_id":"DEMO-INSTANCE"}
```

```json
{"instance_id":"DEMO-INSTANCE","patch":{"active_uav_id":"UAV-01"}}
```

```json
{"instance_id":"DEMO-INSTANCE","action":"create_plan","name":"Demo Plan","default_mission_name":"Task 01","expected_version":0}
```

save_route 请求中的 path 子对象示例（两个点，location 仅展示兼容格式）：

```json
{"pathId":1,"bClosedLoop":false,"waypoints":[{"sequence":1,"latitude":39.981,"longitude":116.347,"altitude":60,"segmentSpeed":0,"waitTime":0,"location":{"x":0,"y":0,"z":1700}},{"sequence":2,"latitude":39.981,"longitude":116.3475,"altitude":60,"segmentSpeed":1,"waitTime":0,"location":{"x":4270.916,"y":-0.012,"z":1699.986}}]}
```

世界坐标只对该保存样本的原点有意义，不可直接移植到另一个地理原点。执行操作：

```json
{"instance_id":"DEMO-INSTANCE","action":"execution_start","plan_id":"plan-1","mission_id":"mission-2","request_id":"demo-start-1"}
```

```json
{"instance_id":"DEMO-INSTANCE","action":"execution_pause","execution_id":"execution-6","control_version":3,"request_id":"demo-pause-1"}
```

语言/视频分属不同端点：

```json
{"instance_id":"DEMO-INSTANCE","language":"zh-Hans"}
```

```json
{"instance_id":"DEMO-INSTANCE","video_target_uav_id":"UAV-01"}
```

元数据接口允许 final 空数组；以下为依据 AppendBatch 约束的最小结束批次，不是像素传输：

```json
{"schema_version":1,"mission_id":"demo-recording","drone_id":"1","batch_sequence":0,"frames":[],"final":true}
```

HTTP UAV response、telemetry YAML 和 JSON v1 control 示例另见 [真机专项](Stage1-Real-UAV-Interface-Audit.md)。

## 6. 数据接口总表

| ID | Interface | Protocol | Direction | Producer | Consumer | Data | Frequency | Current State |
|---|---|---|---|---|---|---|---|---|
| A01 | Real Telemetry | UDP YAML | Jetson→Backend | PX4 ROS2 bridge | UdpReceiver/DroneManager | TelemetryData | 配置10 Hz | LEGACY_PRESERVED；历史接收 |
| A02 | Flight Control | UDP JSON v1 + ROS2 | Backend→Jetson→PX4 | CommandQueue | bridge/TrajectorySetpoint | move/hold target/ACK | Backend 5 Hz；PX4 50 Hz | LEGACY_PRESERVED |
| A03 | Legacy Mission | HTTP/WS→内部执行 | UE→Backend→A02 | 旧序列/路径工具 | Assembly/ExecutionEngine | arrays/waypoints | 操作/执行回调 | LEGACY_PRESERVED |
| A04 | Real Video | RTSP→WebRTC | Camera→MediaMTX→UE | ROS image/GStreamer | UWebBrowser | H264/browser media | 按配置/实际采集 | LEGACY_PRESERVED；未重验 |
| A05 | Heartbeat/ACK/Latency | UDP JSON/YAML+ROS2 | 双向 | HeartbeatManager/bridge | DroneManager/bridge/PX4 | hold,control_ack,ping/pong | 5 Hz/50 Hz；诊断按需 | LEGACY_PRESERVED |
| B01 | HTTP | HTTP JSON | 双向请求响应 | 客户端/Launcher/旧工具 | Backend/客户端 | 33 endpoint | 按操作/轮询 | 部分活跃，详表 |
| B02 | WS | WebSocket JSON | 双向 | Backend/三端 | 三端/Backend | 23 type + mode | 事件/heartbeat/tick | 部分活跃，详表 |
| B03 | Persistence | JSON/JSONL | Backend↔文件 | Stores | Stores | 7族 | 事务/tick/批次 | 当前+Legacy |
| C01 | Command↔Backend | HTTP/WS | 双向 | Command | Context/Plan/UI | 业务写入与快照 | 按操作/推送 | CURRENT_ACTIVE |
| C02 | Map↔Backend | HTTP/WS | 双向 | Map | route/store/render | route session/geographic execution | 操作/10Hz目标 | CURRENT_ACTIVE |
| C03 | Video↔Backend | HTTP/WS | 双向 | Video | preference/view/Registry | identity/video URL/target | 操作/推送 | CURRENT_ACTIVE，媒体为空 |
| D01 | Geographic↔UE | C++ Cesium | 双向 | 点击/位置 | route/actor | lat/lon/h/world | 编辑/位置更新 | G01/G02 |
| D02 | GPS/local↔offset | C++/Python | 方向见函数 | Telemetry/旧控制 | Registry/UDP | WGS84/NED/ENU | 流/操作 | G03–G12 |
| E01 | Plan/Mission/Route/Deployment/Execution | Store JSON | Backend→三端；写经 action | SecurityPlanStore | 业务 UI | 版本化对象与冻结快照 | 事务/tick | CURRENT_ACTIVE |
| E02 | UAV Registry | UE subsystem | HTTP/WS→Registry→UI/Actor | DroneNetworkManager | 三端本地消费者 | 描述符/状态 | 轮询/事件 | CURRENT_ACTIVE |
| E03 | Alert/Event/Context | JSON→UE投影 | Backend→三端 | callbacks/stores | AlertStore/EventStore/ContextSubsystem | 选择/告警/日志 | 事件触发 | CURRENT_ACTIVE |

视频接口 5 组统计：V01 Jetson ROS image/camera_info 输入；V02 GStreamer RTSP 输出；V03 MediaMTX WebRTC 浏览器页；V04 Backend video_url + VideoView 目标元数据；V05 HTTP video-metadata/batch。MP4 fixture 是 V03 的替代演示输入，不另算真机链路。
