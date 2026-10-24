# 上游全景对比（themrdemonized）— 2026-10-24

> 范围：作者全部仓库 + xray-monolith 全部分支 + 与当前 `HEAD` 的逐项比对。
> 方法：`git fetch upstream --prune --tags` → `merge-base --is-ancestor` / `rev-list --count` → 对候选补丁 `git apply --check` / `git merge-tree --write-tree` → 读我方源码确认“问题是否真的存在” → `src\tests\run_tests.bat`（89 checks, 0 failures）。
> 结论优先级：**先看第 0 节，再看第 4/5/6 节；第 3/7 节是“不要碰”的清单。**

## 0. 结论摘要

1. **主线已同步**：当前 `HEAD`（`bedb0c87c`）已包含 `upstream/all-in-one-vs2022-wpo-mt`（2026-10-21）和 `upstream/all-in-one-vs2022-wpo`（2026-10-11）的全部提交；`2026.10.11` 标签也在线内。没有“漏合的主线提交”。
2. **唯一在等我们的上游线是 `upstream/pr`**（2026-10-22，33 个提交 / 21 个真实提交，尚未进 `all-in-one-vs2022-wpo`）。其中约 6 项是可低风险搬运的修复，其余是功能项。
3. **唯一直接对得上“性能瓶颈”的是草/细节实例化**：分支 `grass-instancing-2` 与 PR **#577**（MT 版）/ #567。它能打 85% 顶点来自 flora 的痛点，但**未合并、需要配套 shader、与我们的 SSFX/MT 着色器有冲突**，只能走独立实验分支 + A/B，不能直接进发布。
4. **两个“小而值”的开放 PR**：**#777**（`xrsharedmem` 排序数组 → 哈希表，加载期 O(n²)→O(1)）和 **#765**（雨水湿度按帧率累积的 bug，高 FPS 下干得快 2.4 倍）。
5. **明确不要搬**：`grass-instancing-2` 里的 visual-memory / ToCToU 补丁（基于我们已 revert 的旧实现）、`65534-loop-banish`（正则改写 Lua 脚本、默认开启）、`cmake` / 旧 SSS/BaS/vs2022 分支、以及整合包根本不调用的新 Lua API。

## 1. 仓库面：作者名下只有一个是“我们的引擎”

| 仓库 | 性质 | 结论 |
|---|---|---|
| **xray-monolith** | 我们的上游引擎（C++，2026-10-21 仍在更新） | 见第 2~7 节 |
| STALKER-Anomaly-modded-exes | 预编译 exe / 补丁发布仓库（~634 MB，最后推送 2024-09） | 不是源码；没有可比的引擎分支 |
| ixray-1.6-stcop | **Ix-Ray 1.6** 的 fork（不同代码库、不同渲染架构、30 个分支） | 与 Monolith 无共同祖先，不能按 commit 移植；“details-batch-1024 / feature/pip / cbuffer-shaders”等只能当思路参考 |
| DLTX-MiniMod-Pack、anomaly-demonized-scripts、Better-Stats-Bars、Mugging-Squads 等 | Lua MOD / 工具（非引擎） | 不影响引擎对比 |

> 所以“所有引擎分支”实际只需覆盖 xray-monolith 的 27 个分支（GitHub API 口径）+ 39 个开放 PR 中与我们相关的部分。

## 2. 分支地图（27 个）

### A. 已完全包含在我们线内（12 个，零动作）

`all-in-one-vs2022-wpo-mt`（血缘）、`all-in-one-vs2022-wpo`（主线）、`master`、`all-in-one-vs2022-153-merge`、`hozar2002`、`BaS_NEW`、`SSS`、`SSS18`、`SSS19`、`SSS20`、`SSS21`、`SSS23`。

- 这些分支相对 `HEAD` 的 “ahead” 都是 0；`behind` 只是我们 MT 线自己的提交。
- 旧 `SSS*` / `BaS*` 属于历史天气/武器线，已并入或被后续实现取代，不需要单独回看。

### B. 唯一待并线：`upstream/pr`

- 2026-10-22 的 `9cc5604b8`，33 个提交（含 12 个 merge），21 个真实提交。
- 合并基是 2026-10-11 的主线；**不是** `all-in-one-vs2022-wpo` 的祖先，说明是作者把 PR 先收进 `pr`、还没回灌主线的暂存线。
- 逐项处置见第 4 节。

### C. 草/细节实例化实验线（3 个分支 + 2 个开放 PR）

