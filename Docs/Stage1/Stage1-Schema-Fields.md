# Stage 1 字段清单附录

本附录逐字段列出已保存原生样本的字段、观测类型、单位、语义与来源；不是自动推导的验证 Schema。Required 区分源码输出构造与请求必填，不能把样本存在当成请求必填。嵌套 snapshot 引用同名业务对象，route_snapshot 引用 Route。null 是允许观察到的值，不代表字段可省略。输入约束见主目录。

## SecurityPlan

Source: `Backend/storage/security_plan_store.h`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `id` | string | 输出构造恒有（非请求必填） | — | 对象标识 | 是 | document version |
| `name` | string | 输出构造恒有（非请求必填） | — | 名称 | 是 | document version |
| `description` | string | 输出构造恒有（非请求必填） | — | 描述 | 是 | document version |
| `status` | string | 输出构造恒有（非请求必填） | — | 配置状态 | 是 | document version |
| `content_revision` | integer | 输出构造恒有（非请求必填） | — | 配置内容修订 | 是 | document version |
| `review` | null / object | 输出构造恒有（非请求必填） | — | 人工复核记录 | 是 | document version |
| `version` | integer | 输出构造恒有（非请求必填） | — | 事务/对象版本 | 是 | document version |
| `created_at` | number | 输出构造恒有（非请求必填） | Unix s | 创建时间 | 是 | document version |
| `updated_at` | number | 输出构造恒有（非请求必填） | Unix s | 最后更新时间 | 是 | document version |
| `mission_ids` | array | 输出构造恒有（非请求必填） | — | 任务成员标识 | 是 | document version |
| `workflow_step` | string | 条件/兼容字段 | — | 流程页面阶段 | 是 | document version |
| `workflow_mission_id` | string | 条件/兼容字段 | — | 流程关联任务 | 是 | document version |
| `validation` | object | 条件/兼容字段 | — | 检查结果 | 是 | document version |
| `deployment_id` | string | 条件/兼容字段 | — | 部署标识 | 是 | document version |
| `deployment` | null | 条件/兼容字段 | — | 复制时清空的兼容字段 | 是 | document version |
| `source_plan_id` | string | 条件/兼容字段 | — | 复制来源方案 | 是 | document version |
| `source_deployment_id` | string | 条件/兼容字段 | — | 复制来源部署 | 是 | document version |

## Mission

Source: `Backend/storage/security_plan_store.h`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `id` | string | 输出构造恒有（非请求必填） | — | 对象标识 | 是 | document version |
| `plan_id` | string | 输出构造恒有（非请求必填） | — | 所属方案 | 是 | document version |
| `name` | string | 输出构造恒有（非请求必填） | — | 名称 | 是 | document version |
| `assigned_uav_id` | string | 输出构造恒有（非请求必填） | — | 任务分配 UAV；不等于当前选择 | 是 | document version |
| `route_id` | string | 输出构造恒有（非请求必填） | — | 航线标识 | 是 | document version |
| `status` | string | 输出构造恒有（非请求必填） | — | 配置状态 | 是 | document version |

## Route

Source: `Backend/storage/security_plan_store.h`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `pathId` | integer | 条件/兼容字段 | — | 旧 Path 数字标识 | 是 | document version |
| `bClosedLoop` | boolean | 条件/兼容字段 | — | 旧航线闭环标记；Mock 未据此循环 | 是 | document version |
| `waypoints` | array | 输出构造恒有（非请求必填） | — | 有序航点 | 是 | document version |
| `route_id` | string | 输出构造恒有（非请求必填） | — | 航线标识 | 是 | document version |
| `revision` | integer | 输出构造恒有（非请求必填） | — | 航线修订 | 是 | document version |

## Deployment

Source: `Backend/storage/security_plan_store.h`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `id` | string | 输出构造恒有（非请求必填） | — | 对象标识 | 是 | document version |
| `plan_id` | string | 输出构造恒有（非请求必填） | — | 所属方案 | 是 | document version |
| `content_revision` | integer | 输出构造恒有（非请求必填） | — | 配置内容修订 | 是 | document version |
| `deployed_at` | number | 输出构造恒有（非请求必填） | Unix s | 部署时间 | 是 | document version |
| `deployed_by` | string | 输出构造恒有（非请求必填） | — | 部署客户端 | 是 | document version |
| `reviewed_by` | string | 输出构造恒有（非请求必填） | — | 复核客户端 | 是 | document version |
| `reviewed_at` | number | 输出构造恒有（非请求必填） | Unix s | 复核时间 | 是 | document version |
| `snapshot` | object | 输出构造恒有（非请求必填） | — | 部署冻结对象 | 是 | document version |

