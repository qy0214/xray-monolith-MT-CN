# 项目状态总台账（STATUS）

> **本文件是所有审查发现与遗留事项的唯一权威索引。** 记忆与进度文件已全部统一到 `docs/progress/`：
> - `plans/` —— 三份实施计划（DLSS/FSR3/PiP 移植、Tier2/3 性能、会话检查点）
> - `reviews/` —— zwork run 372f0ebd 六份域审查原始报告
> - `notes/` —— workbuddy 会话笔记
> - `zwork-followup.md` —— zwork 审查的处置明细（已修/调查不改/缓期+前置条件）
>
> 更新约定：任何新修复/新发现/状态变化，更新本文件对应行；细节写进对应明细文档。
> 最后更新：2026-10-24（上游全景对比之后）。

## 0. 批次与提交索引（origin 已同步至 `bedb0c87c`；本地另有 2 个文档提交未推）

| 批次 | 提交 | 内容 |
|---|---|---|
| 阶段 0/1（10-24 晨） | `f8a6d424` `11d9b4ce` `983b8efc` `748e6312` `4ae75fdc` `04c9bb3a` `3943bd1b` | P1-1 别名同步修复+映射表+测试；P1-2 短期接缝+游戏侧迁移；P2-1..P2-4/P2-6/P3-⑥ 小修；CI 接测试；文档；构建脚本入库 |
| 阶段 2（10-24 午前） | `4fcd764a` `8bc85544` `f8b483fd8` | Xr_ini DLTX 拆分；HDR10 评估落档；瞄准日志归因 |
| 阶段 3（10-24 午后） | `058c5d26d` `4798105d6` `b9b7e4306` `a8840c4bd` | 上游实测合并；同步方向决策表测试；上游策略文档；P1-3 定论 |
| HDR10（10-24 晚） | `b172560be` `90b20ebc7` | HDR10 同帧 PiP 修复 + 真机验证通过 |
| zwork 批次一（10-24 深夜） | `a1325cec4` `8e38ece5a` | 四个高危并发修复 + 吞异常补日志 + 卫生包 + 处置明细文档 |
| 记忆统一（10-24 末） | （本提交） | 全部记忆/进度文件收拢到 docs/progress/ + 本台账 |

## 1. 第一轮审查（用户出示的 P1/P2/P3 报告，2026-10-23 核验）

| 项 | 状态 | 说明 |
|---|---|---|
| **P1-1** 首帧别名同步冲掉原生上采样配置 | ✅ 已修复 + **真机验证** | `CCC_TokenConfigured` 锁存 + `upscaler_sync_direction` 纯函数决策表（8 用例）。真机：native-only user.ltx 下 DLSS 存活且 alias 正确回写 |
| **P1-2** 镜头遍改写 Device 全局与游戏线程竞争 | ✅ 短期已修复（构建验证）／⏸️ 长期缓期 | 短期：`Device.ScreenWidth/Height()` 主集接缝 + xrGame 12 处迁移。长期（镜头遍不碰全局）需运行时验证后动 |
| **P1-3** live_effects 恒 0 | ✅ 定论关闭（已文档化） | 读 mod 着色器源码证实**结构性不可达**：刷新路径在时间复用分支内，本引擎同帧设计 `motion.x=0` 恒不进入 |
| P2-1 判空不一致 | ✅ 已修复 | 热成像诊断块两级判空，缺 game 时打 -1 |
| P2-2 探针下标回绕 | ✅ 已修复（机制更正：除零而非越界读） | <32px RT 拒绝 |
| P2-3 FDDA 健康值越界 | ✅ 已修复 | clamp 0..1；外骨骼屏实机确认仍待用户顺带观察 |
| P2-4 日志归档同秒冲突 | ✅ 已修复 + **真机生效** | 毫秒 + 冲突计数 + 返回值检查；后续修复又根除了 close+dtor 双重归档（`_1` 副本不再产生） |
| P2-5 双 token 顺序手工映射 | ✅ 已修复 | `UpscalerAliasMap.h` 单表 + static_assert + 单测；质量 token 有意不纳入（单调无换位风险） |
| P2-6 SetActive"死分支" | ✅ 已按更正处置（非死代码，注释保留） | 降级模式每帧可达 |
| P3-① lens_rendered 原子化 | ✅ 评估后不改 | atomic 破坏纯函数/可测试设计，单 bool 跨线程 x86 无实际撕裂 |
| P3-② ads_is_pip 晚一帧 | ✅ 已文档化 | README 已知问题 |
| P3-③ submodule 文档 | ✅ 早已存在（报告过时） | — |
| P3-④ 零帧 aim ended | ✅ 已修复（真因与报告不同） | 归因到 aimSect + 切换时收尾行；真机日志可见生效 |
| P3-⑤ svp_dump_rt Inf/NaN | ✅ 接受现状 | 诊断用途 |
| P3-⑥ bloom 尾换行 | ✅ 已修复 | — |

## 2. 第二轮质量评估（4.x 报告，2026-10-24 核验）

| 项 | 状态 | 说明 |
|---|---|---|
| 超大文件（imgui 1.4MB 等） | ⏸️ imgui 阻断／其余有意不做 | imgui 资源化被环境策略拦在二进制提取（手动恢复四步见会话记录）；Weapon/console_commands 拆分因上游合并策略**有意不做** |
| 测试覆盖不足 | ✅ CI 已接（`unit-tests-job` 且 gate release） | 4 个测试文件 89 项检查；XR_ini 拆分附等价性验证 |
| Borland 残留 | ✅ 已清理（更正后） | 仅删空体 cpp；FileSystem_borland.cpp 与头文件是活代码已注明 |
| .cppX/.hX 机制 | ✅ 澄清不存在 | 报告有误 |
| C++ 标准偏旧 | ✅ 澄清（v143/stdcpp17） | 是风格问题非标准问题 |
| CI/CD 建议 | ✅ 已存在，补了测试 | 静态分析仍未做（可选） |
| Discord/ICU 静态链接 | ✅ 澄清（动态，DLL 随 sdk/binaries） | 供应链关注点=钉住的旧版运行时 |
| LuaJIT 2.1/64 位 | ⏸️ 长期（依赖整引擎 x64） | 非独立可做项 |
| 网络协议审计 | ⏸️ 降级为已知边界 | 单机场景为主 |

## 3. zwork 六域审查（run 372f0ebd）—— 明细见 `zwork-followup.md`，原始报告 `reviews/`

| 类别 | 项 |
|---|---|
| ✅ 已修复（本批） | 粒子同步[高]、调度器自注销死锁[高]、日志器关闭路径[高]、r_spatial 锁配对[高]、视觉任务按值快照[中低]、三处吞异常补日志[中]、ttapi/xrstring/batch_build 卫生[低中] |
| 🔍 调查后不改（有据） | shared_str 非原子计数（上游同款+本 fork 试过又 revert+析构顺序耦合）；StableOverlaysActive 整帧清史（设计如此） |
| ⏸️ 缓期（前置条件在档） | seqParallel 并发模型[高]、ISpatial_DB 全局锁[高]、骨骼栅栏、LocatorAPI 锁、声音切换窗口、粒子 spatial/ps_active、rt_tempzb 拷贝尺寸、wallmark/Evict 锁粒度、o_trace 共享锁写条目、CEnemyManager 缓存失效、CollideIntoGroup 静态缓冲 |

## 4. checkpoint / 计划文档遗留与用户决定项

| 项 | 状态 |
|---|---|
| 热成像光圈伪影（README 五-1） | ❌ 开放——诊断流程在档（`r__svp_debug 2` + 抓 pip_optics_screen.bmp），需实机 |
| 政策文件 scope_lense_enabled 解析（README 五-2） | ❌ 开放——需 token→段名映射规则 |
| MT 读档崩溃率（README 五/上游已知） | ❌ 开放——zwork 批次一的修复可能改善，待长时游玩观察 |
| FDDA 外骨骼屏实机确认（时钟/血条编码） | ⏸️ 待用户顺带验证（引擎侧已按推断实现+clamp） |
| Tier-1 A/B 性能数据 | ⏸️ 待用户提供（FPS + uSheduler_L + queue 行） |
| lua_parallel_gc 等 inert MCM 项 | ⏸️ 按需（用户要用 pack 自带性能模块时再做） |
| T3 #9a 并行 RT 创建 | ❌ 有实测数据判定不值得做（串行 1042ms vs 并行 1721ms） |
| imgui 字体资源化 | ⏸️ 阻断——用户手动跑提取脚本后可续（四步在档） |
| **推送本地提交** | ✅ 代码已同步（origin=`bedb0c87c`）；本次 2 个文档提交待推 |
| dist/MT-CN/appdata/user.ltx 测试残留（r4_upscaler 行已删） | ⏸️ 待用户决定恢复或入库 |
| sections.ini（仓库根，本地已 exclude） | ⏸️ 待用户决定入库/移动/忽略 |

## 5. 验证与部署状态

