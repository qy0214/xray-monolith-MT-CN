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