## Execution

Source: `Backend/storage/mock_execution.inl`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `execution_id` | string | 输出构造恒有（非请求必填） | — | 执行标识 | 是 | execution_version |
| `deployment_id` | string | 输出构造恒有（非请求必填） | — | 部署标识 | 是 | execution_version |
| `plan_id` | string | 输出构造恒有（非请求必填） | — | 所属方案 | 是 | execution_version |
| `mission_id` | string | 输出构造恒有（非请求必填） | — | 所属任务 | 是 | execution_version |
| `route_id` | string | 输出构造恒有（非请求必填） | — | 航线标识 | 是 | execution_version |
| `route_revision` | integer | 输出构造恒有（非请求必填） | — | 冻结航线修订 | 是 | execution_version |
| `route_snapshot` | object | 输出构造恒有（非请求必填） | — | 执行冻结航线 | 是 | execution_version |
| `plan_name` | string | 输出构造恒有（非请求必填） | — | 创建执行时方案名 | 是 | execution_version |
| `mission_name` | string | 输出构造恒有（非请求必填） | — | 创建执行时任务名 | 是 | execution_version |
| `uav_id` | string | 输出构造恒有（非请求必填） | — | 执行 UAV | 是 | execution_version |
| `state` | string | 输出构造恒有（非请求必填） | — | 执行/编辑状态 | 是 | execution_version |
| `control_version` | number | 输出构造恒有（非请求必填） | — | 控制并发版本 | 是 | execution_version |
| `current_waypoint` | integer | 输出构造恒有（非请求必填） | — | 当前航点，一基 | 是 | execution_version |
| `completed_waypoints` | integer | 输出构造恒有（非请求必填） | — | 已完成点数 | 是 | execution_version |
| `total_waypoints` | integer | 输出构造恒有（非请求必填） | — | 总点数 | 是 | execution_version |
| `segment_progress` | number | 输出构造恒有（非请求必填） | 0..1 | 当前航段进度 | 是 | execution_version |
| `overall_progress` | number | 输出构造恒有（非请求必填） | 0..1 | 总体进度 | 是 | execution_version |
| `progress` | number | 输出构造恒有（非请求必填） | 0..1 | 按距离进度 | 是 | execution_version |
| `position` | object | 输出构造恒有（非请求必填） | — | 当前地理位置 | 是 | execution_version |
| `segment_start` | object | 输出构造恒有（非请求必填） | — | 当前航段起点 | 是 | execution_version |
| `home_position` | object | 输出构造恒有（非请求必填） | — | 独立模拟 Home | 是 | execution_version |
| `default_speed_mps` | number | 输出构造恒有（非请求必填） | m/s | 默认速度 | 是 | execution_version |
| `distance_m` | number | 输出构造恒有（非请求必填） | m (altitude 基准见正文) | 初始总距离 | 是 | execution_version |
| `estimated_seconds` | number | 输出构造恒有（非请求必填） | s | 预计时长 | 是 | execution_version |
| `distance_travelled_m` | number | 输出构造恒有（非请求必填） | m (altitude 基准见正文) | 累计距离 | 是 | execution_version |
| `wait_remaining` | number | 输出构造恒有（非请求必填） | s | 剩余等待 | 是 | execution_version |
| `elapsed_seconds` | number | 输出构造恒有（非请求必填） | s | 积分执行时长，不等于墙钟差 | 是 | execution_version |
| `created_at` | number | 输出构造恒有（非请求必填） | Unix s | 创建时间 | 是 | execution_version |
| `started_at` | number | 输出构造恒有（非请求必填） | Unix s | 开始时间 | 是 | execution_version |
| `updated_at` | number | 输出构造恒有（非请求必填） | Unix s | 最后更新时间 | 是 | execution_version |
| `completed_at` | number | 输出构造恒有（非请求必填） | Unix s | 终止时间 | 是 | execution_version |
| `completion_reason` | string | 输出构造恒有（非请求必填） | — | 完成原因 | 是 | execution_version |
| `failure_reason` | string | 输出构造恒有（非请求必填） | — | 失败原因 | 是 | execution_version |
| `simulation` | boolean | 输出构造恒有（非请求必填） | — | 模拟身份 | 是 | execution_version |

## Event

