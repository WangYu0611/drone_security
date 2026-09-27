# P5.4 UX-01 FINAL RESULT

交付日期：2026-09-26。已实现 Map 本地航线编辑视角锁定。已验证用例通过；完整原生输入覆盖尚未闭合，交付状态为 **CONDITIONAL PASS**，不声明整体全绿。

Branch: `feat/triple-screen-p5-4-3d-multi-uav`

Base HEAD: `f19d6e8b7e4abc9b5ff6c12cb48abccf789b383f`

Final HEAD: 包含本报告的独立交付提交；完整 SHA 见交付回复或 `git log -1`。

Commit: `fix(p5.4): lock map camera during route editing`

开发前已确认该独立工作树在指定基线且 clean。未修改 Backend、Security Plan schema、协议或 Shared Operational Context；未调整浏览阻尼算法及参数。无 push、merge、reset、stash 或历史改写。P5.5 未开始。

## 交互结果

开始编辑时立即锁定，并把 Camera Target 一次性对齐当前 Actual，丢弃未完成的浏览目标与旧手势，不移动当前相机。锁定期间拦截输入、Focus 和模式切换入口，相机更新提前返回，不逐帧覆盖 Transform。停止 Follow 并清除待处理的选择跟随；编辑期间收到的执行自动构图请求被消费，退出后不补发。

航点仍由独立编辑输入处理。原生地面点击完整持有 down/move/up，越过拖动阈值或离开地图后不再加点；航点拖出地图停止更新并保留最后有效位置。新增航点要求当前视口的有效 blocking WorldStatic 命中，复用现有坐标及高度转换。旧的手输地理坐标新增入口隐藏，避免绕过当前视口范围。

保存草稿及脏数据退出提示期间保持锁定；完成或放弃退出收到确认后解锁。重开方案默认浏览。界面显示本地化锁定状态、编辑前调整范围提示和禁用模式按钮提示，不增加确认步骤。

下表中的“原生 PASS”指最终 build-04 在 Stage1 Launcher 启动的 Map Runtime 中经原生操作与画面观察确认；相机数值不变另由 UE 组件断言支持。截图不能替代数值断言。“组件 PASS”不代表原生硬件输入验证。

| Browse Mode | 结果 |
|---|---|
| 2D Pan | 原生 PASS，截图 11、36 |
| 2D Zoom | 原生 PASS，12、37 |
| 3D Orbit | 组件回归 PASS；**NOT NATIVE VERIFIED** |
| 3D Pan | 组件回归 PASS；**NOT NATIVE VERIFIED** |
| 3D Zoom | 原生 PASS，40、54 |

| Route Edit Camera Lock | 结果 |
|---|---|
| 2D Pan blocked | 原生 PASS，空地图拖动不平移、不加点，15 |
| 2D Zoom blocked | 原生 PASS，上下滚轮，16–17 |
| 3D Orbit blocked | 组件 PASS；**NOT NATIVE VERIFIED** |
| 3D Pan blocked | 组件 PASS；**NOT NATIVE VERIFIED** |
| 3D Zoom blocked | 原生 PASS，43 |
| Double Click Focus blocked | 2D/3D 原生 PASS，选中 P03 并更新 Inspector，25、44 |
| Follow Camera blocked | 组件 PASS：进入编辑停止 Follow、清除排队选择，退出不恢复；**未做实时 UAV Follow 原生场景** |
| 自动执行构图 | 组件 PASS：锁定时消费执行构图请求，解锁不重新排队；遥测展示继续刷新 |
| 开始编辑无跳变 | 原生观察及在途阻尼数值断言 PASS |

| Waypoint Editing | 结果 |
|---|---|
| Add | 原生 PASS，2D 点击新增 1 点（22）、3D 微拖 2×1 像素新增 1 点（46） |
| Select | 原生 PASS，2D/3D P03 选择和 Inspector 更新 |
| Drag | 原生 PASS，2D P01 与 3D P03 移动，点数不变，相机固定，19、45 |
| Delete | 原生 PASS，删除测试 P05 后恢复 4 点，23、47 |
| Altitude | 原生 PASS，P03 120→130→120，26–27；3D 再调至 130 后放弃，49–53 |
| Altitude Reference | 原生 PASS，Ellipsoid / AGL 切换，30–31；转换组件回归通过 |
| Hover | 原生 PASS，P03 保存 10 秒，28；Backend 快照核对 |
| Speed | 原生 PASS，P03 保存 8 m/s，29；Backend 快照核对 |
| Inspector Scroll | 原生 PASS，32，相机不缩放 |

