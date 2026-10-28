# 第三方代码出处台账（本分支自建以来）

> 触发条件：`License.txt` 首句声明"随附源码除另有声明外均为 GSC 商业专有代码"，所以**凡是从别处取来的代码，必须在仓库里另行声明**。本文件就是这份"另有声明"。
> 核查方法见文末；表中每条差异数字与头部命中数都是本次实测（md5 / grep -c / git log -S），子代理的结论凡被我推翻的都在 §4 列明。

## 1. 经 OGSR / OpenXRay 进入本分支的代码

这些都是**上游作者自己的提交**，被 cherry-pick 进本分支时保留了原作者名（`git log --format='%h %an'`可见），出处在 git 层是完整的。

**本分支的处置**：这些文件的**代码内不另加出处注释**。原因是本分支要持续与上游合并，往他人代码里插注释会在每次合并时制造无谓冲突；追溯入口就是本表 + 各提交的 message（都带源 URL）。若将来某个文件被我们改写（不再是原样承接），届时该文件内必须自带出处行。

| 本分支提交 | 原作者 | 其声明的源 | 落点 |
|---|---|---|---|
| `374947615` fix floating artefacts | lulnope | `github.com/OGSR/OGSR-Engine/commit/69433fa11a274537c2d82bfbd4e71f5eed1bab35`（原文写"shameless copy of"） | `src/xrGame/GraviZone.cpp` 的 `CBaseGraviZone::exit_Zone` +16 行、`GraviZone.h` 一行虚函数 |
| `46fd396eb` / `7f0c6b54e` Fix npc footsteps | OneMorePseudoCoder | `github.com/OGSR/OGSR-Engine/commit/ec6396cb9d93c63450ca76bad87f0315bed3ae9a`（"Отсюда:"） | `src/xrGame/PHMovementControl.cpp/.h`（该提交 -451/+207）、`material_manager.cpp` |
| `6fdad0a74` Lucy 脚本化调试渲染器 | DEMONIZED | 3 个 OpenXRay commit（`752cfddc`、`a1e58128`、`c17a38ab`）+ OGSR `d359bde2`，message 写 "Many thanks to OpenXRay and OGSR authors" | 41 文件（含 `script_fmatrix_script.cpp` 等） |
| `fbff6e64e` / `feac7dd7f` | MFB | "Ported a commit from OpenXRay"（smart cover 进出距离） | `smart_cover_object.cpp/.h` |
| `440495c79` shader cache 源 CRC | DEMONIZED | "inspired by openxray"（**启发，非拷贝**） | `src/xrCore/ShaderSourceCRC.h` 等 |

**许可注意**：GitHub API 对 `OGSR/OGSR-Engine` 返回 `license: "Other"`、`spdx_id: "NOASSERTION"`（非标准 OSI 许可）。因此这些代码的转载**不因"注明出处"就自动免责**——上游仓库自行把它们合入 Monolith，本分支只是承接；若将来要对外分发本仓库，需要单独确认 OGSR 侧条款。本文件把这点写在这里而不是假装解决。

## 1A. 上游 Monolith 的承接提交（作者名在 git 里保留，落地人为本分支）

本分支上「作者是他人的、由我们落地（committer=qy0214）」的提交共 **11 条**。逐条把落地板与源板（`(cherry picked from commit …)`）的 +/- 行做归一化 md5 比对，结果分两类：