Source: `Backend/storage/security_plan_store.h + mock_execution.inl`；样本 `Evidence/TASK-P5.2/native/final-state.json`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `sequence` | integer | 输出构造恒有（非请求必填） | — | 顺序号 | 是 | document version；Execution 事件随 execution_version |
| `timestamp` | number | 输出构造恒有（非请求必填） | Unix s | 事件时间 | 是 | document version；Execution 事件随 execution_version |
| `event_type` | string | 输出构造恒有（非请求必填） | — | 业务事件名 | 是 | document version；Execution 事件随 execution_version |
| `params` | object | 输出构造恒有（非请求必填） | — | 本地化及关联参数 | 是 | document version；Execution 事件随 execution_version |
| `category` | string | 输出构造恒有（非请求必填） | — | 事件分类 | 是 | document version；Execution 事件随 execution_version |
| `target_id` | string | 输出构造恒有（非请求必填） | — | 事件目标，执行事件是 execution_id | 是 | document version；Execution 事件随 execution_version |
| `message` | string | 输出构造恒有（非请求必填） | — | 后端文字 | 是 | document version；Execution 事件随 execution_version |
| `source` | string | 输出构造恒有（非请求必填） | — | 来源 | 是 | document version；Execution 事件随 execution_version |
| `execution_snapshot` | object | 条件/兼容字段 | — | 事件时执行状态投影 | 是 | document version；Execution 事件随 execution_version |

## Waypoint

Source: `Backend/storage/security_plan_store.h::checkPath`、`MapMissionRouteWidget.cpp::Save`。

| Field | Type | Required | Unit | Meaning | Persistent | Versioned |
|---|---|---|---|---|---|---|
| `sequence` | integer | 请求必需，checkPath检查 | — | 顺序号 | 随 Route | Route revision |
| `latitude` | number | 请求必需，checkPath检查 | degree | 纬度 | 随 Route | Route revision |
| `longitude` | number | 请求必需，checkPath检查 | degree | 经度 | 随 Route | Route revision |
| `altitude` | number | 请求必需，checkPath检查 | m (altitude 基准见正文) | 地理高度 | 随 Route | Route revision |
| `segmentSpeed` | integer | 请求必需，checkPath检查 | m/s | 抵达该点航段速度；0 使用 Mock 默认 | 随 Route | Route revision |
| `waitTime` | integer | 请求必需，checkPath检查 | s | 抵达等待时间 | 随 Route | Route revision |
| `location` | object | 可选兼容字段 | — | UE 世界坐标缓存 | 随 Route | Route revision |

## 嵌套对象字段

| Object.Field | Type | Required | Unit / Meaning | Source | Persistent / Versioned |
|---|---|---|---|---|---|
| review.content_revision | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 配置内容修订 | `SecurityPlanStore::transact(review)` | 随宿主文档/版本 |
| review.reviewed_at | number | 该条件对象存在时构造；location仅UE兼容字段 | Unix s / 复核时间 | `SecurityPlanStore::transact(review)` | 随宿主文档/版本 |
| review.reviewed_by | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 复核客户端 | `SecurityPlanStore::transact(review)` | 随宿主文档/版本 |
| validation.ready | boolean | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.issues | array | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.mission_count | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.assigned_count | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.routes_ready | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.validated_content_revision | integer/null | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate/transact` | 随宿主文档/版本 |
| validation.issues[].mission_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 所属任务 | `SecurityPlanStore::validate` | 随宿主文档/版本 |
| validation.issues[].code | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::validate` | 随宿主文档/版本 |
| validation.issues[].params | object | 该条件对象存在时构造；location仅UE兼容字段 | — / 本地化及关联参数 | `SecurityPlanStore::validate` | 随宿主文档/版本 |
| edit_sessions[].edit_session_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].plan_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 所属方案 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].mission_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 所属任务 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].owner_instance_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].owner_client_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].base_content_revision | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].state | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 执行/编辑状态 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].started_at | number | 该条件对象存在时构造；location仅UE兼容字段 | Unix s / 开始时间 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].last_seen_at | number | 该条件对象存在时构造；location仅UE兼容字段 | Unix s / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| edit_sessions[].lease_expires_at | number | 该条件对象存在时构造；location仅UE兼容字段 | Unix s / 同名业务属性，见源码 | `SecurityPlanStore::transact(begin_route_edit)` | 随宿主文档/版本 |
| execution_requests[].request | object | 该条件对象存在时构造；location仅UE兼容字段 | — / 同名业务属性，见源码 | `mock_execution.inl::executionRequest` | 随宿主文档/版本 |
| execution_requests[].execution_id | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 执行标识 | `mock_execution.inl::executionRequest` | 随宿主文档/版本 |
| position / home_position / segment_start.latitude | number | 该条件对象存在时构造；location仅UE兼容字段 | degree / 纬度 | `mock_execution.inl::position` | 随宿主文档/版本 |
| position / home_position / segment_start.longitude | number | 该条件对象存在时构造；location仅UE兼容字段 | degree / 经度 | `mock_execution.inl::position` | 随宿主文档/版本 |
| position / home_position / segment_start.altitude | number | 该条件对象存在时构造；location仅UE兼容字段 | m (altitude 基准见正文) / 地理高度 | `mock_execution.inl::position` | 随宿主文档/版本 |
| Waypoint.location.x | number | 该条件对象存在时构造；location仅UE兼容字段 | UE cm / 同名业务属性，见源码 | `MapMissionRouteWidget route serialization` | 随宿主文档/版本 |
| Waypoint.location.y | number | 该条件对象存在时构造；location仅UE兼容字段 | UE cm / 同名业务属性，见源码 | `MapMissionRouteWidget route serialization` | 随宿主文档/版本 |
| Waypoint.location.z | number | 该条件对象存在时构造；location仅UE兼容字段 | UE cm / 同名业务属性，见源码 | `MapMissionRouteWidget route serialization` | 随宿主文档/版本 |
| execution_snapshot.state | string | 该条件对象存在时构造；location仅UE兼容字段 | — / 执行/编辑状态 | `mock_execution.inl::executionEvent` | 随宿主文档/版本 |
| execution_snapshot.position | object | 该条件对象存在时构造；location仅UE兼容字段 | — / 当前地理位置 | `mock_execution.inl::executionEvent` | 随宿主文档/版本 |
| execution_snapshot.progress | number | 该条件对象存在时构造；location仅UE兼容字段 | 0..1 / 按距离进度 | `mock_execution.inl::executionEvent` | 随宿主文档/版本 |
| execution_snapshot.current_waypoint | integer | 该条件对象存在时构造；location仅UE兼容字段 | — / 当前航点，一基 | `mock_execution.inl::executionEvent` | 随宿主文档/版本 |

