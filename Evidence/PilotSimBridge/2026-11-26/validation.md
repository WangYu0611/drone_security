# 驾驶员 Mock 输入桥验证

2026-11-26 完成隔离验收。`30c778e` 对齐 v1 手动控制逻辑。`d21b640` 将本机摇杆输入接入 Backend 已有的 Pilot 控制会话。 [驾驶员 Mock 输入桥](../../../tools/pilot_sim_bridge/README.md)

## 隔离验收

使用 Node.js 26.13.1。在仓库根目录设置 `DRONE_BACKEND_EXE` 指向待验证的 Backend 构建，然后运行下方命令。

```powershell
node Backend/tests/pilot_control_integration.mjs
```

脚本创建独立 Backend、临时凭据和 Mock 数据，使用本机临时端口。测试配置显式启用模拟、关闭调试接口，端口映射为空。脚本退出时关闭自身启动的进程和连接，并删除临时凭据。以下各次验收均确认凭据文件已删除，结果记录中的 Backend 和输入桥进程均已退出。构建及验证数据保存在本机测试目录。

| 阶段 | 运行标识 | 通过 | Backend EXE SHA-256 |
| --- | --- | --- | --- |
| `30c778e` Mock 控制 | `protocol-Sbfjnf` | 22/22 | `cdbb01da49c57f4ff886a8a0dc08e3b260b5ac188bba3dadf2350615d70b7890` |
| `d21b640` 本机 UDP 输入桥 | `protocol-OwvYbT` | 27/27 | `432ba2e447fec9132541822fe8ad7b195096c34de855f3e747f22f8abd467edc` |
| `d21b640` 按手册复跑 | `protocol-KBEExw` | 27/27 | `432ba2e447fec9132541822fe8ad7b195096c34de855f3e747f22f8abd467edc` |

同轮运行的 Backend 单元测试共 25 组，214/214 通过。手册的 PowerShell 示例通过语法检查。

### Mock 控制

协议测试覆盖 Command 授权和撤销、Video/Map 交接申请、单机任务释放、指定 Pilot 会话、旧输入隔离和重启处理。位移检查覆盖显式起飞、起飞初期禁止水平移动、空中最低高度、位置及速度包线、单次飞行动作、急停和输入超时。使用已有模拟航向验证机体系输入方向，偏航输入保持航向不变。

### 输入桥

额外检查从本机 UDP 输入、认证 Pilot WebSocket 到 Backend 模拟位移的链路，覆盖授权与新鲜回中输入、机体系轴向、一次性动作、异源和异会话输入、重复序号和越界轴值。停发后模拟位移停止，Backend 退出操控会话。启用测试中的桥记录为 `listening → active → stopped`，以 `INPUT_TIMEOUT` 和退出码 0 结束。含 Real 配置标记的隔离反例拒绝启动桥，同时保留普通 Mock 客户端行为，桥记录 `MOCK_BRIDGE_START_FAILED` 并以退出码 1 结束。

### UE 和飞控验证范围

第一阶段的 `PilotShellWidget.cpp` 单文件编译通过。UE Editor 的完整构建未完成，未进行原生 Pilot 界面测试。本机未发现可运行的 PX4 SITL，未执行 PX4 软件在环或真实飞机验证。上表的位移和动作结果均来自独立 Backend Mock。

## PX4 SITL 后续验收

- 准备可运行的 PX4 SITL 环境和模拟执行端接入。本次输入桥的执行端为 Backend Mock。
- 记录 PX4 SITL 版本、模拟场景和对应项目构建，确认测试环境只使用仿真数据及资源。
- 分别记录仿真器位置、Backend 遥测和 UE 地图显示，核对各端的时间戳、对应机号和位置变化。
- 保存授权、操控开始、停止输入、会话退出与清理结果的时序。核对停发后的仿真位置保持、旧会话的失效、已授权状态的保留及新会话的显式启用。
- 单独保存原生 UE 界面操作记录，注明覆盖的界面、构建和输入来源。