| Viewport Boundary | 结果 |
|---|---|
| Edge drag | 原生 PASS，P01 拖入 Inspector、离开地图时相机不动，20；保持最后有效位置 |
| Invalid hit | 已实现 blocking WorldStatic/有效坐标守卫及中英文轻量反馈；**NOT NATIVE VERIFIED**，本次未取得可靠无地面射线场景 |
| Auto-pan | 已执行的边界拖动原生 PASS；组件相机固定断言 PASS |
| Empty map drag | 最终原生 PASS，不误加点（15）；组件覆盖越阈值后返回、出界后返回均不加点 |
| Micro drag | 原生及组件 PASS，阈值内按点击新增恰好 1 点 |

| Mode Switch / 编辑生命周期 | 结果 |
|---|---|
| Disabled during edit | 原生 PASS，2D/3D 按钮禁用，18；服务层同样拒绝切换 |
| Restored after edit | 原生 PASS，38–40、55–56 |
| Save Draft | 原生 PASS，ACK 后仍编辑且锁定，33–34；保存中暂停编辑输入时仍锁定的组件断言通过 |
| Finish Edit | 原生 PASS，无跳变；Pan / Zoom 恢复，35–37 |
| Cancel Edit | 原生 PASS，脏数据提示期间滚轮仍被阻断（51–52），放弃退出后恢复保存内容并解锁（53–56） |
| Plan reopen | 原生 PASS，57，重新打开为只读浏览状态 |

最终 Backend 航线仍为 4 点：P01/P02/P03/P04 高度为 **40/80/120/60 m AGL**；P03 悬停 **10 s**、速度 **8 m/s**。后续 3D 拖动和 130 m 测试均通过放弃退出丢弃，未持久化。快照见 [plans.json](../Evidence/TASK-P5.4/ux-01/native-final/plans.json)。

## 自动化、构建和本地化

| 验证 | 发现 / 完成 / 成功 / 失败 | Aggregate / 进程 |
|---|---|---|
| 离线 RouteEditCameraLock.2D / .3D | 2 / 2 / 2 / 0，无测试警告 | PASS，exit 0 |
| 最终 Workflow.MapLiveDraftGuard | 1 / 1 / 1（带警告）/ 0 | PASS，exit 0 |
| 最终 P54.CameraDampingAnd3DRoute | 1 / 1 / 1（带警告）/ 0 | PASS，exit 0 |
| 最终 P52.MapExecutionMonitor | 1 / 1 / 1（带警告）/ 0 | PASS，exit 0 |
| 最终 Localization.FTextAndDraftIsolation | 1 / 1 / 1（带警告）/ 0 | PASS，exit 0 |

离线测试位于空 Editor World，无 Backend、在线 Cesium tiles 或模拟执行 fixture。它断言进入锁定不跳变、旧手势清理、所有 Camera Target 与 Actual Transform 连续 600 帧不变、重复锁定幂等、释放旧手势无跳变，以及退出后 Zoom / Focus / Mode / Orbit 恢复。最后一次离线运行使用 build-03，后续 build-04 未改动相机服务或该测试；build-04 增补执行自动构图抑制及相应回归。

MapLiveDraftGuard 同时验证编辑与 Camera 相互独立、保存期间锁定、Follow 停止、航点拖动与参数修改、地面点击交易的拖动取消边界；这些组件交易断言并非原生射线命中测试。保留现有 Inspector 与 3D route/damping 回归。

最终四项分别使用全新 Editor 进程串行运行，均有 `Test Completed. Result=` 完成记录，不以单独 exit 0 判定成功。启动前使用隔离 Backend 数据运行 P4 工作流、P5.2 Mock 执行协议 fixture 及 UE 航线 fixture，命令均成功。这是范围内回归，未运行全 UE / 全 Backend 套件，不代表真实无人机或真实视频验收。

证据：