## UE Registry 逐字段

Source: `Source/UE5DroneControl/DroneOps/Core/DroneOpsTypes.h`。完整声明另存 registry-structs.txt。C++成员不是网络必填；运行时值的有效性由availability/valid标志决定。

### FDroneDescriptor

| Field | Type | Required | Unit / Meaning | Persistent | Versioned |
|---|---|---|---|---|---|
| Name | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| DroneId | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Slot | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| BackendIdString | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| IpAddress | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| ControlPort | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| MavlinkSystemId | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| BitIndex | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| ThemeColor | FLinearColor | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| UEReceivePort | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| TopicPrefix | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| VideoUrl | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| bIsEnemyTarget | bool | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| EnemyInitialLocation | FVector | C++声明成员；非网络必填 | UE cm | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
### FDroneTelemetrySnapshot

| Field | Type | Required | Unit / Meaning | Persistent | Versioned |
|---|---|---|---|---|---|
| DroneId | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Availability | EDroneAvailability | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| WorldLocation | FVector | C++声明成员；非网络必填 | UE cm | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| NedLocation | FVector | C++声明成员；非网络必填 | NED m | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| GeographicLocation | FVector | C++声明成员；非网络必填 | Lat,Lon,Alt (degree/degree/m) | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Velocity | FVector | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Attitude | FRotator | C++声明成员；非网络必填 | degree | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Altitude | float | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Battery | int32 | C++声明成员；非网络必填 | % (-1 unknown) | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| bArmed | bool | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| bOffboard | bool | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| bGpsFix | bool | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| bLocalPositionValid | bool | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| LastUpdateTime | double | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| GpsLatitude | double | C++声明成员；非网络必填 | degree | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| GpsLongitude | double | C++声明成员；非网络必填 | degree | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| GpsAltitude | double | C++声明成员；非网络必填 | m AMSL | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| BatteryPercent | int32 | C++声明成员；非网络必填 | % (-1 unknown) | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| TaskMode | EDroneCommandMode | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| TaskState | EDroneTaskState | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| TaskErrorDetail | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| CurrentWaypointIndex | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| TotalWaypoints | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| LocalState | EUELocalDroneState | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
### FDroneTaskStateSnapshot

| Field | Type | Required | Unit / Meaning | Persistent | Versioned |
|---|---|---|---|---|---|
| DroneId | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| ArrayId | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Mode | EDroneCommandMode | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| State | EDroneTaskState | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| CurrentWaypoint | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| WaypointCount | int32 | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| Detail | FString | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
| UpdatedAt | double | C++声明成员；非网络必填 | 本地运行字段；由网络/Registry赋值 | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |
