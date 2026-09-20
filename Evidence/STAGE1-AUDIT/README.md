# STAGE1-AUDIT 证据索引

本目录是 2026-09-20 只读审计的派生证据。没有启动 Backend/UE/Jetson，没有调用飞控/业务写接口，没有执行设备命令。所有代码修改仅限本审计脚本，不涉及产品源代码。

- `baseline.txt`：基线分支/HEAD；最初 status clean。
- `tracked-inventory.txt`：全工程受版本管理文件清单，含二进制资产路径。
- `source-sha256.json`：347个扫描源文件/配置哈希。
- `real-uav-scan.txt`、`coordinate-scan.txt`、`persistence-scan.txt`：当前源码匹配与原始行号，现场IP/令牌脱敏。
- `blueprint-references.txt`：文本侧Blueprint/资产加载引用；不是Blueprint图导出。
- `localization-candidates.txt`：全量候选索引，含已本地化的调用，不能全部当缺陷。
- `http-routes.json`、`http-source.txt`：33个HTTP method/path与完整handler源码行号。
- `ws-types.json`：19下行、4上行合法type；旧mode分支另列。
- `domain-event-and-error-symbols.txt`：业务事件/错误符号候选，不混入WS类型数量。
- `registry-structs.txt`：UE Registry相关完整C++声明。
- `example-*.json`：从已保存P5.2原生final-state提取的真实对象样本；不是本次请求结果。
- `historical-telemetry.txt`：历史日志匹配计数、脱敏片段、原始行号；只证明相应传输/状态事实。
- `history.txt`：真机相关文件历史与已删除UI的取证；不修改历史。
- `counts.json`：接口统计和旧测试XML计数。持久7族、坐标12接口、视频5组、真机5组按正文明确枚举，非自动发现所有潜在插件接口。
- `validation.json`：本轮文档/链接/代码未修改等静态核对结果。

`build_inventory.py` 只读取git/源码/已保存证据，写本目录和 Docs/Stage1/Stage1-Schema-Fields.md；不导入产品代码。`verify_audit.py` 核验本轮文档，不复跑原项目运行测试。

阅读入口：[项目汇报稿](../../Docs/Stage1/Stage1-Presentation-Summary.md)、[13分钟演示稿](../../Docs/Stage1/Stage1-Demo-Runbook.md)、[功能审计](../../Docs/Stage1/Stage1-Feature-Audit.md)。

证据限制：受控Mock/协议/自动化/原生UI/历史设备记录分别标注。字段清单中的观测类型不代替验证器；原生图像仅视觉复核了已有完成和Map执行截图，未生成新截图。没有对Blueprint内部图、外部固件、真实飞行或真实视频做新验收。