| 对象 | 规模 | 状态 | 判断 |
|---|---|---|---|
| `grass-instancing-2` | 132 ahead（20 个真实提交），最后同步 MT 2026-10-11 | 未进主线 | 老实现（常量数组 + 自带 shader）；其中的崩溃修复多数已过时/已 revert，见 §5.4 |
| `grass-instancing-merge` / `grass-pre-instancing` | 20 / 7 ahead，2026-03 | 历史中间态 | `pre` 是 `grass-2` 的祖先；`merge` 是旧的分支合并，均不用 |
| **PR #577** `detail-grass-instancing-mt` | 10 文件 +485/−144 | open，2026-09-01，mergeable | **最接近可落地**；DX11 instance VB + 距离密度曲线 + 前到后排序 |
| PR #567 `detail-grass-instancing` | 65 文件 +1972/−409 | open，非 MT | #577 的非 MT 母本；不直接用于我们 |

### D. 历史 / 无关 / 不建议（12 个）

| 分支 | 为什么不动 |
|---|---|
| `cmake` | 713 ahead，但合并基停在 2025-08，是另一条 CMake 构建移植线；改动面巨大、与当前 msbuild 工作流冲突 |
| `65534-loop-banish` | 在 `script_storage.cpp` 里用正则把 `for i=1,65534` 改写成 `pairs(server_objects_registry)`，**默认开启**；会改写任意 mod 脚本，语义风险高于收益 |
| `cform_cache` | 3 个提交全是 revert（含 HUD shaking / VS22 build / 旧 PR） |
| `SSS22` | 单个 “SSS22 fix”，无上下文，已被后续 SSS 线覆盖 |
| `vs2022` / `vs2022-DLTX` / `vs2022-vanilla` | 2023~2024 的构建配置分支，均被现主线吸收或过期 |
| `my_vs2022_rockets` / `sun_dir` / `BaS` | 2022~2023 的单点功能，已在主线/被取代 |
| `ubgl-grenade-class` | 新增 `Add(Remove)AmmoClass/GrenadeClass` Lua API；已检索整合包脚本，**没有任何调用**，不需要为它加 API |
| `hozar2002` / `all-in-one-vs2022-153-merge` / `master` / `BaS_NEW` | 已在 A 组（零 ahead） |

## 3. `upstream/pr` 逐项：21 个真实提交

判定口径：先看“我方代码是否仍有这个问题”，再看补丁能否干净应用，最后看是否值得为它做适配。

| # | 提交 | 内容 | 我方现状 | 建议 | 适配成本 |
|---|---|---|---|---|---|
| 1 | `0bd86fc17` | `CBaseGraviZone::exit_Zone`：非生物离开重力异常时停掉 telekinesis/粒子，治“物体悬浮不落” | 无 `exit_Zone` 覆写（基类虚函数在） | **搬** | 直接 apply，零冲突 |
| 2 | `c8baff0f9` + `427b6981d` | 幻影（Phantom）在 `stBirth` 动画中被打不死；最终形态=只在 `Hit()` 允许 `stBirth→stShoot` | `Hit()` 只判断 `stFly`，问题在 | **搬**（只取最终一行，不要两个 commit 全搬） | 手工 1 行 |
| 3 | `4f7fa5b4e` | `CCustomMonster::net_Relevant()`：`NET` 在 `net_Spawn` 末尾填充前不导出（Azazel 网络崩溃） | 无覆写；`NET` 成员在 | 可选搬 | 直接 apply，零冲突 |
| 4 | `7b6d5549d` | `CUIFixedScrollBar` 捕获对象从 `m_ScrollBox` 改为滚动条自身（鼠标拖动错位） | 旧代码在 | **搬** | 直接 apply |
| 5 | `15d42e813` | `CUIScrollView` 只在空白区按下才拖拽 pad，避免点中列表项也滚动 | 无 `m_pad_drag_active` | **搬** | 直接 apply |
| 6 | `801b939f0` | 纹理创建失败时打印 `HRESULT` + `GetDeviceRemovedReason()` | 已有 `_hr` + `texture_gpu_lock`，但没有设备移除原因日志 | **搬日志部分**（保留我们的锁） | 手工 ~5 行 |
| 7 | `a2021988e` | 删掉退出时不对称的 `Pause(TRUE…)` 和 DEBUG `CheckPrivilegySlowdown` | 两段都在 | 低优先/可不动；不是已证实的崩溃源，且 `x_ray.cpp` 有冲突 | 手工 |
| 8 | `98df21101` | camattach FOV | **已有 MT 版**（`set_cam_attached_fov` / PR #773） | 跳过 | — |
| 9 | `1ba7dc959` | PDA 排名跳过“id 已属于非 stalker”的行 | 源文件 `gamedata/scripts/aaaa_script_fixes_mp.script` 已入库，补丁干净；但部署走 `db/mods/00_modded_exes_gamedata.db0`，当前 db0 解包副本**没有**这层保护 | **值得搬**：改源码后重打 db0 | 脚本 + 打包 |
| 10 | `2669ff5f1` / `cfdb8faa9` / `f80f99b2c` | 从 .ltx 读环境 modifier + `env_mods_reload` 命令 + 文档/模板 | 无 | 功能项，暂不搬（等主线合并） | 中 |
| 11 | `36ac5b9b3` / `6b4906da7` / `626b0b95e` / `17f4c27bd` | 加载画面按目的地显示 artwork，可开关 | 无 | 功能项，暂不搬；与 gamedata/测试脚本绑定 | 大 |
| 12 | `0eca23321` | 怪物逐对象移动目的地覆盖（8 文件 Lua API） | 无 | 功能项，暂不搬；整合包未调用 | 中 |
| 13 | `4f7fa5b4e` 之外 | 其余 merge（#742/#748/#757/#758/#760/#763/#764/#771 等） | 对应真实提交已在表中 | — | — |

