# zwork 审查流（run 372f0ebd）后续处置记录 — 2026-10-24

对 `.zwork/runs/372f0ebd/` 六份域审查报告的逐条处置。原则：小而确定的高危项直接修；
架构级改动（影响帧结构或需要运行时测量）记录结论与前置条件后缓期。

## 已修复（本批）

| 报告 | 发现 | 处置 |
|---|---|---|
| 4-规划师 #1 [高] | ParticlesAsyncManager 无同步 | `IsStarted` 改 `std::atomic<bool>`（acquire/release）；worker 的整个更新段与 `ForceUpdate` 共用一把可重入 `xrCriticalSection`（粒子自身 Update 可能再 Play 出新效果，同线程重入安全） |
| 8-游戏逻辑 #2 [高] | xrSheduler 自注销自旋死锁 | 新增 `m_processing_thread`（atomic，UpdateDeferred 入口登记）；`Unregister` 在同线程调用时跳过自旋，走既有的 Registration 延迟注销路径；跨线程自旋保留 |
| 7-引擎 #2 [高] | 日志器关闭路径竞态 | `bIsAlive` 原子化且关闭在 `logDataGuard` 内置 false（与 SimpleMessage 的检查+SetEvent 互斥）；worker 改 `thread_spawn_ex` 可联接，关闭先 join 再动 writer（消除 mid-record UAF）；`UnpauseLogging` 在同锁内检查 `bLogThreadOpen`（杜绝对已关闭句柄 SetEvent）；关闭幂等（消掉 close+dtor 双重归档产生的 `_1` 重复归档） |
| 9-AI #2 [高] | r_spatial 双锁不配对 | `o_trace` 的 `q_ray` 填充与遍历改入 `lock_query` 专属域（与 `feel_vision_query` 同锁）；锁序保持 visible→query，无反向嵌套 |
| 9-AI #4/#6 [中/低] | 视觉任务捕获成员引用 | vision_client 与 CustomMonster 的 s2 任务改为按值快照（位置 + 透明度阈值）；无意义的跨线程 static thread id 改局部 const |
| 4-规划师 #4/5/6 [中] | 三处吞异常 | script_binder 的 net_Spawn/save、PHCommander、alife_online_offline_group：保持吞掉（mod 兼容策略），补 Msg（save 失败会显式说明存档包不完整） |
| 7-引擎 #5 [中] | ttapi 无边界 | AddWorker 加 VERIFY（槽位上限 + 非空）；RunAllWorkers 加 VERIFY(assigned>0)（0 时索引回绕 (DWORD)-1） |
| 7-引擎 #8 [低] | xrstring dump fopen 未判空 | 判空返回（D: 盘不存在即崩溃的点） |
| 10-卫生 #3 [低] | batch_build 缺平台参数 | 8 处 msbuild 补 `/p:Platform=x64` |

## 调查后不改（有据）

- **7-引擎 #1 [高] shared_str 非原子计数**：上游 `all-in-one-vs2022-wpo-mt` 的 xrstring.h 与我们逐字相同（NonAtomic 是上游设计，非本 fork 引入）。本 fork 历史上试过 Atomic（`4e3bd7ad6`）又被 revert（`9f7799e32`，revert 无原因记录），且 `2c095daf3` 的提交信息说明容器计数与全局字符串容器的静态析构顺序耦合。翻回 Atomic = 重演一次被撤销的实验，且作用于全引擎最热的数据结构。**结论：不动**。未来若要再试，前置条件：搞清 4e3bd7ad6 被 revert 的原始原因、静态析构顺序审计、性能对比。
- **6-规划师 #3 [中] StableOverlaysActive 整帧清史**：DlssIntegration 注释已论证（设备视图/瞄准是屏幕锁定内容，时间累积必然拖影），是设计而非缺陷。审查员自己也标注"待确认"。

## 缓期（架构级，需专项）

| 项 | 缓期原因与前置条件 |
|---|---|
| 8-#1 [高] seqParallel 迁 GameThread 与渲染并发 | 帧结构级改动；回调清单（ALife/子弹/地图/寻路/怪物声音）需逐一审计写集，或迁回 seqParallelBeforRender。需要带着 Optick 捕获做对比测量，不宜盲改 |
| 9-#1 [高] ISpatial_DB 全局无锁 | 全引擎空间查询共用；加读写锁是全局性能点。与本批 r_spatial 修复不同，这要测量帧预算后设计 |
| 7-#3 [中] CalculateBones 与渲染无栅栏 | 需要双缓冲骨骼或帧栅栏，影响渲染提交路径；配合 Optick 数据决策 |
| 7-#4 [中] LocatorAPI m_files 无锁 | 重扫描/卸档窗口与多线程文件打开的读写锁；改错会卡加载，需要加载期压力测试 |
| 4-#2/#3 [中] 粒子 spatial 在 worker / ps_active 遍历窗口 | 前者继承自上游；后者需核实迁移/删除是否严格限定主线程帧相位（审查员自标待确认） |
| 4-#7 [中] 声音 switch_device 窗口期 | 需要设备切换的实机复现环境（拔插音频设备） |
| 6-#2 [中] rt_tempzb 与 u_SceneDepth 拷贝尺寸 | 需要 Desc 对比注入验证；r__svp_debug 抓包可定位 |
| 6-#4/#6 [低] wallmark/Evict 锁粒度 | 性能打磨项，正确性成立 |
| 9-#3 [中] o_trace 共享锁下写条目 | 依赖 9-#1 的空间库方案一并设计（双缓冲条目） |
| 9-#5 [中] CEnemyManager useful 缓存无失效 | 行为级改动（300ms 陈旧容忍是否可接受），需游戏内观察 |
| 4-#8 [低] CollideIntoGroup static contacts | 物理岛并行化之前无风险（当前物理完全串行）；并行化动工时第一颗雷，此处留档 |