| 本分支提交 | 作者 | 源提交 | 比对结果 |
|---|---|---|---|
| `74fcc33fe` Initial azazel net fix commit | vacwave | 无 cherry-pick 行（随上游分支合入） | 一致 |
| `8c0765647` UIScrollView 拖拽判定 | Burke T | 同上 | 一致 |
| `d0c52dd1b` fix(ui) 滚动条捕获 | erepb | 同上 | 一致 |
| `374947615` fix floating artefacts | lulnope | `69433fa1`（**OGSR-Engine**，见 §1） | 一致 |
| `9d7126fc4` Fix race condition in texture loading | Chad | `v0.2.2` 内 | 一致 |
| `a3456c91d` Fix wallmarks crash | Chad | `v0.2.2` 内 | 一致 |
| `8bc95cd7f` Implement map caching (PDA stutters) | Chad | `3aaf2204…` | **改写**：仅多一行标记 `// upstream v0.2.2: PDA map caching (fixes map stutters)` |
| `19b19090a` move discord rpc off main thread | Chad | `5a95f84a…` | **改写**：include 结构不同（我方需 `#include <thread>`，不带 `pch_script.h`），并保留本分支的 Azazel 注释行 |
| `cb445886f` mutex for item destruction/insertion | Chad | `14ed5b3b…` | **改写**：我方额外补了 items 清理循环（`poolSI.destroy(D->G[i].items[clr])`）与 `xrCriticalSectionGuard lock(pool_mutex)` 的作用域 |
| `468e9bdc4` checks for invalidated wallmarks | Chad | `f3c84df8…` | **改写**：我方多了 `StateDecs desc;`、少了源板的 `continue;`（分支结构按本仓库代码重排） |
| `00c4c0897` Fix nullptr crash in Inventory::DropItem | Chad | `78a57161…` | **改写**：保留本分支的抛物实现（owner 判空、`cam_Active()->vDirection`、速度取水平模长），并把 `H_SetParent` 扩到所有掉落物；文件内有注释说明 |

**读这些文件时要注意**：git 的 author 是上游作者，容易让人以为内容也是原样。凡标「改写」的 5 条，代码里混有本分支的改动；这 5 条的源哈希都在提交 message 里，对照方法见 §5。

另外：`git log HEAD --no-merges` 中「作者与 committer 都不是 qy0214」的上游提交共 **4095** 条，属基座谱系（Anomaly 2019 → Monolith 各分支），由继承与一次我们自己做的 merge（`058c5d26d`，2026-10-24，零冲突）带入。这部分的出处 = 上游仓库与 git 作者名，不逐条登记。

## 2. Vendored SDK

| 目录 | 许可 | 版本与派生关系（实测） |
|---|---|---|
| `sdk/DLSS/`（NGX 头 + `nvsdk_ngx_d.lib`） | **NVIDIA RTX SDKs LICENSE**（`LICENSE.txt`，日期行 "v. March 14, 2024"）；头文件内 `SPDX-License-Identifier: LicenseRef-NvidiaProprietary`、`Copyright (c) 2018-2023 NVIDIA CORPORATION & AFFILIATES` | SDK 包版本号未随文件提供（头文件里只有 `NVSDK_NGX_VERSION_API_MACRO 0x0000015`）；可辨识信息为：版权年份 2018-2023、许可文本日期 "v. March 14, 2024"、配套运行时 `nvngx_dlss.dll` 文件版本 **310.9.1.0**。许可条款实核：§2a 禁止移除版权/专有声明，§7 允许 SDK 内含第三方组件的独立声明——要求是**别剥离**，不是"必须新增致谢" |
| `sdk/FSR3/`（463 文件） | **AMD MIT**（`LICENSE.txt`，`Copyright © 2023 Advanced Micro Devices, Inc.`）+ `Third_party_notices.txt`（MIT 声明），两者完整保留 | **不是 AMD 官方 SDK tag**：`README.txt:4` 自述来自 OGSR 的 `3rd_party/Src/FidelityFX-SDK`，OGSR checkout `8adb7cf1650120987d84fcc8be7e73ad8a6f9945`，并明写"community D3D11 backend…not AMD's officially supported DX12/Vulkan backend"。API 头标识 upscaler = 3.1.2 |
| `dist/MT-CN/bin/` 三个 dll（磁盘有、不入库） | 随 NVIDIA / AMD SDK | 指纹见 `dist/MT-CN/来源与要求.md` §一：`nvngx_dlss.dll` 58,956,912 B `3975567b8943…`（DLSS 310.9.1.0）、`ffx_backend_dx11_x64.dll` 8,425,984 B `b9eea14fe044…`、`ffx_fsr3upscaler_x64.dll` 44,032 B `d2a08178c8f3…` |

**另外三处涉及他人代码、但都不是本分支引入的**：