**建议先做的一批（Batch A）**：#1、#2、#4、#5、#6（+ 可选 #3）。全部是“现状缺、补丁小、可测试”的修复；#9 先查脚本，#7 观察即可。

## 4. 草/细节实例化：值不值得动？

### 4.1 为什么它是最高优先级

性能审计的锚点：几何 ~9M verts，**~85% 来自 flora**；DLSS 只带来 ~1.5×。也就是说瓶颈在“草的顶点/批次数”，不在像素。PR #577 正好改这一层：

- DX11 下新增 per-instance vertex buffer（stride 64B，容量 2^18），一次填充、按区间重复绘制；
- `UpdateVisibleM` 增加**距离密度曲线**（knee/curve）和稳定哈希采样，远处稀疏化；
- 每个对象的可见 slot 按距离**前到后排序**，帮助 early-Z；
- 新增 `r__Detail_density_knee` / `r__Detail_density_curve` 控制项。

### 4.2 为什么不能直接进发布

1. **未合并**：PR #577 仍是 open（2026-09-01 后未更新），上游主线没有它；`grass-instancing-2` 也是实验分支。
2. **需要配套 shader**：PR 描述附带 `grass shaders.zip`（5 个文件：`deffer_grass.vs`、`deffer_grass_bb.vs`、`deffer_detail_{s,w}_flat.vs`、`details_billboard.s`）。我们载荷里的 `deffer_grass.vs` 是 **5710B 的旧常量数组版**，PR 版是 **7592B 的 instance 版**；直接替换会丢掉/改写我们现有的 SSFX/MT 改动（`wave_prev`、`dir2D_prev`、motion vectors、fade 参数等需要逐项核对）。
3. **源码冲突**：`git merge-tree HEAD tmp-pr577` 在 `src/Layers/xrRenderDX10/dx10DetailManager_VS.cpp` 有 content conflict；`DetailManager.h` 还会碰到我们最近加的 `pool_mutex`（自动合并但语义要复核）。
4. **历史包袱**：`grass-instancing-2` 里还夹带了 `m_objectsShared`（已被我们 `1b525d33a` revert）和 `cNameVisual` 自旋等待（已被 `1c0c5b5a0` revert，且当时就是“修 eye_pp_s2 崩溃”的反向修复）。**这些 commit 一个都不要 cherry-pick。**
5. **视觉风险**：上游对草/细节做过“优化→部分 revert”的历史（STATUS §8 已记）；需要实机看闪烁、密度跳变、阴影/风动。

### 4.3 建议的验收方式

1. 独立 worktree（如 `exp/grass-instancing`），merge PR #577 + 移植 shader；
2. DX11-AVX 构建，89 tests 先过；
3. 固定存档/固定视角，记录：GPU frame time、`r__detail_radius` 50/100/200 三档、草的 draw call/顶点数、`r__Detail_density_knee/curve` 默认值；
4. 目标不是“FPS 好看”，而是 **同一画质下 GPU 时间下降 ≥15% 且无闪烁/穿帮**；达不到就只留档，不进发布。

