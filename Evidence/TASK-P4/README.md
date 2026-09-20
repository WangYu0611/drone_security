# TASK-P4 Evidence Index

## Final Result

**CONDITIONAL PASS**。业务验收 A–N、P4 HTTP/WS、最终定向 UE 测试通过。Backend 两项历史 planner 失败保留；局部遗留显示、本地化命令进程诊断和网络广播原子性边界见最终报告。没有真实飞机或真实视频流通过声明。

- [正式报告](../../Docs/TASK-P4-SecurityPlanWorkflowLocalization.md)
- [原生 A–N 表](native-acceptance.md)
- [原始需求](request.md) / [续作收口要求](closure/resume-request.md)

## Selected final result

| 域 | 结果 | 文件 |
|---|---|---|
| UE5.8 Development Editor | PASS，exit 0，当前目标无需重新编译；前次完整增量编译也成功 | [最终构建](closure/ue-build.log)、[实际编译](automation/build-final-trial3.log) |
| Backend Release | PASS | [最终构建](backend/build-final.log) |
| Backend full suite | 60 executed / 58 PASS / 2 retained FAIL | [log](closure/backend-tests.log)、[XML](closure/backend-tests.xml) |
| HTTP/WS | 21/21 PASS | [结果](closure/protocol/p4-results.json)、[HTTP](closure/protocol/p4-http.json)、[WS](closure/protocol/p4-ws.json) |
| UE Command | 2/2 Success | [log](automation/closure-command.log)、[report](automation/closure-command/index.json) |
| UE Map + Localization | 2/2 Success | [log](automation/closure-map.log)、[report](automation/closure-map/index.json) |
| UE Video isolated | 1/1 Success | [log](automation/closure-video-isolated.log)、[report](automation/closure-video-isolated/index.json) |
| UE aggregate | 5 Started / 5 Completed Success | [汇总](closure/automation-results.json) |
| Localization | 两文化资源正式生成，字节与上次一致 | [GatherText](closure/gather.log)、[说明/哈希](closure/localization-results.json) |
| Native restart | Backend 精确相等 + 三端真实窗口恢复 | [JSON 比较](closure/restart-comparison.json)、[客户端](closure/N-presence.json)、[进程](closure/N-processes.json) |
| Native copy | 新 ID、rev 1→2、原 deployment-12 哈希相同 | [结果](closure/N-copy-results.json)、[创建](closure/N-copy-created.json)、[修改](closure/N-copy-modified.json) |
| Execution boundary | audited arrays 0 / Assembly starts 0 / Exec starts 0 + isolated call path | [分层证据](closure/deploy-execution-boundary.json) |
| Git protection | main/P1/P2/P3 unchanged | [protected refs](closure/protected-refs.json) |

### Final localization screenshots

| Client | zh-Hans | en |
|---|---|---|
| Command | [中文](closure/command-zh-Hans-final.png) | [English](closure/command-en.png) |
| Map | [中文](closure/map-zh-Hans.png) | [English](closure/map-en.png) |
| Video | [中文](closure/video-zh-Hans.png) | [English](closure/video-en.png) |

业务草稿保护的截图另外保存在 native/K-map-*.png；最终六图用于核验新编译文案和编号，不冒充重新验证脏草稿。

## Historical failing trial — retained

- `protocol/trial1/`、`protocol/p4-trial1.log`：Copy 后残留旧 Mission selection；修复后 21/21。
- `automation/p4-command-live-trial1*`：403 未被识别；结构化 HTTP 错误及客户端状态码兜底已修复。
- `automation/p4-localization-trial1*`：PIE 未加载 game localization / 刷新未完成；已修复。
- `automation/p4-video-trial1*`：Cesium 网络错误，未出现产品断言失败。
- `automation/closure-video*`（不含 `-isolated`）：并发 UE 引起 Cesium SQLite `database is locked`，测试 Result=Fail；没有修改断言/抑制日志，关闭本次原生实例后同测试重跑 Success。
- `localization/gather-*`：保留早期参数引用失败及所有 GatherText 尝试。
- `backend/domain-trial1*`：会话动作错误修改 Plan version，已修复。
- 早期 `build-*-trial1.log` 包括真实 C++ 编译错误，后续修复/通过记录保留。
- `native/F-save-failure-retains-draft.png` 和 `backend/native-errors.stdout.log`：受控持久化故障的预期拒绝，是 draft 保留的验收证据，不应删除。

## Diagnostic result — not a feature PASS

- GatherText **commandlet exit 0**，进程 exit **1**：已有 GameFeatureData/AssetManager 启动错误；完整 stderr/stdout 保留。资源生成步骤明确打印两个 locres 成功。
- UE `WriteFileIfModified(..., BinaryHash)` 保留相同资源的时间戳；mtime 仍为 9 月 15 日，不代表本次没执行。没有人为 touch 时间戳。见 localization-results.json。
- Cesium enum 初始化、GameFeatureData、并发 SQLite cache lock 和短暂 VSM 警告分开记录；不把引擎退出 0 当成测试通过。
- `automation-summary-partial.json` 是中间记录；最终采用 automation-results.json。
- `native/J-review-invalidated.json` 是未成功点击的早期快照，最终采用带 `-final` 的快照。
- 所有 Backend/UE fixture 为 loopback / synthetic；真实相机、Jetson/PX4、飞行不在本次 PASS 范围。

## Raw evidence preservation

日志仅移除了 URL 内 Cesium token 的值，未移除失败、时间戳或结果。原始字节保存在 `Saved/P4QA/raw-evidence/`（Git ignored），[manifest](closure/log-redaction-manifest.json) 记录原 SHA-256 与备份路径。此前本地 commit 未重写；本次未 push。

[文件索引和 SHA-256](closure/evidence-manifest.json) 覆盖归档证据（排除自引用清单自身）。本次测试进程结束后再归档，避免日志继续变化。