| 项 | 事实 |
|---|---|
| `src/3rd party/optick-git` | **Git 子模块**，`url = github.com/bombomby/optick.git`，pin 在 `8abd28dee`（`1.4.0.0-9`）。`.gitmodules` 由上游 ProfLander 于 2025-06-19 加入（“Integrate optick profiler”）；本分支只在 CI 里 `submodules: true` 拉取（`b0a35975`），许可随子模块自带 |
| `.github/workflows/*` | 用到的第三方 Actions 全部由上游引入（Dmitry Chernyavsky）：`microsoft/setup-msbuild@v1.3.1`、`al-cheb/configure-pagefile-action@v1.3`、`c-hive/gha-remove-artifacts@v1.2.0`、`softprops/action-gh-release@v2.3.2`、`actions/checkout@v4`、`actions/upload-artifact@v4.6.0`、`actions/download-artifact@v4.1.8`。本分支的 CI 提交（`60862562` 独立 release job、`4ae75fd2` 测试 job、`3943bd1b` 构建脚本）**只复用**这些 action，没有引入新的他人 Action |
| `src/3rd party/**`、`sdk/include`、`sdk/binaries`、`sdk/libraries`、`sdk/autoexp.dat`、`compressor/` | 最早加入者：Anomaly（2019-08-26）与 DEMONIZED（2024-09-25）——**基座自带**，本分支未向其中新增文件（`sdk/DLSS`、`sdk/FSR3` 除外，见上表）。`sdk/autoexp.dat` 自带 “Copyright(c) Microsoft Corporation” |
## 3. 派生自整合包里他人 MOD 的文件（`dist/MT-CN/`）

基线路径缩写：`SSS` = `mods\[GFX] 屏幕空间着色器 23.5 — ScreenSpaceShaders Update 23.5 — Обновление шейдеров`（作者 **ascii1457**，moddb：`.../mods/stalker-anomaly/addons/screen-space-shaders`）；`WD` = `mods\[GAM] 可穿戴设备 — Wearable Devices`；`NV` = `mods\[GFX] 夜视仪着色器 — Beefs NVGs Shaders`；`汉化` = `mods\[汉化] 汉化整合包 — A.N.T.H.O.L.O.G.Y 汉化整合包`；`ROOT` = 合并后的 `Anomaly-1.5.3-Anthology 2.1\gamedata`。

**表 A：着色器（差异行数 = 我们副本 vs 该 mod 原件）**

| 我们的文件 | 基线 | 差异 | 原件作者头是否保留 |
|---|---|---|---|
| `shaders/r3/screenspace_mvectors.h` | SSS | 19 | ✓ `@ Author: …ascii1457`；我们只加 `#include "dlss_contract.h"` |
| `shaders/r3/ssfx_ao.ps` / `ssfx_il.ps` / `ssfx_ssr.ps` / `ssfx_sss.ps` / `ssfx_sss_ext.ps` / `ssfx_water_ssr.ps` | SSS | 33 / 15 / 27 / 9 / 13 / 27 | ✓ 六个都保留 `ascii1457` 头（各 +1～4 行 DLSS 相关） |
| `shaders/r3/deffer_grass.vs` | SSS | 73（版本差） | **原件本就无 `@ Author` 头**（实测 orig=0），行内 2 处 moddb 链接与原件一致 |
| `shaders/r3/deffer_impl_flat.ps` | SSS | 267（版本差） | 同上（原件无头，1 处 moddb 链接保留） |
| `shaders/r3/night_vision.h` | NV（区分：仓库根 `gamedata/shaders/r3/night_vision.h` 是**上游自带**那份，来自 `29633bd1` DEMONIZED 2024-09-25 “gamedata reorganize”；dist 里这份才是我们部署的 Beefs 改版） | 199（不同 Beefs 版本） | ✓ `BEEF'S SHADER BASED NIGHT VISION EFFECT` + 致谢 LVutner / Sky4Ace / Meltac |
| `shaders/r3/wd_body.vs` | WD | 9 | ✓ 首行点名 WearableDevices |
| `shaders/r3/csm_sunmask.ps`、`dlss_*.h/.ps`、`upscaler_camera_mv.ps` | 无（本仓库原创） | — | 本仓库产物；`dlss_cas.h:1` 另声明"CAS adapted from OGSR's postprocess_cas.ps, based on AMD's CAS presentation"+GPUOpen 链接 |

**表 B：脚本与文本**