**真机已验证**（2026-10-24，日志在 `appdata\logs\`）：
- P1-1（native-only 配置 DLSS 存活 + alias 回写）；
- HDR10 同帧 PiP（`r4_hdr10_on 1` 下目标集创建、lens=1、零拒建）；
- SDR 零回归（对照局干净）；
- P2-4 归档毫秒时间戳；P3-④ 瞄准行归因；P2-1 诊断行正常。

**待验证**：zwork 批次一（长时游玩：粒子密集+多怪视觉+存读档；观察无新崩溃、无 `_1` 双归档、留意 `[script_binder]`/`[PHCommander]`/`[alife]` 行）。

**当前部署**（`MODS\MT-CN\bin\AnomalyDX11AVX.exe`，校验清单 26 条已同步）：
- 构建：DX11-AVX，2026-10-24，`exeSize=28665856`
- SHA256：`8A0171A94EE06EB26A1B71D6E5D9B47241545A9271EC4F0AF4448128070570DC`
- 测试：89 项检查全过；CI：unit-tests-job 已 gate release

**上游同步**：remote `upstream`=themrdemonized/xray-monolith，血缘分支 `all-in-one-vs2022-wpo-mt`；已实测合并一次（零冲突）；策略=每批次开始 `fetch+merge`，不做重度拆分（详见 README 构建章节）。

## 6. Mimosa 全库深度安全扫描（2026-10-24）

- Scan ID `scan-2026-10-24T04-05-45.363Z-3b8e5ec0f968`，seal `sha256:6534d554…7e689`，产物目录 `C:\Users\Admin\.mimosa\security-scans\project-5a70021b0a0479efe6a16a4d\scan-…-3b8e5ec0f968`；深度 deep，静态边界（无运行时验证），6934/6934 文件解析。
- 运行状态 **inconclusive**：调用图部分不完整（动态派发/规模），跨文件可达性可能不全；verdictEffect=none（不做安全/不安全的裁决）。
- 依赖扫描：not_applicable（0 包）。
- **Findings：5 条，全部 HIGH「命令注入」，且全部位于 `src/3rd party/optick-git/gui/Optick/*.cs`（vendored Optick 采样器 GUI 工具，C# 开发者侧）**：EditTaskTrackerListDialog.xaml.cs:77、FrameCapture.xaml.cs:195、TaskTracker.cs:101/122、SummaryViewerModel.cs:165——均为"用户输入拼接进系统命令"模式（开发者打开恶意 capture 文件的攻击面）。**引擎本体（src/ 引擎代码）0 条发现。**
- 处置：vendored 第三方 GUI 工具，不随玩家 exe 分发（引擎仅静态链接 optick 采样核心）；按供应链项记录在案，暂不改第三方源（改了会被 submodule/vendor 更新冲掉）。若未来分发 Optick GUI 给第三方使用，需先评估这五处。

## 7. 游戏安装+MOD 全量审查（2026-10-24，明细 `anthology-audit-2026-10-24.md`）

- **cvar 契约零缺口**：169 个 MCM 脚本引用的 19 个引擎命令全部已注册（比对工具链假象已记录在案）。
- **MT-CN 载荷 25/25 完整**；⚠️ **游戏挂载的仍是 HDR10 修复版 exe，zwork 批次一未上机**——待用户 JSGME 重挂（停用→启用→清着色器缓存）。
- ⚠️ Standart profile 双 3DSS 变体同时启用（PiP 实际生效，但依赖 MO2 行序，建议禁用普通变体）；HARD profile 无 PiP（引擎按设计降级，需知情）。
- ⚠️ checkpoint 所载三个 `_engine_backup_*` 目录已不在磁盘；现行回退=JSGME !BACKUP（在位）+ git + MT-CN.rar。

> **2026-10-24 12:21 更新**：玩家已重新挂载 JSGME——当前运行会话 exeSize=28665856（zwork 批次一已上机，SDR，0 错误）。第 7 节"挂载版本落后"项已解除。另按玩家指示，user.ltx 基线已改为 `r4_hdr10_on 1` 并恢复 `r4_upscaler dlss`（`735d194b7`，载荷已同步）——下次 JSGME 重挂即默认 HDR10 + 同帧 PiP。

## 8. 上游 v0.2.2（主线发布标签）对比（2026-10-24）

- 血缘：与我们于 `1132a3f68`（2025-12-30）分叉；`v0.2.2`=主线 2026-01-11 发布（含我们没有的 422 提交/342 动 src，+85K/−33K 行、490 文件）；我们的 `-mt` 线含其没有的 3334+ 提交（wpo/-mt 十个月演进 + 我们的适配层）。**整线合并不做**（与其 delta 重叠我们近批 49 文件中的 19 个，含全部渲染热点）。
- **高价值 cherry-pick 候选**（多数零冲突）：`6a39149e2` 读档后 Inventory::DropItem 空指针崩溃（正对"MT 读档崩溃率"已知项）；`a60a10b9d`+`755b95238` wallmark 失效崩溃防护（zwork 缓期项相关）；`c5d151c70` 纹理加载竞态；`90ac23f4e` 物品销毁/插入互斥；`3172ccb1d` PDA 地图缓存治卡顿（碰 UIMap.cpp，小冲突）；`5f0f6b701` Discord 移出主线程（碰 device.cpp/Actor.cpp，中度冲突——关联我们 12:30 局在 Discord 销毁行突然消失的现象）。
- 观察项（不搬）：真 PiP 阴影 WIP、草渲染滚动缓冲（已 revert）、视觉优化做了又部分撤销（印证 9-AI 审查的复杂性判断）、AI CPU 6.5→1.5ms 与 PDA 缓存值得后续评估搬运。

## 9. v0.2.2 cherry-pick 批次（2026-10-25）

六个上游修复全部搬运完成（`00c4c0897`…`19b19090a` + 适配 `627258e04`）：①读档后 DropItem 空指针（保留我方精细抛掷）②wallmark 崩溃防护两连（script 共享串 + 失效 Parent 守卫 + sampler 空态）③纹理加载竞态 ④物品池互斥（DetailManager.h 补 pool_mutex 声明）⑤PDA 地图缓存（我方 luabind 回调保留 + AddToCache 追加）⑥Discord RPC 移出主线程（rpc_info 数组化 + 互斥锁 + 专用 std::thread + 退出 join——不再在 pApp 销毁后跑回调，对准 12:30 无栈中断现象）。适配要点：不 spawn 上游通用 mt_Thread（我们的 MT 模型是 secondary_tasks/GameThread）。DX11-AVX 0 错误 + 89 测试全过，已部署（exe sha256=4808FD08…，28,665,688→28,665,888B 校准见清单），待真机验证。

## 10. 上游全景对比（2026-10-24 复核，明细 `upstream-survey-2026-10-24.md`）

- **同步状态**：`HEAD` 已包含 `upstream/all-in-one-vs2022-wpo-mt`（10-21）与 `upstream/all-in-one-vs2022-wpo`（10-11）全部提交；`2026.10.11` 标签在线内；origin 已同步。27 分支里 **12 个 ahead=0**（SSS 系列/BaS_NEW/master/153-merge/hozar2002/wpo/wpo-mt 等），其余为旧主题线或实验线。
- **唯一待并线 `upstream/pr`**（10-22，33 提交 / 21 真实）：Batch A 可低风险搬 5 项——GraviZone `exit_Zone`（悬浮物，`0bd86fc17`）、Phantom 出生期受击（只取 `427b6981d` 的最终 Hit 一行）、UI 滚动条两连（`7b6d5549d`/`15d42e813`）、纹理 `GetDeviceRemovedReason` 日志（`801b939f0`，需保留我方 `texture_gpu_lock`）；可选 `net_Relevant`（`4f7fa5b4e`）。功能项（env modifiers、加载 artwork、怪物目的地 API、camattach）暂不搬；camattach 已有 MT 版（PR #773）。`1ba7dc959`（PDA 排名 Lua，源文件已入库、补丁干净；生效需重打 `00_modded_exes_gamedata.db0`）。
- **性能主项**：草/细节实例化 **PR #577**（MT 版，10 文件 +485/−144）/ #567 未合并；`grass-instancing-2` 为实验分支（10-11 后未同步）。需配套 5 个 grass shader，且与我们的 `deffer_grass.vs`（5710B 旧常量数组版 vs PR 7592B instance 版）及 `dx10DetailManager_VS.cpp` 冲突。建议独立 worktree 做 A/B，≥15% GPU 收益且无闪烁才考虑发布。
- **纠正（勿搬）**：`grass-instancing-2` 里的 visual-memory 判空基于 `m_objectsShared`，该实现已被 `1b525d33a` revert；`cNameVisual` 自旋（`47583e80e`）已被 `1c0c5b5a0` 以“修 eye_pp_s2 崩溃”为由 revert；`CPHSimpleCharacter`/`CAgentManager` 判空在我们 HEAD 里**已经存在**。这四个“小防护”不是净收益，**禁止 cherry-pick**。
- **两个小而值（开放 PR）**：**#777**（`xrsharedmem` → `xr_unordered_map`，加载期 O(1)，merge-tree 干净）、**#765**（雨湿度按 `fTimeDelta`，修高 FPS 干燥快 2.4×，顺带 `level.set_rain_wetness`）。
- **不要碰**：`cmake`（2025-08 的平行构建线，713 提交）、`65534-loop-banish`（正则改写 mod 脚本、默认开启）、`cform_cache`（纯 revert）、旧 SSS/BaS/vs2022 线、`ubgl-grenade-class`（已检索整合包，无 `AddAmmoClass` 调用）。PR #368 是 `ui_options.script` 的本地化 MAV monkey-patch（draft），跑中文包前先复现 #367 再决定。
- **部署提醒**：`MODS\MT-CN\bin` 已是 4808FD08（28.6MB），但游戏 `bin` 仍是 10-03 的 48MB exe / 旧 DLSS DLL；13:13 日志证明当时跑的是 MT 版，13:15 后 JSGME 已停用/还原，新构建尚未重挂。下次实机前停用→启用并清着色器缓存。

> **§10 独立复核（2026-10-25）**：对 survey 全部论断逐项实证，全部成立。决定性证据：`47583e80e` 的补丁内容就是把 `m_p_tasks_count` 自旋等待加回 `cNameVisual_set`/`net_Destroy`——正是 `1c0c5b5a0`（"revert changes to fix eye_pp_s2 crash"，在祖先链内）移除的代码；`38efb138e` 对我方树直接 apply 失败（VMM 已分叉）；`CAgentManager` 我方 ：48 的守卫比上游补丁更严（多 memoryPtr 判空）。Batch A 五项 dry-run：四项零冲突，`a2021988e` 在 x_ray.cpp:330 失败（与其"低优先"结论一致）。PR #777 对我方树 apply 干净、基线 -mt 成立。仓库面实证：modded-exes 非 fork、最后推送 2024-09-29、~634MB；ixray-1.6-stcop 是 ixray-team 同名库的 fork（不同代码库）。部署提醒再次确认：**游戏 `bin` 当前是 48,067,584B 原版 exe（JSGME 已停用），4808FD08 尚未挂载**。复核结论：survey 可作为批次 A/B/C 的执行依据。

> **批次 A+B 执行完毕（2026-10-25）**：A=6 提交（GraviZone `exit_Zone` 治悬浮物 `374947615`、UI 滚动条/列表两连 `d0c52dd1b`/`8c0765647`、azazel `net_Relevant` `74fcc33fe`、幻影出生期受击+纹理设备移除日志 `78493c8b2`）；B=2 提交（雨湿度按帧时间 `45498135f`、sharedmem 哈希 `58b9960da`）。DX11-AVX 0 错误 + 89 测试全过，已部署（exe sha256=71CB423D…）并推送。**待真机**：悬浮物落体、幻影可被击杀、PDA/背包 UI 拖拽手感、读档（azazel）、高帧率下雨后地面干燥速度、加载耗时（#777 需冷/热各一次记录）。

> **批次 C 就绪（2026-10-25）**：实验 worktree `C:\game\xray-monolith-mt-grass`（分支 `exp/grass-instancing`，已推 origin）＝主线 HEAD + PR #577 引擎合并（唯一冲突 dx10DetailManager_VS.cpp 已解，采用上游实例化架构）+ PR 自带 r3 shader（as-authored 暂存）+ A/B 协议（EXPERIMENT.md）。DX11-AVX 构建 0 错误。**未进发布载荷**。A/B 要点：PR shader 丢我方 MT/DLSS 草运动矢量（DLSS 下草会拖影=预期现象）；判定线=同机位 GPU 时间 ≥15% 下降且无闪烁；通过才谈生产化（需把实例化流与我方 prev-position 逻辑融合）。

> **镜内假影根因修复（2026-10-25，ad1c786d2）**：玩家截图定位——举镜时镜内出现放射状假树影、随视角走、随变焦加剧（腰射正常，太阳盘不动）。根因：同帧 PiP 下 SSS 屏幕空间阴影缓冲只为主遍渲染（镜头遍被 `m_render_pass_is_svp` 守卫跳过），但镜头遍的光照积累仍采样 `rt_ssfx_sss`——拿到的是**主视空间的陈旧阴影数据**，套镜头坐标即视角绑定假影，变焦越高错位越大。修复：镜头遍将 `rt_ssfx_sss`/`rt_ssfx_sss_tmp` 清为中性（1=无影）再采样，主遍照常。DX11-AVX 0 错误+89 测试，已部署（exe sha256=4BA2CC8C…）并推送。**待真机**：举镜满倍率复查镜内树影是否恢复正常；M82A2 满倍率"准星变粗"若仍存在则为独立问题（准星是 HUD/镜内分划，不受 SSS 影响——大概率是对比度错觉，修复后应一并消失）。

> **腰射光影根因修复（2026-10-25，ad40ac3e8）**：玩家 S2 实测（`r4_upscaler off` → 腰射假影消失）锁定 DLSS 域；S3（jitter 0）读数为 DLSS 无抖动的预期退化（变暗+方形块=无效配置），S4（关 TAA）无效。根因：太阳级联矩阵的对齐粒度为 **4 纹素**（R_sun.cpp），转身时级联矩阵按 4 纹素一跳连续滑动，DLSS 时间重建把这些跳变累积成"绑定视角方向"的大块漂移树影（腰射也有）。修复：对齐粒度 4→1 纹素（游泳幅度降 4 倍，R2/R3/R4 共用）。此前 `ad1c786d2` 的 SSS 镜头遍清空修复解决的是**举镜**时的同域问题（镜内采样到主视空间陈旧 SSS 数据）。两个修复互补。已部署（exe sha256=23033BB1…，28,662,272B）。**待真机**：同机位转身观察腰射+举镜的树影漂移是否消失/显著减轻；注意载荷 user.ltx 仍是 detail_density 0.4（玩家实测手改回 0.72 未同步——待定夺）。

> **"第二光源"根因修复（2026-10-25，5e32c09a8）**：玩家实测定位——假树影的方向绑定视角（前进/后退不动，左右/转视角变化），与太阳级联影同时存在（"两套光源"）。根因：`cl_sun0_dir_e`（SSS 屏幕空间树影的光线方向常量，视空间太阳方向）按帧缓存，而同帧 PiP 每帧有镜头/主两个相机——镜头遍先绑定（镜头视空间值被缓存），主遍复用 → 主视口的 SSS 树影方向被绑死在瞄准方向。修复：去掉按帧缓存，每次绑定重算（一次矩阵变换）。同时段验证：S2（关上采样）消除腰射病灶=DLSS 域确认；S3（jitter 0）读数为 DLSS 无抖动的预期退化；S4（关 TAA）无效；camera_mv 0 无效且触发一次卡住（另行观察）。已部署（exe 需重挂后验证）并推送。**待真机**：腰射+举镜转身，"第二光源"应消失；卡住问题若复现需单独抓取。

> **雾平面视锁定修复（2026-10-25，d64961f17）**：玩家报告"两套光源"且 upscaler off 下依旧（20:35 会话，r4_upscaler off 实测）——推翻 DLSS 域假设，锁定原生渲染路径的视相关按帧缓存绑定器。全场审计发现两处：`cl_sun0_dir_e`（视空间太阳方向，已修 5e32c09a8）与 **`cl_fog_plane`（雾平面派生自当前相机 mFullTransform，本次修复）**。镜头遍先绑定 → 主遍复用镜头相机的雾平面 → 主视口距离雾分布锚定视角（大片明暗异常/第二光源光晕）。修复=去掉按帧缓存、每次绑定重算（其余 6 个按帧缓存绑定器均为视无关值，已逐一核实）。DX11-AVX 0 错误+89 测试，已部署（exe sha256=CE60D451…）并推送。**待真机**：转身+左右平移观察"第二光源"光晕与异常暗带是否消失。另：卡住问题（camera_mv 0 时）未复现未诊断；detail_density 基线待用户定夺（0.4 vs 实测 0.72）。

> **光影问题二分阶段（2026-10-25 晚）**：雾平面修复（d64961f17）后玩家实测**依旧**——四项假设修复（SSS/级联/L_sun_dir_e/雾平面）全部未命中。同时玩家实测**关闭 DLSS 也无效**（推翻此前 S2 的"DLSS 域"结论——该结论作废）。转入**二分定位**：好→坏窗口修正为 [E0 原版整合包引擎（10-03 时代，无 DLSS/PiP）→ 当前 HEAD]；玩家 10-19 构建（608625628，我 30 提交之前）从未被测试=决定性分界点。已构建 10-19 测试版（`MODS\MT-CN\bin\AnomalyDX11AVX_test_1019.exe`，sha256=50441D3D…，与正式 exe 并排、未覆盖）。**测试流程**：把正式 exe 改名 → test_1019 改名为正式名 → 清缓存 → 同机位观察。若 10-19 已坏=MT 分支自身演进引入（DLSS 移植 9dc5a5b5 为头号嫌疑，向 10-03→10-19 继续二分）；若 10-19 正常=我的 30 提交引入（按提交组二分）。另：camera_mv 0 会话卡住一次（未复现）；detail_density 载荷 0.4/玩家实测 0.72 待定。

> **"第二光源"根因修复（2026-10-25 深夜，e318abb95）**：10-19 基线测试（玩家实测复现）把窗口收敛到 MT 分支自身演进（DLSS/PiP 移植期）；玩家形态描述（"点状放射圆形四射光线、自行旋转"的 omni 屏幕空间影）+ S2 反转（关 DLSS 无效）+ 腰射/镜内影子同步 → 定位到 **`phase_ssfx_sss_ext` 的静态光源槽填充门控**（`sss_currentframe = dwFrame + 2`，每 2 帧才刷新）。同帧 PiP 下：镜头遍先填充（镜头视图的阴影光源集），主遍跳过刷新复用 → 主视口 omni 影子按镜头视图光源集计算 = 视绑定的"第二光源"（与上采样器无关，两视图影子同步一致）。修复：每次调用（每视图）都刷新，槽位稳定性由既有匹配逻辑保持，CheckPackage 门控随 else 分支一并移除。DX11-AVX 0 错误+89 测试，已部署（exe sha256=43769B23…）并推送。**待真机**：腰射+举镜转身，"第二光源"（放射光线光晕+错误树影）应消失。同源修复合计三件：SSS 镜头遍清空（ad1c786d2）、L_sun_dir_e 按绑定重算（5e32c09a8）、本件（e318abb95）——全部是"同帧双相机 × 按帧/跨帧缓存的渲染状态"这一类缺陷的不同表现。

> **SSS omni 光源槽诊断转储（2026-10-25 深夜，ebb5537a2）**：四个假设修复全部无效 + 玩家关 DLSS 也无效 + 10-19 基线也复现 → 静态分析穷尽，转为数据驱动。新增 `r__sss_dump_lights` 诊断 cvar（0/1）：开启后每秒转储一次 SSS-ext 的 8 个光源槽（世界/视空间坐标、范围、类型、相机位置）——"第二光源"的 omni 阴影射线从槽内光源的视空间位置放射，dump 的世界坐标可直接对照地图上的灯具/电筒/月亮定位错误光源的真身。已部署（exe sha256=AF2B5A7A…，28,662,272B）并推送。**待玩家**：正常游玩至异常出现 → `r__sss_dump_lights 1`（控制台）→ 异常明显时等几秒 → 发日志；dump 数据回来后按光源世界坐标定位真身并修复。

> **影子方向再归因 + 视角绑定待测（2026-10-26 凌晨）**：屏幕空间层全关 + 冻风后地面暗斑仍在 → 是 CSM 树冠影，不是叠加层；静态相机 8 s 对比证明"会动"的部分是风动枝叶影。玩家提出新假设：**错位影子可能绑定在角色视角上**。02:31 dump 拿到引擎相机 `(-1.000,-0.009,-0.023)` 与 `sun_dir=(-0.656,-0.639,0.402)`：太阳在相机**背后偏左**（仰角 40°、离视线 130°）；对 02:14 视图那条细长直影，引擎预测地面影子 +16.1°，实测影带 +28.9°（同象限右上，差 12.8°≈路面横坡+时间差），即影子背离太阳、投影物在相机背后。**已部署诊断构建**（02:42，28,667,392 B，md5 `729eb8a6…`）：新增 `dbg_position.bmp`（G-Buffer 视空间位置/法线）与 `dbg_color_sun.bmp`（原始帧上按相机+`E.sun_dir` 画黄色太阳十字），日志输出 `sun screen ndc/uv`。**明天实测**：①同位置只转 ~50° 连做两次 `r__dump_shadow_buffers 1`+F12，退出刷日志 → 用 rt_Position 对同一世界地面点比对明暗，直接判定是否"视角绑定"；②黄色十字 vs 可见太阳圆盘 → 判定天空烘焙太阳是否不跟随动态太阳（"第二光源"候选）；③光柱锚定为独立问题，待太阳完整入画+光柱可见的截图量汇聚点。详见 `docs/progress/notes/2026-10-26-mt-shadow-direction.md`。

> **相机验证：太阳位置 1.4px 吻合 + 双视角影子方向（2026-10-26 08:49）**：玩家同位置两视角各做 `r__dump_shadow_buffers 1`+F12（相机相差 72°），日志给出两帧相机与 `E.sun_dir`。视角 1 可见太阳圆盘过曝质心 `(576.8, 62.8)`，引擎投影 `(575.4, 63.5)` → **误差 1.4px**，据此否定"天空烘焙太阳不跟随动态太阳"假设；两视角的平地影子方向场与实际阴影一致（视角 1 太阳在画面上方偏左、卡车左侧亮/车影朝右；视角 2 太阳在左缘外、灌木左上亮/影子朝右上），屏幕方向随视角改变 → 未见屏幕锁定。**注意**：本次跑的是游戏自带 `bin` 里的旧 exe（10-25 23:30），未产出 `dbg_position`/`dbg_color_sun`，且两次 dump 旧文件名互相覆盖；新构建（08:54，28,666,368B，md5 `7bb53dd4…`，dump 文件名带 `_<n>` 序号）已放 `MODS\MT-CN\bin`，需启动器停用→启用 MT-CN 挂载后再做一次双视角测试，完成 rt_Position 世界点比对。详见 `docs/progress/notes/2026-10-26-mt-shadow-direction.md` §7。

> **09:04 双视角实测结果（2026-10-26）**：新构建已挂载并跑出 `dbg_*_1/_2`。视角 1 相机 `(-48.57 5.24 -97.44)`、`dir=(-0.960 -0.097 -0.261)`、`sun_dir=(-0.8840 -0.4399 0.1581)`（太阳在背后 148°，仰角 26.1°）。① 画面中心偏左的长暗带在 `dbg_accum_1.bmp`（原始太阳 pass）里同样存在 → **暗带是 CSM 太阳阴影本身**，屏幕空间层（AO/SSS/IL/雾/光柱）对新证据可以排除。② 平地影子方向灭点 dump `(1056,381)`（右上）；暗带反投影世界轴向 `(0.743,0.669)`，与 `L_h` 夹角 127.9°（把下端当根部则 52.1°）；暗带位于左陡坡脚，45–60° 坡面可把同一条太阳影扭成屏幕向下，故单帧**不能**判定 CSM 方向错。③ 08:49 平地卡车对照仍与太阳一致 → 不是全局翻转。④ **世界点 A/B 仍未完成**：第二视角转了约 100°、无共享地面；dump 2 相机行被刷盘截断（日志尾 `* [DBG] ca`）；`dbg_position` 因 16F 钳位+RT 复用不可用。**下一轮**：平地+干净投影物（根部与影子同框）、原转 40–60° 保留同一片地面，各 F12+dump；平地仍 >20° 偏差才确诊 CSM 方向。下一版构建把相机/太阳写进 dump 文件名 + 增加深度专用灰度转储。详见 notes §8。

> **更正 + 三轮技能审查结果（2026-10-26 下午）**：§8 的"暗带在 accum 里所以是 CSM"**是错的**。SSFX 的 `accum_sun_near/far.ps` 结尾把太阳乘上 `s_ssfx_sss.b`；`.b` 不是常量，而是 `ssfx_sss_blur.ps` 两遍模糊后的**屏幕空间直接 SSS 遮罩**（`rendertarget_phase_blur.cpp` 的 blur_setup `(0,1,0,0)`→`(1,0,2,0)`），主视口 `ssfx_issvp.x=0`。09:04 日志显示该会话 `ssfx_sss(1,1,0,0)`、`ssfx_sss_quality(12,4,1,1)`、风也开着 → 暗带是 CSM+SSS 混合，不能单独归因 CSM。SSS 遮罩按 `-L_sun_dir_e` 做屏幕空间 march、0.95 时间累积、两遍模糊，天然视角绑定——这正是"树影跟着视角走"的真实机制。两轮独立子代理复核一致：CSM 渲染/采样同源（`X.D.combine=cascade.xform`、`m_shadow=TexelAdjust·combine·mInvView`、`gbd.P` 视空间），符号/手性/矩阵顺序/正交/深度翻转全对，**CSM 方向在数学上不可能旋转**。另查出两个真实 CSM 缺陷：FAR 级联体积用 `xform_prev`（`:569-572`）；无投影物时 `phase_smap_direct` 不清空但累加照跑（`R_sun.cpp:311-317/357-365`）→ 旧阴影图配当前矩阵采样，运动中产生视角漂移错影。修复方案：A) 引擎开关把 SSS Dir Len 置 0（一帧内分离 SSS 影，先做）；B) 清空移出条件；C) FAR 体积改用 `xform`。详见 notes §9。

> **两个 CSM 真缺陷已修（2026-10-26 09:56 构建）**：① `R_sun.cpp` 把 `phase_smap_direct`（bind+clear）与阴影 pass 的 view/project 设置移出"有投影物"条件——每个级联无条件清空；② `r4_rendertarget_accum_direct.cpp` 恢复 `R2FLAGEXT_SUN_ZCULLING` 条件（默认 flag 关闭 → FAR 用自身 `xform`，不再用中景 `xform_prev`）。`build_mtcn.cmd AVX NOPAUSE` 0 错误，exe 28,666,368 B，md5 `2ba29c96eb2ab559d8cb98034728477b`。**已按玩家要求暂存到 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`**，旧诊断构建保留为同目录 `AnomalyDX11AVX_diag0854.exe` 作回滚。**当前游戏 `bin` 是原版 exe（48,067,584 B / 10-03 / `f983ce27…`），MT-CN 未挂载**——测试前需在启动器启用 MT-CN，确认 `bin\AnomalyDX11AVX.exe` 变为 28,666,368 B / `2ba29c96…`。SSS 层（方案 A）未动。详见 notes §10。

> **配置修正（2026-10-26）**：截图键 `kADD → kF12`（删除重复的 `bind_sec`），解决与 MCM `beef_nvg/keybind_nv_up = 78`（小键盘 +）的撞键；F12 未被其他键位占用。MCM `ssfx_module/sss/enable_dir_mcm` 与 `ssfx_sss_quality.z` 同步关闭（屏幕空间直接 SSS 影，§9 证实的视角绑定来源；omni 保留），使本次真机测试只验证 CSM 修复。MOD 副本与游戏当前生效的 user.ltx/axr_options.ltx 同步修改；并把 `anthology_runtime_seasons/style = 6` 补进 MOD 的 MCM 副本。风/光柱/雾/光晕/AO/IL 未动。详见 notes §11。

> **暗屏修复 + 太阳光柱门控（2026-10-26 10:22）**：玩家关 SSS 后全屏变暗——根因是 `r4_R_render.cpp` 关闭路径只在"本进程渲染过 SSS"后清 `rt_ssfx_sss`；从启动起禁用时该 RT 保持创建值 0，太阳 accum 乘 `.b=0` → 太阳贡献全黑。修复：禁用时无条件清成 `(1,1,1,1)`（主视口+PiP 两支）。同时给体积太阳光柱加可见性门控：`need_to_render_sunshafts()` 用 `sun_adapted->direction` 经 `Device.mView` 转到视空间，`vs.z<=0.05`（太阳在视平面后）时整层不渲染，`accum_direct_volumetric()` 同步调用该门控。构建 10:22，28,665,856 B，md5 `4f46cd1849542467d429dcf5789a4e9d`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；备份 `AnomalyDX11AVX_csmfix0956.exe`（09:56）、`AnomalyDX11AVX_sssclear1019.exe`（10:19）、`AnomalyDX11AVX_diag0854.exe`。待重启/重挂验证：亮度恢复、太阳在画面内才有光柱；若仍有随视角宽带，先关 `ssfx_fog_scattering`（该层不读太阳方向）。详见 notes §13。

> **引擎侧太阳锚定光柱 + 级联真值转储（2026-10-26 11:33）**：新增引擎自有 `csm_sunmask.ps` + `CBlender_sunmask` + `apply_sun_volumetric_mask()`，在远景太阳体积光绘制后按太阳屏幕位置做径向遮罩 → 体积光柱只在太阳处出现（跟随视角的 SSFX 体积光被绑定到太阳）；太阳在画面外/背后时光柱为 0。`r2_sunshafts_mode` 改回 `volumetric`。`r__dump_shadow_buffers 1` 新增输出三条 `* [DBG] cascadeN r0..r3` 级联矩阵。构建 11:33，28,669,952 B，md5 `dbd8116bee108449ba6dd37bf7034017`，已暂存 `MODS\MT-CN\bin`（11:09 备份为 `AnomalyDX11AVX_jitter1109.exe`），新 shader 已在 `MODS\MT-CN\gamedata\shaders\r3`。DLSS 未改。详见 notes §17。

> **逐级联阴影图捕获（2026-10-26 12:35 构建）**：`r__dump_shadow_buffers 1` 后**下一帧**在 `render_sun_cascade()` 内逐级联读回 `rt_smap_depth`（`dbg_smap_c0/1/2.bmp`，D24S8/D32 正确灰度化）并输出 `* [DBG] smap c=N ...` + 该级联矩阵 + 该级联的 `probe ... mode=N` 行（`mode=0/1/2` 分别对应近/中/远）。帧末探针只剩 `mode=2`（FAR），不再产生无意义的 c0/c1 行。构建 12:35，28,676,608 B，md5 `d10bbfe719f1bd9e77258568c1d2d6d8`，已暂存 `MODS\MT-CN\bin`；12:03 构建备份 `AnomalyDX11AVX_probe1203.exe`。判读要点：`side` 在与矩阵自洽时恒正，不作为方向证据；看 `occ` 世界坐标/高度、`dm-dp` 差、`sh` vs `lum`、以及不同朝向同一 `pw` 的 `sh` 稳定性。详见 notes §20。

> **12:45 最终诊断构建**：探针网格 40 px；`r__dump_shadow_buffers` 新增 `* [DBG] layers sss_q/ao_q/il_q/fog_scatter/shafts_mode/wind_tree` 自述行（MCM 会重新打开 AO/IL，必须记录当帧真值）。28,676,608 B，md5 `cbe68c7b060bc9f7dc5f15105b4c1cfd`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`，12:35 构建备份 `AnomalyDX11AVX_percascade1235.exe`。详见 notes §20–§21。

> **12:49 最终构建**：`dbg_smap_d<seq>_c<N>.bmp` 按 dump 序号命名（两次朝向的阴影图都保留）。28,676,608 B，md5 `0d4a2a6e690943c8dea099f062efbff0`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；12:45 构建备份 `AnomalyDX11AVX_probe1245.exe`。测试必须重新挂载 MT-CN，日志 `[MT-CN] engine build ... exeSize=28676608` 为新构建。

> **12:59 两组 dump 判读（2026-10-26）**：近级联光轴与 `sun_dir` 一致到 <0.0005；细影带 x≈520,y≈520–600 的阴影图遮挡物为竖直细杆 `occ≈(-58.3,1.7–4.0,-95.8)`（相机前 10m），同列 G-buffer 只见 14–16m 处地面 → 疑似“无物体的影子”。两次 dump 朝向差 94°，地面不重叠，跨视角 A/B 未完成。新构建 13:28 `d5fa352c…` 增加 `dOcc`（遮挡物投影处可见面与 occ 的距离）+ 过滤 `dp>1` 越界样本，已暂存 `MODS\MT-CN\bin`；12:49 构建备份 `AnomalyDX11AVX_cascadeprobe1249.exe`。详见 notes §23。

> **13:47 dOcc 轮 / 14:01 修正构建**：dOcc 结果暴露探针自身缺陷——`occ` 被投影到镜像的屏幕 x（例子：距接收点 0.78m 的 caster 应投到 u≈0.62，日志 cv.u=0.379），因此 dOcc 全部无效，不能作为阴影结论。新构建 `a843041c…`（28,679,168 B）改为本地组合 `mProject*mView`、每行输出 `rcv` 自检、逐级联捕获只对主目标执行。已暂存 `MODS\MT-CN\bin`；13:28 备份 `AnomalyDX11AVX_docc1328.exe`。详见 notes §24。

> **14:34 轮结论：探针自身镜像，dOcc 作废**：`rcv` 与采样像素在 u 上系统性互为镜像（平均误差 0.509，v 仅 0.00025），例 (0,0)→rcv(1.000,0.001)。因此该轮 `caster_near/far` 全部无效。已改为“重建公式的解析逆”做正投影，并把 `p11/ht/vt/view/target` 写进日志；判据改为“树冠 caster 的 dOcc 应普遍变小”作为独立验证。构建 14:5x md5 `879b40b893b2d5c2ed3e38e335bc0547`（28,679,168 B）已暂存，14:01 备份 `AnomalyDX11AVX_rcv1401.exe`。详见 notes §25。

> **14:54 轮：探针自检通过，抓到同一根“无主遮挡物”**：`rcv` 误差 ≤0.00046，dOcc 可信。c0 的 38 个可投影阴影样本中，遮挡物全部比接收点更靠近相机；`up<1m` 类 22/32 判为可见（探针判读正确）。x=480 列三次采到同一世界位置 `(-58.2, 2.5–5.9, -95.5)`（相机前约 10 m），该处可见面差 `dOcc=6.4/21.7/90.8 m`，与 12:59 轮细影带 caster `(-58.3,1.7–4.0,-95.8)` 同点 → 阴影图里有 colour pass 不画的竖直遮挡物。新构建 15:0x `764b9760…`（28,684,800 B）加 `nearvis` 列表（相机 35 m 内全部静态 visual 的 .ogf 名/包围盒），用于判定该点是“被剔除”还是“根本不在静态层级”。已暂存 `MODS\MT-CN\bin`。详见 notes §26。

> **15:16 轮：无主遮挡物对上静态模型**：`rcv` 误差 ≤0.00046（探针可信）。`mode=0` 中 `dOcc>3` 的三点 `(-58.5,2.2–5.1,-95.9)`（相机前约 10 m，dOcc 4.6/13.9/111.1 m）全部落在静态 visual **#0（type=11 MT_TREE_PM，box 高 0.4→33.2 m）** 的 AABB 内 → 这棵树被 colour pass 漏画、却进了阴影图，正是"影子没有对应物体"。其余 far 行只被地形 visual 包住，属贴地台阶正常情形。新构建 `ddf595c0…`（28,685,312 B）在 nearvis 行里输出 `ssa/frust/hom/fdrej` 四项可见 pass 判定，用来在 HOM 误剔、全细节剔除不一致、视锥、progressive LOD 四种成因之间一次定案。已暂存 `MODS\MT-CN\bin`；15:04 备份 `AnomalyDX11AVX_nearvis1504.exe`。详见 notes §27。

> **15:33 轮：三条剔除全部排除**：93 个静态 visual 的判定为 92×(frust=1,hom=1,fdrej=0)，`r_ssaDISCARD=0.000`（SSA 丢弃未开）。ghost 树 `#0 (MT_TREE_PM, box y 0.4→33.2)` 在 d1/d2 三次采样里稳定出现（`occ≈(-58.3,2.6–5.1,-95.2)`，`dOcc=59.7/7.1/6.6 m`），却没被任何剔除解释。只剩"没进颜色队列"或"进了队列但没画出来"两种。新构建 `9a2707d2…`（28,687,360 B）在 `nearvis` 行加 `subC/subS`（按帧记录的入队集合）一次定案，已暂存 `MODS\MT-CN\bin`。详见 notes §28。

> **15:49 轮 + 截图**：ghost 树 #0（`MT_TREE_PM`，AABB 4.6×32.8×5.2 m，相机前 ~10 m）在 `frust/hom/fdrej` 全通过、`subC=subS=1` 的情况下仍无像素；其预测位置 (468,459)/(474,562) 内部像素上 G-buffer 是 27–39 m 的地面。截图里影带"指向"的是另一棵 24–27 m 处的暗树干 → 影子与可见树对不上，与玩家描述一致。发现并修掉仪表缺陷：`subC` 原先在 pass 循环之前打标记（passes 全 null 也记 1，等于没证明）；现改为**入队 pass 计数**并附 `se/npass/prio/vs/ps`。构建 `888994c5…`（28,689,920 B）已暂存。详见 notes §29。

> **新增 `r__sun_shadow_casters`（0 all / 1 no_trees / 2 terrain_only / 3 none）+ `shoffset` 数值量测**：开关只过滤太阳阴影级联的投影物（颜色 pass 不动，所以它不会让物体消失，只会让影子消失），dump 行 `sun casters mode/kept/skipped` 自证生效；`shoffset` 对每个 >8 m 高的静态物体量"影子带中心横向偏移 dlat"和"沿太阳方位的影子起止 along"，正确值应为 `dlat≈0`、`along≈0..L`。构建 `be19e90d…`（28,696,576 B）已暂存。测试协议与恢复命令见 notes §30 及对话内说明。

> **17:23 轮 A/B 定档（模式 1/2/3/0）**：`kept/skipped` = 560/1124、100/1574、0/1654、mode 0 未计数（已修）。c0 阴影样本 `sh1` = 32 / 22 / **0** / 50（c1 54/43/0/155，c2 68/60/0/187）→ 细影带与树冠斑点由**树**投影，右侧大片暗区由**地形**投影，`3 none` 时太阳阴影为零。开关本身工作正常，影子完全来自 CSM 深度图。截图序 17:19:13/17:19:48/17:20:19/17:21:21 = 1/2/3/0。

> **17:47 轮：shoffset v2 成立但不能定案，改仪器**。`groundY=` 出现（量法生效），c1/c2 一致的 `dlat=0.20~4.60`、`along` 起点 0.0~20.3 —— 但森林窗口里邻树的影子能占同一段，`dlat` 无法区分"错位"与"邻树"。离线读 `dbg_smap_d1_c*.bmp` 复核：树顶处 `dm-dp≈+0.11`（=62 m 光线长度，即该列只剩地面）说明图里"这棵树不在它该在的列"，但 8 位灰度每级 2.0~2.6 m 不足以给可信纹素偏移，且 c1 `dm=1.0000` 是地面被远平面夹住而非空。**新增 `r__sun_shadow_casters 4`（图里只留一棵树）+ `svp_single_caster_blob`（用完整 24 位深度输出 `centroid/predict/dtex`）**，把"影子相对物体挪了多少"变成唯一可归属的数字。构建 `b0024a5d…`（28,700,160 B）已暂存 `MODS\MT-CN\bin`，上一版备份 `AnomalyDX11AVX_shoffset2_1735.exe`。测试协议见 notes §31。

> **18:24 轮：mode 4 空转，两个原因已修**。（1）扇区根列表给的树指针与级联插入路径收到的不是同一层对象（组 vs 部件），`kept=0 skipped=1640/1700` → 改为**在过滤器内部记录候选并锁定**（`R_dsgraph_pick_single_sun_caster()`，`single caster picked ... from N candidates`）。（2）`R_sun.cpp:336` 的草阴影 `Details->Render()` 绕过投影物过滤，kept=0 时图里仍有 28569/4463 个非清除纹素 → 实测用现成命令 `r__enable_grass_shadow off` 排除，blob 行加 `grass=` 自证。另用同相机四张 A/B 截图逐像素相减定量：全部 CSM 改变 62.9% 像素、平均暗 20.6 亮度；树单独贡献 51.0% 像素（远大于"细影带+斑点"的定性描述）。构建 `56ffe07c…`（28,701,696 B）已暂存。详见 notes §32。

> **19:18 mFullTransform 顺序修复构建**：`svpcam::DeriveZoom`（r4_R_render.cpp:292/317）把 `mFullTransform` 建成 `mView·mProject`，而 `device.cpp:434`（ApplyDevice）是 `mProject·mView`——列向量 `transform()` 约定下反序等于把投影矩阵喂给世界坐标，镜头遍所有 `Device.mFullTransform.transform()`（累加遍深度裁剪 d_Z、enable_scissor、draw_rain）拿到垃圾裁剪坐标。已改为正确顺序，全库 full-transform 构建点统一。`build_mtcn.cmd AVX NOPAUSE` 1m17s 0 错误；28,701,696 B，md5 `66308bafdf8f79c1e11d06fb48190ce8`。真机确认：镜内亮度/光影正常（无副作用），但**不是树影错位的解药**。详见 notes §33。

> **19:40 静态闭环深挖（全世界影子错位 + DLSS 关闭也错位）**：日志确认 19:36 测试构建含 mFullTransform 修复、`[Upscaler] Off` 运行时生效（DLSS 真关）、`ssfx_sss_quality z=0`（SSS 方向层真关）→ DLSS/SSS/TAA/omni(0/8)/rain/sun_original 全部洗清，影子 100% 来自 CSM 深度图（`r__sun_shadow_casters 3` 时影子全无）。按用户要求全文（非片段）重读整条太阳阴影链：`R_sun.cpp`、`accum_direct_cascade`、`blender_light_direct.cpp`、`CameraManager::ApplyDevice`+调用树、`r__dsgraph_render` 矩阵序列、`u_compute_texgen_screen`、`pos_decompress` binder、缓存 CRC 机制、`accum_sun_near.ps`（整合包两 mod 版）、`gbuffer_stage.h`、`shadow.h` 全。**「影子绑视角」的每种代码机制逐一排除**（mInvView 失步/smap 内容污染/矩阵顺序/双重逆变换/FOV 不同源/PCSS 屏幕半径/sunfilter 旁路/旧缓存/对齐单侧/shadow_bias）。静态链 100% 闭环 → 收敛为两种可能：A「自有引擎无问题」前提（用户已实测验证成立）；B 运行时 binder 值错误。CCCC 目录文件为 OGSR 游戏数据，不可代表 Anomaly（用户指正）。详见 notes §33。

> **19:54 `r__shadow_probe` 数值见证仪器构建（B 计划）**：新增 cvar `r__shadow_probe 0/1` + `r__shadow_probe_step 1..600`（默认 30 帧）。实现于 `accum_direct_cascade`（m_shadow 送着色器之后，纯只读）：对固定世界点 P 每 step 帧每级联打印 `A=m_shadow·mInvView·P`（着色器实际采样坐标）与 `B=TexelAdjust·combine·P`（相机无关正确值）及 `dUV`（=错位量，sm UV 单位），附 cam/invT/xf_t/p22/rsw/fov/sun。判据：dUV 随旋转增长→mInvView 旋转陈旧；随平移增长→mInvView 平移陈旧（ApplyDevice 时机）；xf_t 不随相机→盒未重拟合；A≈B→采样链数值正确，转查 smap 内容。`build_mtcn.cmd AVX NOPAUSE` 1m02s 0 错误；28,704,256 B，md5 `c269fb397ecedfadff00acbed5c4cf68`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；mFullTransform 修复版备份 `AnomalyDX11AVX_mtfx1918_1954.exe`。测试前需 JSGME 停用→启用 MT-CN。测试协议与判据表见 notes §34。

> **20:09 probe v2 构建（v1 判读后升级）**：v1（19:54 构建 c269fb39）真机 175 行数据结论——**引擎矩阵链数值体检通过**（invT 恒等于 cam 且随相机同步 → mInvView 无陈旧/ApplyDevice 每帧正常；xf_t 随太阳合理微动；p22 与 FOV 自洽）；v1 的 dUV 巨大（6.4/2.1/0.8 UV）经数量级分析证实是**仪器自身 bug**（把世界点喂给 mInvView 得到 cam+R·P_world 模 ~127 的伪向量；正确为 m_shadow·mView·P，而与 B 数学恒等——A/B 对比设计不独立）。jitter 链（binder 逻辑全文复现）排除。v2 三指标：①dUV 自检（mView 版）②**gbrErr——复现 gbuffer_stage.h 位置重建（uv+tan+RenderScreenWidth+jitter → gbd.P vs 真实 mView·P）**，攻唯一未数值化的运行时链 ③三方尺寸对账（RenderScreen vs RT 对象 vs Device 显示）。构建 1m00s 0 错误；28,705,280 B，md5 `0fd1961ef556508ee5c3cf2b2676ac2c`，已覆盖 `MODS\MT-CN\bin`；v1 备份 `AnomalyDX11AVX_probev1_1954.exe`。测试前需再次 JSGME 停用→启用。详见 notes §35。

> **21:39 轮：根因找到并修复 —— R4 `pos_decompression_params.HorzTan` 符号反了（x 轴镜像）**。probe v2 的 144 行给出 `dUV=0`（矩阵链无暗伤）而 `gbrErr` 逐条精确 = `2×Pview.x`（站定屏幕中心 0.016 m 只剩 jitter；Pview.x=-14.21 → 28.4295；-23.18 → 46.3680）→ 着色器重建出的视空间 x 是真值的镜像。三方对齐：三份 `gbuffer_stage.h`（SSFX/热成像/R.A.K）同为 `P.z*(pos2d*zw - xy)`；R2/R3 binder 的 `HorzTan` 为正（`-VertTan/fASPECT`），R4 被改成 `-1/_11`＝负，而 `_11=_22*fASPECT` 说明正确等价形是 `+1/_11` —— 只差符号，y 两版一致故只有 x 镜像。git 证据：初始 `d404b95d9`、DLSS 移植 `9dc5a5b58` 均正号，`8710b80c4 fix(pip)...derive the position decompression from the projection` 直译时翻错。这解释了 §33"静态全文闭环却症状仍在"（跨层符号约定错，且镜像对往返自检不可见——14:0x 我把它当仪器约定绕开，反而掩盖了它）。修复三处：`r4.cpp` 取正号、probe 同步并改 `uvy=0.5-0.5*ndc.y` 与去掉重复 jitter 减法、诊断侧 `r4_R_render.cpp` 同步。构建 `bc0f2568…`（28,705,280 B）已暂存，probe v2 备份 `AnomalyDX11AVX_probev2_2009.exe`。单机位仪器本轮起有效（`kept=1`、`blob c=2 dtex=(115.4,71.8)`，选优需加"距相机 <25 m"）。详见 notes §36。

> **22:0x 真机验收：影子错位已修复**（构建 `bc0f2568…`，HorzTan 符号修正）。用户实机确认"影子正常了"。根因与证据链见 notes §36；该修复尚未提交，工作树内。

> **22:52 轮（M82A1M 准星 / 外骨骼手表 / Promin 黑屏）**：`r__dump_section` 一次拿到 603 行解析后配置，绕开 `.xdb` 字典压缩。① M82A1M 与 M82 的放大键**完全相同**，只多 `s3ds_reticle_type=5`（+`ms_count/ms_colors`）；3DSS 脚本缺省 0 → M82 用贴图准星、M82A1M 用按角度绘制的程序化准星，故放大后变粗。② 外骨骼手表黑屏：`wd_exo.script:51` 把 `blocked` 作用到**全部**设备屏幕，而 BHS 补丁的 `promin_screen=false` 正是 blocked 的来源 → 以 MT-CN 覆盖 `wd_exo.script` 收窄到 `spec.id=="promin"`，带载入标记自证生效。③ Promin 黑屏门槛为 `is_worn and is_powered and not hidden`，且 `wd_battery` 用**物品耐久**当电量（`power_critical=0.05`，戴着持续掉）→ 交付 `zzz_mtcn_promin_probe.script` 直接把三道门打进日志。裸手臂穿模是 BHS 对 `_none` 判 `mode=unsupported`，非引擎问题。详见 notes §37。

> **23:30 轮**：MT-CN 的 `wd_exo.script` 散件覆盖确认生效（标记行打印、无 Lua 报错，散件压过 `db\scripts\*.xdb`）。同时**推翻上一条"耐久当电量"推断**：本轮设备为新刷（`powered=1`），且 `mode=builtin` 按 `zzzz_wd_gfy_exo_screen.script:160` 的语义就是"Promin 未穿戴"→ 那次黑屏属预期。待判病例收敛为"门全开时是否仍无内容"（22:52 出现过 `worn=1 powered=1 hidden=0` + `mode=promin`）。探针 v2 修掉 `printf` 只认 `%s` 导致的丢值，并加 `attached/uiModule/uiGet/watch hidden/exo`；覆盖脚本内加 `wd_exo hide:` 逐设备打印。详见 notes §38。

> **23:46 轮**：探针 v2 读数完整。覆盖确认生效（`kind=blocked` 下手表 `screen=false`、只有 promin/vektor 被屏蔽）。**症状 1 改判**：外骨骼下手表消失的主因是 `apply_hide_flags()` 里对所有设备的 `set_model_hidden(name, worn)`（日志 `model=true` 全部设备），screen 屏蔽只是次要一半 —— 是否同样收窄待用户定。Promin 五道门全通过（`worn=1 powered=1 hidden=0 ratio=1.000 attached=1 uiModule=d_promin_ui uiGet=true lit=1`）→ 无内容嫌疑收敛到外骨骼材质链与 mode 选择。穿模原因查明：非外骨骼手臂只有一套 `DEFAULTS` 偏移、BHS 的 per-model profiles 只覆盖外骨骼视觉（并更正 `PATCHES[community]` 是阵营贴花不是变换）。详见 notes §39。

> **10-27 00:2x**：热成像搁置（用户决定）。为继续查 PDA/手表屏幕内容，加 `r__dump_file <fs_root> <path>`：走引擎 FS 取覆盖胜出者、落盘到 `appdata\logs\dbg_file_*`，并用 `file::vfs` + `m_archives` 报出**是哪个归档给的**（`-1`=散件）。目标已由脚本确定：Promin 屏幕着色器 `r3\wd\crt.ps/.s`、Vektor `r3\wd\crt_vektor.ps/.s`、通用 `r3\wd\ui`；这些在磁盘上不存在，只在 `.xdb` 里。构建 `2fc55ba6…`（28,717,568 B）已暂存。详见 notes §40。

> **10-28 18:1x｜快旅选项缺失定案（mod 内容缺陷，引擎无责）**：汉化包 `fix_fast_travel_markers.script` 的目的地判定用**名字子串**（`smart_terrain`/`backpack`/`_bag`…），而标记点循环用 `clsid() == clsid.smart_terrain`；AnthologiA 的自定义 smart terrain section 叫 `pre_sim_*`/`pre_baza1`，不含该子串 → "标记点有、快旅项没有"。MT-CN 覆盖加 `pcall` 保护的 clsid 兜底（diff 恰好三处）。**真机已验收**：18:26 那场日志三次 `traveling to pre_sim_12 / pre_bar / wiz_nauch`。撤回过的错误结论一并留档：曾判定"两侧回调名不匹配且无人转发"，被日志证伪（base 确实派发 `map_spot_menu_add_property`，id 有效），桥接脚本已从两处删除。

> **10-28 19:xx｜MT-CN 中文界面补齐**：包翻译走 string id，硬编码 Lua 字面量永远覆盖不到（`use_english_text_for_missing_translations 1` 让它们显示英文）。扫 182 个 mod 脚本目录 / 1314 个脚本：19 个 mod 有英文 UI 字面量共 200 条，其中真正玩家可见的是 WD 可穿戴 94 条与 Banjaji 调试启动器 50 条（F7 菜单）；`Aim Position`/`Fire Point` 等骨骼插口名、库注册名属**不可翻译**层。发 `zzz_mtcn_wd_i18n`（包 `wd_gui.register_*` / `wd_mcm.on_mcm_load`，整串精确匹配）与 `zzz_mtcn_dbg_launcher_i18n`（包全局 `prepare()`，`content[*][*].name` 仅显示用；生成面板分类是 `itm_to_spawn_classes` 的连接键，翻译会静默破坏筛选，故不动）。id 级基本完整：chs 89,402 条，仅 24 条被引用 id 缺中文（18 条在「英文适配 2.1」内属有意英文）。两条 MCM 硬编码散文用 **prose-as-id**（MCM 的 `text` 会过字符串表）落 `configs/text/chs/st_mtcn_i18n.xml`。用户定名：`Promin → 护腕PDA`、`Vektor → 护腕表`。**尚待真机看**：`wd i18n active` / `dbg i18n active` 未在 18:26 日志出现（那时尚未部署）。

> **10-28 19:4x｜部署通道定案为 MO2 单一来源**：证据链——只在 mod 里存在的 MT-CN 脚本照样加载 ⇒ 挂载层赢读取；据此发现并拆掉两个隐患：mod 里带的 `gamedata\configs\axr_options.ltx`（10-26 11:02 快照 `b4d53b9f`）与 `appdata\user.ltx`（10-26 11:32）是**游戏自己每帧读、退出时写**的可配置文件，快照赢读取会把玩家后来改的选项每启动退回（当前两份差异正是 `speed/encumbrance`、`speed/weapon`）。`[modded_exes]` 段只有一行 version，MT 的值在 `user.ltx`（实测 `scheduler_batch_size 512`、`lua_use_functor_cache 0`、`mt_task_manager 0`），且根目录活文件里 MT 相关值与快照逐条一致 → 移出不丢设置。现分工：`mods\MT-CN\gamedata` = gamedata 唯一来源（30→28 文件），`MODS\MT-CN` 只剩 exe/dlss dll/db0/说明/清缓存 bat；备份在 `_mtcn_offline_20261028\`，`来源与要求.md` §二三 与 `README_部署与撤回.txt` 同步改写。

> **10-28 20:0x｜桌面「补丁」目录审计（结论：不要安装）**：482 文件逐条 md5 比对。它 = 上游 1.0.5.6（2026-10-12）提取物 + SSS 23.5 mod 目录副本。**0 个我们缺失的文件**（双方文件集合完全相同）；SSS mod 476 个里 427 个与已装全等，49 个差异**全部我们占优**：44 个 MCM 文本仅 `&`→`&amp;` 之类转义（且补丁少一条 desc id、无 chs），5 个着色器是我们已装版本多了 MV include / `wpn_clamp *= 0.55f` 钳制；4 个 exe 是原版 Monolith 1.0.5.6，而我们 fork 的上游基线是 **10-24**、在跑的又是自研 MT 构建 → 覆盖等于双重回退。另注意其 mod 目录名（英文原名）与 MO2 里已装的中文名不同，整包拖入会产生第二个同名 SSS mod 争抢文件。

> **10-28 20:1x–21:2x｜仪器退场 + 修复落库 + 构建部署**：用户批准后先摘掉"已修复问题的遗留探针"——`r__dump_shadow_buffers` 全套 934 行（读回、数值探针、近旁 visual、着色器来源）、`r__sun_shadow_casters` 159 行、`r__shadow_probe`/`_step` 块、以及历史里已有的 `r__sss_dump_lights`（单独 `6fd9649d4` 删除）；保留 `r__dump_section`/`r__dump_file`+`CLocatorAPI::all_files()`（按需、仍是查 `.xdb` 的主力）。前三者从未进历史，删除前整树状态作为 `_pre_probe_removal_20261028.patch` 入库（`*.patch -text` 保证逐字节，历史里搜不回来）。落库 13 条（`8a4ccd887` HorzTan 去镜像、`f3cd37091` 级联无条件复位、`987d70bca` sun_original 阴影位、`f35395cb2` 光柱锚日+两道门、`9ee1fdf3d` 镜筒 full-transform 顺序、`550b72637` SSS 遮罩中性清屏、`be5f95b00` SSS 光槽即刻释放、`e9e39eab6` dump 工具、`4b40b9faa` 瞄准日志占位符、`ec24aa448` 文档、`b8364d7c2`+`8fcae7188` dist 快照补齐、`docs` 本条）。**构建**：`MSBuild -p:Configuration=DX11-AVX -p:Platform=x64` 0 错误，产物 28,679,168 B（小 40,448 B），md5 `3beee11e` / SHA-256 `ea6fcf23…`；**坑**：Git Bash 下 `/p:` 被 MSYS 当路径吃掉，MSBuild 报 MSB1008 仍 exit 0。已部署至整合包 `bin\` 并同步 `MODS\MT-CN\bin\`，清着色器缓存（458 文件 / 4 MB），旧构建备份 `_binbackup\AnomalyDX11AVX.probe-build_20261027_0108.exe`（md5 `d86e777b`）。穿模按用户决定关闭，`zzz_mtcn_wd_fit.script` 删除（副本在 `_mtcn_offline_20261028/probes/`）。

> **10-28 21:5x｜第三方代码出处台账（用户规则：引用他人代码必须给出明确出处）**：新建 `docs/THIRD_PARTY_PROVENANCE.md`（三档 + 复查命令）。**① 经 OGSR/OpenXRay 进入的全部是上游作者自己的提交**：`374947615` lulnope←OGSR `69433fa1`（`GraviZone::exit_Zone` 16 行）、`46fd396eb`/`7f0c6b54e` OneMorePseudoCoder←OGSR `ec6396cb`（`PHMovementControl` -451/+207 + `material_manager`）、`6fdad0a74` DEMONIZED←3 个 OpenXRay commit + OGSR `d359bde2`（41 文件）、`fbff6e64e`/`feac7dd7f` MFB←OpenXRay（smart cover）、`440495c79`"inspired by openxray"属启发。cherry-pick 保留了原作者名，处置=**代码内不插注释**（免得与上游合并制造冲突），追溯入口是本台账 + commit message。**② Vendored SDK**：`sdk/DLSS` = NVIDIA RTX SDKs LICENSE（"v. March 14, 2024"，头文件 SPDX `LicenseRef-NvidiaProprietary`；实核义务为 §2a 禁止剥离声明、§7 允许附带第三方声明，**不是**强制新增致谢；NGX 版本号待登记）；`sdk/FSR3` 463 文件 = AMD MIT + third-party notices 完整，但其 `README.txt:4` 自述取自 **OGSR checkout `8adb7cf1650120987d84fcc8be7e73ad8a6f9945`** 的 community D3D11 backend，**不是 AMD 官方 SDK tag**。**③ dist 中派生自 mod 的**：SSFX(ascii1457) 七个 `ssfx_*.ps` + `screenspace_mvectors.h`（差 9–33 行，作者头都在）、`night_vision.h`(Beefs，差 199 行=不同版本，致谢 LVutner/Sky4Ace/Meltac 在文件内)、`wd_body.vs`/`d_promin_health_ui`(WD)、汉化包的 `fix_fast_travel_markers`。本次给 `wd_exo`（基线实为 ROOT 那份 `b2ab76be…`，mod 目录那份差 14 行）、`fix_fast_travel_markers`（原件无作者，只能声明我方改动）、`st_monolith_dlss.xml`、`csm_sunmask.ps`（点明 `s_vol` 属 SSFX）、`Dlss/` 全部 8 个文件（此前一处出处声明都没有）以及 `DlssIntegration.cpp` 的依赖清单补了出处行；dist 与线上 MO2 保持 md5 全等；`st_monolith_dlss.xml` 的注释第一次插到了 XML 声明之前，已改回（声明必须位于字节 0）。**两条撤回**：子代理称"`deffer_grass.vs`/`deffer_impl_flat.ps` 被删掉 `@ Author` 块"——实测 SSS 原件本就无作者头（双向 grep=0，moddb 链接数一致 2/1）；称"`r4.cpp:524` 的 ssfx 探针需注明"——`git log -S ssfx_motionvectors` 显示出自上游 `5cdf9bdcc`（DEMONIZED，2025-06-28）。OGSR 仓库许可经 GitHub API 核为 `Other`/`NOASSERTION`，台账明确写了"注明出处不等于免责，对外分发前需单独确认其条款"。**编译验证**：注释改动后重建 DX11-AVX → 0 错误；产物仍是 28,679,168 B 但 md5 变为 `d32f9e11`，与已部署/已入库的 `3beee11e` 不同——PE 时间戳使构建不可逐字节复现，所以 exe 的识别要用 size + 源码提交，不能只比 md5；**游戏与 dist 仍保留 21:19 那份构建**（注释不改代码生成，无必要重新部署）。

> **10-28 22:2x｜出处审计二轮（用户要求"不要再有遗漏"）**：查清「作者是他人的、由我们落地」的提交实为 **11 条**（Chad×6、erepb、Burke T、vacwave、lulnope）。逐条把落地板与 `(cherry picked from …)` 源板的 +/- 行做归一化 md5 比对：**6 条一致、5 条属"承接+改写"**——`8bc95cd7f`（仅多一行 v0.2.2 标记）、`19b19090a`（include 结构不同，我方需 `<thread>`、无 `pch_script.h`）、`cb445886f`（我方补 items 清理循环与 `xrCriticalSectionGuard` 作用域）、`468e9bdc4`（多 `StateDecs desc;`、少 `continue;`）、`00c4c0897`（保留我方抛物实现并把 `H_SetParent` 扩到全部掉落物）。台账新增 **§1A** 逐条登记；基座谱系另有 4095 条上游提交（一次我们做的 merge `058c5d26d` 零冲突 + 继承），不逐条登记。**三类此前未记的他人代码执行面**：optick 为子模块（`bombomby/optick` pin `8abd28dee`，上游 ProfLander 2025-06-19 引入，我们仅 CI fetch）、CI 第三方 Actions 全部由上游 Dmitry 引入（我们只复用）、`src/3rd party/**`、`sdk/include|binaries|libraries`、`sdk/autoexp.dat`（自带 Microsoft 版权）、`compressor/` 均为基座自带，本分支未新增文件。**两处"待核"关闭**：`00_modded_exes_gamedata.db0` 出自 `compressor/! compress_gamedata.cmd:2` 的 `xrCompress.exe gamedata -ltx build_patch.ltx -pack -strong -1024 -db`（工具属上游，我们只归档，md5 `b8bb004e`）；NGX 头文件无版本号，可辨识信息 = 版权 2018-2023 + 许可文本 "v. March 14, 2024" + 运行时 dll 310.9.1.0。**我自己的两处错并纠正**：① README §四第 6 条把仓库根 `night_vision.h` 说成"待清理的重复副本"，实为上游自带（`29633bd1`，DEMONIZED 2024-09-25 "gamedata reorganize"），两份都有主、不可删；② 行尾归一化时用 latin1 读 + utf8 写，把两份 `README_部署与撤回.txt` 写成双重编码，已按字节还原（与 HEAD 差异回到干净 +9/−0）。另单独验过 README 中译未丢致谢：翻译前后 GitHub 人名/资产链接集合完全一致（Arszi、OpenXRay×3+OGSR、clayne 三处均在）。
>
> **10-28 22:05–22:14｜目录事件（JSGME 停用→启用）**：证据是 `MODS/!BACKUP/` 出现 `bin/AnomalyDX11AVX.exe.MT-CN`、`db/mods/…db0.MT-CN`、`清空着色器缓存.bat.MT-CN` 与游戏根 `MT-CN_说明/` 的 22:14 时间戳。**无损失**：根 `bin\AnomalyDX11AVX.exe` 仍是部署的 `3beee11e`（28,679,168 B）、`db0` 仍 `b8bb004e`、MO2 的 `+MT-CN` 仍在 modlist 第 2 行（27 文件）。被清掉的是根 `gamedata\` 里那批 18 个历史遗留散件着色器——因 19:40 起 `MODS\MT-CN` 不再携带 gamedata，重新启用也不会放回。**后果**：根目录已无兜底副本，**必须经 MO2 启动**，否则 DLSS/FSR3 契约、SSFX 覆盖、穿戴设备与快旅覆盖全部失效；已写入 `README_部署与撤回.txt` 第六节（MODS / dist / 游戏根三份 md5 一致 `e2ae8892`）与仓库 README §四第 5 条。