## 5. 其它开放 PR 的价值排序

| PR | 规模 / 基线 | 判断 |
|---|---|---|
| **#777** `faster-animation-loading-mt`（shared mem 哈希表） | 2 文件 +37/−65，基线正好是我们的 `f768576c0`，merge-tree 干净 | **值得试**：`smem_container` 从有序 vector 插入 O(n) 改 `xr_unordered_map` O(1)，直接减少模型/动画加载期的内存搬移；风险中等（容器语义要跑长时+读档） |
| **#765** `rain-wetness-fix` | 3 文件 +13/−2，干净 | **值得搬**：湿度按 `Device.fTimeDelta` 累积；当前高 FPS 下干得快 2.4×，与 DLSS 高帧率场景直接相关；顺带加 `level.set_rain_wetness` |
| #368 `localization-mav-fix` | 1 文件 +27/−1，draft，mergeable=false，2025-12 | 我们跑中文包，值得**先复现 #367**；它是 `ui_options.script` monkey-patch，需核对 Anthology 是否已带同样修复 |
| #779 `profiling`（optick + 游戏内 profiler） | 220 文件 +3439/−556，MT | 对下一轮性能定位有用，但体量大、vendor 改动多；等主线收编再做 |
| #775 `shotgun reload` | 2 文件 +41/−33，MT 基线 | 纯玩法修复；玩家报“霰弹枪换弹中断”时再取 |
| #762 `shniaga-modal` | 6 文件 +37/−7 | 主菜单 UI 修复；低优先 |
| #691 `oneTBB` | 229 文件 +36161/−45469 | 调度器/分配器整体替换，风险极高，不碰 |
| #615 `PiP` / #548 `MT-SOUND` / #697 `scriptable UI` / #751 `PDA map API` 等 | 大型功能/实验 | 我们有自己的 PiP 线；其余等主线或按 mod 需求再评估 |

## 6. 当前项目状态（与本次对比相关的部分）

- `HEAD = bedb0c87c`（v0.2.2 六项 cherry-pick + 适配 + 文档），`origin` 已同步（`git rev-list --count origin/…mt..HEAD = 0`），工作树仅 `dist/MT-CN.rar` 未跟踪。
- 回归：`src\tests\run_tests.bat` → **89 checks, 0 failures**。
- 最新构建：`_build\_game\bin_dbg\AnomalyDX11AVX.exe`，28,658,688 B，SHA256 `4808FD08…`（2026-10-24 14:46）。
- **部署状态需注意**：`MODS\MT-CN\bin\AnomalyDX11AVX.exe` 已是 `4808FD08…`；但游戏 `bin\AnomalyDX11AVX.exe` 仍是 48,067,584 B / 2026-10-03 / `179F1D0B…`，且 `bin\nvngx_dlss.dll` 是 29,981,808 B 的旧版。最后一次 13:13 的日志显示当时跑的是 `Modded Exes MT-TEST version 2026.10.24`，说明 **13:15 之后 JSGME 把 MT-CN 停用/还原了**，而 14:46 的新构建还没有重新挂载。下次实机前需在 JSGME 停用→启用并清一次着色器缓存。
- 已知开放项（STATUS §4）不变：热成像光圈伪影、`scope_lense_enabled` 政策解析、MT 读档崩溃率观察、FDDA 外骨骼屏确认、Tier-1 A/B 数据、imgui 字体资源化。

## 7. 建议的批次

| 批次 | 内容 | 前置/验收 |
|---|---|---|
| **A（安全修复）** | GraviZone `exit_Zone`、Phantom 出生期受击、UI 滚动条两连、纹理设备移除原因日志、可选 `net_Relevant` | 直接 apply + 手工适配；89 tests；DX11-AVX 构建；实机跑一圈异常/UI/读档 |
| **B（正确性 + 加载）** | PR #765 雨湿度、PR #777 shared-mem 哈希 | B1 先单独构建测读档/长时；B2 记录同一存档加载耗时（冷/热） |
| **C（性能实验）** | PR #577 + shader 移植 | 独立 worktree；A/B；≥15% GPU 时间收益且无闪烁才考虑进发布 |
| **D（等待）** | `pr` 功能项、#779 profiler、主线下一批 | 作者把 `pr` 回灌 `all-in-one-vs2022-wpo-mt` 后按现有策略 fetch+merge |
