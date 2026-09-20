继续当前 TASK-P4，不要重新开始，不要重新创建分支，不要丢弃、reset、stash 或覆盖当前 P4 worktree 中的任何修改。

先读取当前工作树、Git 状态、最近提交、现有 Evidence 和测试日志，恢复当前执行上下文。

已完成并应优先复用已有结果，不要无理由重复：

* P3 Base HEAD：`f3c56eebffb690c70f7a7699ee8ed78cda8df931`
* P4 独立 branch/worktree 已建立。
* Backend baseline build PASS。
* P3 HTTP/WS baseline 13/13 PASS。
* P3 UE core baseline 3/3 PASS。
* P4 已实现：

  * `content_revision`
  * Validation revision
  * Review record
  * immutable Deployment Snapshot
  * Copy as Draft
  * Route Edit Session / lease
  * Dirty Draft protection
  * Command 负责 Plan/Mission/UAV
  * Map 负责 Route
  * Backend UI Preference Context
  * zh-Hans / en localization
  * structured Plan event localization
  * explicit Video Target
  * Active UAV / Video Target 解耦
* P4 HTTP/WS 最终已达到 21/21 PASS。
* Backend 全量测试当前为 60 项：58 PASS，2 FAIL；失败仍然只是历史：

  * `SegmentDistanceTest.Crossing`
  * `AssemblyPlannerTest.ConflictHeightSeparation`
* Map 保存失败保护已经真实验证：

  * Backend 保存失败后 editor 保留；
  * waypoint 保留；
  * Route Actor 保留；
  * Undo stack 保留；
  * 成功 ACK 后才退出编辑。
* Command 原生 CRUD 已验证。
* 三个 Mission 已分别分配 UAV-01 / UAV-02 / UAV-03。
* 三条 Route 已保存。
* Validation 已达到 READY 3/3。
* Review 已验证。
* Review invalidation 已验证：内容 revision 10 → 11 后旧 Review 清除，重新 Validation + Review 后 Deploy 恢复。
* Video 原生独立性已验证：

  * 明确打开 UAV-01；
  * Command 选择 UAV-02；
  * Video 保持 UAV-01；
  * 明确调取 UAV-02 后才切换。
* 三端中英文同步已经原生验证，并验证 Map 未保存 3-waypoint Draft 在切换语言期间保持。
* 原生 Deploy 已完成：

  * Plan DEPLOYED；
  * 三个 Mission DEPLOYED；
  * 显示“已部署，执行尚未启动”；
  * 创建 `deployment-12`。
* Backend restart 后 Plan、Mission、Deployment Snapshot、language、video target、active selection 已逐字段一致。

现在只做剩余收口，不要重新设计 P4。

## 1. 先检查当前工作树

记录：

```text
branch
HEAD
git status
git diff --stat
recent commits
```

不得丢弃任何未提交修改。

## 2. 修复当前已知最终 UI 问题

原生语言验收截图已发现：

1. 部分 selected button 存在中英文混排；
2. Video 的 UAV 编号存在补零不一致。

只修正这些明确问题及由它们直接引起的 Localization 显示问题。

不要扩大 UI 重构范围。

## 3. 最终 Localization Build

重新运行正式 GatherText / Localization build。

确认：

```text
en .locres
zh-Hans .locres
```

均存在且是最新生成。

重新构建：

```text
UE5.8 Development Editor
```

必须 PASS。

## 4. 最终定向回归

至少重新运行受最终 UI/Localization 修改影响的：

```text
P4.Localization.*
P4.VideoTarget.*
P4.Workflow.*
```

确认 Test Started / Test Completed。

Cesium 网络错误如再次出现，必须与产品 assertion 分开记录；不得把网络 failure 隐藏，也不得把一次 Cesium failure 当作产品功能失败。

重新确认：

```text
Backend full suite = 58/60，仅历史两项失败
P4 HTTP/WS = 21/21 PASS
```