| 我们的文件 | 基线 | 状态 |
|---|---|---|
| `scripts/d_promin_health_ui.script` | WD 同名文件 | 编辑 ~45 行；首行点名 `[GAM] 可穿戴设备 — Wearable Devices` ✓ |
| `scripts/wd_exo.script` | **ROOT** 的 `wd_exo.script`（md5 `b2ab76be…`）；WD mod 目录那份与 ROOT 差 14 行 | 本次补声明（基线取 ROOT 而非 mod 目录，原作者未知） |
| `scripts/fix_fast_travel_markers.script` | 汉化 包同名脚本 | 该原件**不声明作者**，无法向上归属；本次补声明：只有 `is_travel_target` 的 clsid 兜底是我们的改动 |
| `scripts/modxml_dlss_options.script`、`zzz_mtcn_wd_i18n.script`、`zzz_mtcn_dbg_launcher_i18n.script`、`zzz_mtcn_limb_mirror.script`、`configs/text/chs/st_mtcn_i18n.xml` | 本仓库原创，但**包裹/引用他人模块** | 文件内已点名被包装对象（`wd_gui`、Banjaji 的 `ui_debug_launcher.script`、BHS `zzz_player_injuries.script:432/630/225/257`、`haru_quick_action_wheel_mcm.script:1844` 等）✓ |
| `configs/text/eng/st_monolith_dlss.xml` | 本仓库原创 | 与 ROOT、汉化 包内三份 md5 全等（`a87d5ba1…`，加注释后）；本次补声明 |
| `db/mods/00_modded_exes_gamedata.db0` | 上游 `compressor/` 工具产出 | `compressor/! compress_gamedata.cmd:2` = `xrCompress.exe gamedata -ltx build_patch.ltx -pack -strong -1024 -db`；工具属上游（DEMONIZED，2024-09-25），本分支只是归档其产物（`474c85ab`，md5 `b8bb004e`，与线上 `db\mods\` 同一份） |

## 4. 本次核查中被撤回的说法

- ❌ "`deffer_grass.vs` / `deffer_impl_flat.ps` 被我们删掉了 `@ Author: ascii1457` 块" → 实测 SSS 原件 `grep -ci "@ author|ascii1457|SCREEN SPACE SHADERS"` = **0**，我们副本同为 0，且两边 moddb 链接数相同（2 / 1）。不是剥离，是原件本来就没有。
- ❌ "`r4.cpp:524` 的 `o.ssfx_motionvectors` 探针是我们加的、需要注明 SSFX" → `git log -S ssfx_motionvectors` 显示出自上游 `5cdf9bdcc`/`bf55e483d`（DEMONIZED，"SSS 23 Update"，2025-06-28）。不是我们的代码，未加声明。
- ⚠️ 子代理称"NVIDIA 许可要求随软件添加'含 NVIDIA 提供源码'的致谢" → 我在 `sdk/DLSS/LICENSE.txt` 实际核到的是 §2a（禁止移除声明）与 §7（第三方组件可带独立声明），未见强制新增致谢的条款。

## 5. 复查步骤（任何人可重跑）

```bash
# ① 我们自己的提交里，别人代码的出处
git log HEAD --format='%h|%an|%s' | grep -iE 'ogsr|openxray'
git log -S '<symbol>' --format='%h %an %ad %s' --date=short -- <file>   # 某段代码是谁引入的
# ② dist 与线上 MO2 是否仍全等（mod 层是单一来源）
md5sum dist/MT-CN/gamedata/<f> "…/Modpack-…/mods/MT-CN/gamedata/<f>"
# ③ 派生关系：差异行数
diff <(tr -d '\r' < 我们副本) <(tr -d '\r' < mod原件) | grep -c '^[<>]'
# ④ 作者头是否还在
grep -c 'ascii1457' dist/MT-CN/gamedata/shaders/r3/ssfx_*.ps
# ⑤ SDK 派生与许可
head -12 sdk/FSR3/README.txt ; grep -n 'SPDX\|Copyright' sdk/DLSS/include/nvsdk_ngx.h | head
```

**维护约定**：以后新增任何取自他处的代码，落库前做两件事——(a) 在本文件对应表加一行（作者/URL 或 commit 哈希/差异规模），(b) 在代码文件里留一行同样信息。只写在 commit message 里不算完成，因为文件会被单独读。
