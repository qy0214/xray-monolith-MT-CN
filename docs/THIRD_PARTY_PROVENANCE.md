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

## 2. Vendored SDK

| 目录 | 许可 | 版本与派生关系（实测） |
|---|---|---|
| `sdk/DLSS/`（NGX 头 + `nvsdk_ngx_d.lib`） | **NVIDIA RTX SDKs LICENSE**（`LICENSE.txt`，日期行 "v. March 14, 2024"）；头文件内 `SPDX-License-Identifier: LicenseRef-NvidiaProprietary`、`Copyright (c) 2018-2023 NVIDIA CORPORATION & AFFILIATES` | NGX 具体版本号未登记（**待补**）。许可条款实核：§2a 禁止移除版权/专有声明，§7 允许 SDK 内含第三方组件的独立声明——要求是**别剥离**，不是"必须新增致谢" |
| `sdk/FSR3/`（463 文件） | **AMD MIT**（`LICENSE.txt`，`Copyright © 2023 Advanced Micro Devices, Inc.`）+ `Third_party_notices.txt`（MIT 声明），两者完整保留 | **不是 AMD 官方 SDK tag**：`README.txt:4` 自述来自 OGSR 的 `3rd_party/Src/FidelityFX-SDK`，OGSR checkout `8adb7cf1650120987d84fcc8be7e73ad8a6f9945`，并明写"community D3D11 backend…not AMD's officially supported DX12/Vulkan backend"。API 头标识 upscaler = 3.1.2 |
| `dist/MT-CN/bin/` 三个 dll（磁盘有、不入库） | 随 NVIDIA / AMD SDK | 指纹见 `dist/MT-CN/来源与要求.md` §一：`nvngx_dlss.dll` 58,956,912 B `3975567b8943…`（DLSS 310.9.1.0）、`ffx_backend_dx11_x64.dll` 8,425,984 B `b9eea14fe044…`、`ffx_fsr3upscaler_x64.dll` 44,032 B `d2a08178c8f3…` |

## 3. 派生自整合包里他人 MOD 的文件（`dist/MT-CN/`）

基线路径缩写：`SSS` = `mods\[GFX] 屏幕空间着色器 23.5 — ScreenSpaceShaders Update 23.5 — Обновление шейдеров`（作者 **ascii1457**，moddb：`.../mods/stalker-anomaly/addons/screen-space-shaders`）；`WD` = `mods\[GAM] 可穿戴设备 — Wearable Devices`；`NV` = `mods\[GFX] 夜视仪着色器 — Beefs NVGs Shaders`；`汉化` = `mods\[汉化] 汉化整合包 — A.N.T.H.O.L.O.G.Y 汉化整合包`；`ROOT` = 合并后的 `Anomaly-1.5.3-Anthology 2.1\gamedata`。

**表 A：着色器（差异行数 = 我们副本 vs 该 mod 原件）**

| 我们的文件 | 基线 | 差异 | 原件作者头是否保留 |
|---|---|---|---|
| `shaders/r3/screenspace_mvectors.h` | SSS | 19 | ✓ `@ Author: …ascii1457`；我们只加 `#include "dlss_contract.h"` |
| `shaders/r3/ssfx_ao.ps` / `ssfx_il.ps` / `ssfx_ssr.ps` / `ssfx_sss.ps` / `ssfx_sss_ext.ps` / `ssfx_water_ssr.ps` | SSS | 33 / 15 / 27 / 9 / 13 / 27 | ✓ 六个都保留 `ascii1457` 头（各 +1～4 行 DLSS 相关） |
| `shaders/r3/deffer_grass.vs` | SSS | 73（版本差） | **原件本就无 `@ Author` 头**（实测 orig=0），行内 2 处 moddb 链接与原件一致 |
| `shaders/r3/deffer_impl_flat.ps` | SSS | 267（版本差） | 同上（原件无头，1 处 moddb 链接保留） |
| `shaders/r3/night_vision.h` | NV | 199（不同 Beefs 版本） | ✓ `BEEF'S SHADER BASED NIGHT VISION EFFECT` + 致谢 LVutner / Sky4Ace / Meltac |
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
| `db/mods/00_modded_exes_gamedata.db0` | 本仓库 compressor 产物 | 生成方式**待补**（本次未核） |

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