- [离线结果](../Evidence/TASK-P5.4/ux-01/offline-1790436340508172000/summary.json)
- [最终四项结果](../Evidence/TASK-P5.4/ux-01/ue-1790437430144301200/summary.json)
- [最终构建日志](../Evidence/TASK-P5.4/ux-01/build-04.log)：Development Editor，Succeeded，19.41 秒。
- [最终构建 SHA256 清单](../Evidence/TASK-P5.4/ux-01/final-build-manifest.json)：源码、测试、本地化资源及 DLL；验收结束后核对一致。

新增文本进入既有 ProductText / FText 与 en、zh-Hans 资源。最终 FText 自动化和原生中英文锁定提示通过（英文 14、中文 50）。本地化生成结果须分开解释：首次 PowerShell 参数导致配置文件名被错误传递，保留失败；修正为 Python argv 后 GatherText 内部完成并返回 0，生成资源，但整个命令进程因既有 GameFeatureData AssetManager 错误返回 **1**，不能记为命令整体 PASS。见 [localization-02-result.json](../Evidence/TASK-P5.4/ux-01/localization-02-result.json) 及对应完整日志。

## Native Verification 与已知限制

原生证据共 57 张；**10–57 为最终 build-04**，见 [原生结果](../Evidence/TASK-P5.4/ux-01/native-result.json) 和 [观察时间记录](../Evidence/TASK-P5.4/ux-01/native/observations.jsonl)。运行范围为隔离 Backend + Map。Command 仅短暂启动用于准备，因同可执行文件、同标题窗口捕获混淆而通过 Launcher 停止，之后在唯一 Map 窗口中验收。Video 未启动；不声称本次三客户端验收。截图 01 实为 Command，不能作为 Map 证据。

中间 build-02 的空地图拖动曾意外新增 P05，原失败截图 [08](../Evidence/TASK-P5.4/ux-01/native/08-edit-empty-drag.png) 和撤销 [09](../Evidence/TASK-P5.4/ux-01/native/09-undo-unintended-p05.png) 保留。之后修复原生地面点击完整手势归属，最终同类拖动 [15](../Evidence/TASK-P5.4/ux-01/native/15-final-empty-drag-no-add.png) 不再加点；没有覆盖或删除失败证据。

已知限制：

- 原生工具 `sky.drag` 不支持指定 mouse button 或持续时间，无法验证持续右键 Orbit、中键 Pan，明确为 **NOT NATIVE VERIFIED**；需要用户后续人工覆盖浏览及编辑两种状态。
- 无效 Ground/Terrain hit 的轻量反馈尚未原生触发；实时 UAV Follow 场景只完成组件/源码验证。
- 本次范围内 UE Aggregate 均通过但带警告；启动日志仍含既有 Cesium 枚举未初始化和 GameFeatureData 错误。未过滤错误、未宣称全套 UE 环境健康；本次未复现 Niagara / Gaussian Splat 崩溃。
- 原生 3D 仍有既有 Lumen 曝光提示、低高度 P01 的建筑遮挡。未调整地形、网络或渲染设置。
- 无航点选择时，既有禁用 Inspector 字段可保留上次显示值；截图 57 的禁用高度字段不是持久化结果，已以路线标签及 Backend 快照核对实际 120 m。

本次启动的原生进程已通过 Launcher 停止，最终 [ownership.json](../Evidence/TASK-P5.4/ux-01/native-final/runtime/ownership.json) 为空；最终回归也通过自身 Runtime 清理所属进程。原 fixture 文件不变，原始来源哈希见 [native-fixture-origin.json](../Evidence/TASK-P5.4/ux-01/native-fixture-origin.json)。

审阅日志仅对 JWT 值脱敏，原始日志保存在忽略目录 `Saved/UX01RawEvidence`，原始/审阅 SHA256 与替换数量写入 [redaction-manifest.json](../Evidence/TASK-P5.4/ux-01/redaction-manifest.json)。失败、警告、断言和截图均保留。

Deferred: 原生右/中键持续拖动、无效地面命中与实时 Follow 人工补验；不开展临时解锁、边缘滚屏、相机预设、Cesium 网络、Gaussian Splat 修复或 P5.5。

git status: 提交后状态及完整 Final HEAD 由交付回复记录；本报告和范围内源码、测试、资源、证据一起独立提交。