## 5. 完成 Native Acceptance N

必须补完真正的三客户端恢复。

重启：

```text
Backend
Command
Map
Video
```

确认恢复：

```text
DEPLOYED plan
3 missions
deployment-12
current language
video target
active UAV
active Plan/Mission
```

不能只验证 Backend JSON。

需要真实 Command / Map / Video 客户端证据。

## 6. 原生验证 Copy as Draft

在重启后的 Command：

```text
Copy as Draft
```

确认：

* 新 Plan ID；
* 新 Mission IDs；
* 新 Route IDs；
* status = DRAFT；
* content_revision 正确；
* review 不继承；
* deployment 不继承；
* source_plan_id/source_deployment_id 正确。

随后修改新 Draft 的任意安全业务字段。

再次读取原 `deployment-12` Snapshot。

必须证明：

```text
deployment-12 before == deployment-12 after
```

原部署内容完全不变。

## 7. 最终确认 Deploy 没有进入执行链路

检查 Evidence / Backend instrumentation。

必须证明本次 Security Plan Deploy：

```text
/api/arrays calls = 0
AssemblyController = 未触发
ExecutionEngine = 未因 P4 Deploy 启动
PX4 / MAVLink = 未由 P4 Deploy 调用
```

## 8. 收口 Native A–N

生成正式表格：

```text
A Startup
B Create Plan
C Mission Organization
D Map Edit Request
E Dirty Draft
F Save Failure
G Save Success
H Validation
I Review
J Review Invalidation
K Localization
L Video Target
M Deploy
N Restart / Copy
```

每项必须标：

```text
PASS
FAIL
NOT EVALUABLE
```

并链接对应 Evidence。

不要把协议测试替代原生验收。

## 9. Evidence 收口

更新：

```text
Evidence/TASK-P4/README.md
```

区分：

```text
historical failing trial
diagnostic result
selected final result
```

保留此前所有失败证据。

不要删除：

* 首轮 HTTP/WS Copy selection failure；
* GatherText 参数失败；
* Cesium network failure trial；
* Backend persistence fault trial；
* 其他真实失败记录。

## 10. Git 分阶段提交

检查已有 commits。

如果尚有未提交修改，按实际变更内容生成合理提交，不要制造一个巨大 commit。

目标阶段至少能够区分：

```text
Backend/domain
Workflow UI
Route draft protection
Localization
Video target
Tests
Evidence/docs
```

不得修改：

```text
main
P1
P2
P3
```

不得 merge。

## 11. 最终报告

完成：

```text
Docs/TASK-P4-SecurityPlanWorkflowLocalization.md
```

报告必须包含：

* Final Result
* Branch
* Base HEAD
* Final HEAD
* Architecture
* Role Matrix
* content_revision
* Validation
* Review
* Deployment Snapshot
* Route Edit Sessions
* Command workflow
* Map workflow
* Localization
* Event localization
* Video Target
* Persistence/restart
* Backend tests
* HTTP/WS tests
* UE automation
* Native A–N
* Known Issues
* Retained Issues
* Deferred
* Evidence Index
* Git checks

保留历史两项 planner failure。

P2/P3 retained issues如果没有在 P4 明确修复并重新验收，继续保留。

## 12. 最终交付前 Git Gate

最终执行：

```text
git status
git rev-parse HEAD
git log --oneline --decorate
```

确认：

```text
P4 working tree clean
main unchanged
P1 unchanged
P2 unchanged
P3 unchanged
no merge
```

最终回复只在全部工作完成后给出：

```text
Final Result
Branch
Base HEAD
Final HEAD
git status
Backend tests
HTTP/WS result
UE automation result
Native A–N result
Localization result
Video Target result
Retained Issues
Evidence path
Report path
main modified? yes/no
P3 modified? yes/no
merge? yes/no
```

继续执行直到 TASK-P4 正式收口。不要重新实现已经完成并有证据的部分。
