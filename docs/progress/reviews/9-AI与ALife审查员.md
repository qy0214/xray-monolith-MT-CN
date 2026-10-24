# AI与ALife审查员 — run 372f0ebd participant 9

工具预算已耗尽，基于已核实证据交出部分结论。

# src\xrGame AI/ALife 面只读审查（部分完成）

说明：本环境无 shell/git 工具，审查仅通过文件读取与文本搜索完成；工具预算中途耗尽，spawn 系统、怪物战斗（奇美拉/burer/pseudogiant）、level_vertex 限制器、try/catch 空吞扫描、xrServerEntities 存档兼容等项**未及读取，不报臆断发现**。锁语义已核实：`xrSRWLockGuard(lock, bool shared)`，false=独占、true=共享（src/xrCore/xrSyncronize.cpp:134-158）；`xr_task_group` = PPL `concurrency::task_group`，并行执行（src/xrCore/_thread_types.h:18）。

## 发现清单

**[高] 全局空间数据库无锁并发查询 — src/xrEngine/Feel_Vision.cpp:267**
`o_trace` 在 PPL 工作线程调用 `g_SpatialSpace->q_ray(...)`；同时主线程/其他 worker 对同一 `ISpatial_DB` 做 `q_frustum`（Feel_Vision.cpp:106）、insert/remove（对象 spawn/destroy）。`ISpatial_DB` 类无任何锁成员（src/xrCDB/ISpatial.h:247-273），xrCDB 全目录搜索无 mutex。而引擎自身也把 GameThread 跑在同一 task_group（src/xrEngine/device.cpp:498），多 worker 视觉任务天然并行。后果：并发树遍历/遍历中改树 → 崩溃或脏结果。修复方向：给 ISpatial_DB 查询/增删加读写锁，或查询前快照。

**[高] 同一 Vision 实例 `r_spatial` 跨锁竞争 — src/xrEngine/Feel_Vision.cpp:176、267 与 :106**
`o_trace` 只持 `lock_visible`（共享）即写 `r_spatial`（:267 q_ray 输出+遍历），而 `feel_vision_query` 持 `lock_query`（独占）重写 `r_spatial`（:106-127）；两把锁无配对。s2 任务为异步派发（src/xrGame/CustomMonster.cpp:696、src/xrGame/vision_client.cpp:69），下一帧 s1 可与未完成的 s2 任务重叠 → 同一 vector 并发写/读。修复方向：o_trace 全程改持 lock_query，或改用局部结果容器。

**[中] `o_trace` 持共享锁写、主线程持共享锁读 — src/xrEngine/Feel_Vision.cpp:176；src/xrEngine/Feel_Vision.h:69-100**
`o_trace` 在 shared 模式 `lock_visible` 下写 `I->fuzzy / I->Cache / I->cp_LP / I->cp_LAST`；`feel_vision_get`、`feel_vision_get_vispoint` 同样以 shared 锁读 `fuzzy / cp_LAST`（12 字节 Fvector，可撕裂读，影响可见性判定与射击点）。修复方向：写者持独占锁或条目改双缓冲。

**[中] `m_position` 主线程写 / worker 读 — src/xrGame/vision_client.cpp:47 vs :72**
`eye_pp_s01` 主线程写 `m_position`，异步 lambda 经 `this` 于执行时读取（`feel_vision_update(m_position,...)`）；任务排队滞后两拍重叠时为数据竞争。对照 CustomMonster.cpp:696 已按值捕获 `eye_matrix.c`（安全），vision_client 版漏改。修复方向：lambda 按值捕获位置。

**[中] CEnemyManager::useful Lua 缓存无失效路径 — src/xrGame/enemy_manager.cpp:104-117；src/xrGame/enemy_manager.h:33**
`m_useful_cache[entity_alive->ID()]` 缓存回调结果 200ms+jitter，但 `remove_links()` 与 `reload()` 均不清缓存；u16 ID 复用或脚本运行时替换 `useful_callback`（enemy_manager.cpp:19-21）后可命中陈旧结果（含错误的敌我判定，最长约 300ms）。修复方向：remove_links/reload/set_callback 时 `clear()`。另注：该缓存无锁，当前仅主线程调度调用故不构成竞争（已核实 secondary_tasks 在 xrGame 仅用于视觉两处）。

**[低] 静态线程 ID 并发写 — src/xrGame/CustomMonster.cpp:695、src/xrGame/vision_client.cpp:68**
`static DWORD this_thread_id = 0;` 被所有 worker 并发覆盖，仅影响 PROF_THREAD 标注，良性 UB。修复方向：改 `thread_local`。

**[低/待确认] `xr_sparse_map` 已实现未接线 — src/xrCore/_stl_extensions.h:723；src/xrGame/alife_object_registry.h:16-18**
`xr_sparse_map` 定义后在本工作树 xrGame/xrServerEntities 无任何使用点；`CALifeObjectRegistry` 仍为 `xr_map<ALife::_OBJECT_ID, CSE_ALifeDynamicObject*>`。"sparse_map 重做 / server_objects_registry" 未找到对应代码——若在未推送提交中则待确认；就现有代码而言不存在“注册表遍历中插删”风险点（`alife_object_registry.h:17` 另有 `max_id = _OBJECT_ID(-1)` 默认值，语义可疑但未及追踪）。

## 总体健康状况

已覆盖的视觉多线程化是本次改动的高风险面：锁的粒度设计（per-Vision 双锁）思路合理，但 `r_spatial` 的锁配对遗漏、共享锁下写条目和全局空间库无锁这三处足以造成偶发崩溃，属于上线前必修项。CEnemyManager 缓存为纯性能取舍、失效面小，低风险；其余领域因预算未及审查，建议对 spawn/怪物战斗/序列化补一轮同深度抽查。