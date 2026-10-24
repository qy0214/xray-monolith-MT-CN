# DLSS/FSR3 + MOD 名称兼容 + PiP(3DSS) 契约 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: 用 执行实施方案 逐任务实施本计划（本计划由人类同伴选择"本会话内联执行"）。步骤用 `- [ ]` 勾选跟踪。

**Goal:** 在不修改整合包任何文件的前提下，让我们的 MT 分支引擎支持整合包 MOD 期望的 DLSS/FSR3 与 PiP(3DSS) 接口，消除 MCM 打开即崩的缺陷。

**Architecture:**
1. 引擎内部保留 pipdlss 的**原始 cvar 名**（`r_upscaler` / `r_dlss_quality` / `r_fsr3_quality` / `r__svp_*` 等），把其 `Dlss/*`、SDK、着色器契约移植进我们仓库。
2. 另加一层**兼容别名**：注册整合包 MCM 使用的名字（`r4_upscaler*`、`scope_lense_*`），把值翻译/同步到原始 cvar 或引擎既有状态（SecondVP / `scope_*`）上。
3. PiP 不整搬 pipdlss 的 470 KB 光学子系统：整合包的 PiP 是**着色器契约**（MOD 自带 `models_scope_reticle.ps` 等，向引擎索取 `scope_lense_*` 常量/纹理），我们只实现这套契约 + 引擎既有的 SecondVP 渲染。

**Tech Stack:** C++17 / MSBuild VS2022 / DirectX 11 / NVSDK NGX（DLSS）+ FidelityFX FSR3（静态库） / xray-monolith MT 渲染管线（R4）。

**Spec（真值来源，实施时逐一阅读）:**
- pipdlss 参考树：`C:\game\xray-monolith-pipdlss` @ `dc43c41f`（功能提交：`c0bfc688` `091b4147` `41ab6a53` `a6979bb3` `30ef9ac3` `0e8f45b6` `1b3b00d5`）
- pipdlss 接口文档：`tools/dlss/README.txt`（13.9 KB，含 `nvngx_dlss.dll` 310.9.1 的 URL+SHA256、pass 顺序、SSS/jitter 契约）
- 整合包 MCM 契约：`C:\game\ANTHOLOGY\translation_audit\db_root_extract_scripts\scripts\anthology_upscaler_mcm.script`
- 整合包 PiP MCM：`...\mods\[WPN][1.1][SCP] R.A.K …PiP 3DSS…\gamedata\scripts\rak_pip_quality_mcm.script`
- 整合包 PiP 着色器契约：同 mod 的 `gamedata\shaders\r4\{models_scope_reticle.ps, models_scope_reticle_precise.ps, anthology_pip_temporal.h, anthology_pip_motion_sample.h, svp_quality.ps, svp_motion_map.ps, svp_motion_depth.ps}`
- 桥接脚本：同 mod 的 `gamedata\scripts\zzz_anthology_pip_device_bridge.script`

## Global Constraints

- **不改 MOD**：一切交付物只进 `MODS\MT-CN`（MO2 覆盖）与我们仓库 `C:\game\xray-monolith-mt`；绝不修改 `Modpack-1.5.3-Anthology 2.1\mods\**`。
- **只保留原始名 + 兼容别名**：不得重命名 pipdlss 的 `r_upscaler`/`r_dlss_quality`/`r_fsr3_quality`/`r_dlss_sharpness`/`r__svp_*`。
- **cvar 名/类型/范围必须与 MCM 声明逐字一致**，否则 MCM `clamp(nil/mismatch)` 会再次 FATAL。
- **提交需用户明确批准**；默认只改本地工作区，不 push、不 commit。
- 分发引擎时必须**清空** `appdata\shaders_cache`（着色器缓存与引擎/着色器包绑定）。
- 迭代只验证 `DX11|x64`（AVX）一个配置；最终打包再出全配置。
- 引擎构建命令：`msbuild /p:Configuration=DX11-AVX src\engine-vs2022.sln`（**解决方案级**配置名是 `DX8/DX9/DX10/DX11` 与 `*-AVX`；`Release-AVX` 会被静默映射成 DX9，务必用 `DX11-AVX`）。产物 `_build\_game\bin_dbg\AnomalyDX11AVX.exe`，再拷到 `MODS\MT-CN\bin\`。
- 现状基线：`axr_options.ltx`/`user.ltx` 的 `scheduler_batch_size = 512`（已解决卡顿，勿回退）。

## 进度（2026-10-17）

- ✅ **Task 0 已完成**：备份到 `C:\game\ANTHOLOGY\_engine_backup_20261017_known_good`（`MODS_MT-CN` 整包 + 构建产物 + 512 配置快照 + 1365 个着色器缓存 + `MANIFEST.txt` 全 SHA256 + git 基线 a13b3d2a）。
- ✅ **Task 1 已完成并部署**：新增 `UpscalerCompat.h`，21 个 MOD 名 cvar 在 `xrRender_console.cpp` 内实现（不新建 .cpp 的理由见 Task 1）；`DX11-AVX` 构建 0 错误；21/21 名字经 exe 字符串校验通过；已部署 `MODS\MT-CN\bin\`，`MODS\MT-CN.sha256.txt` 已更新。**待用户在游戏内确认两页不再崩。**
- ✅ **Task 2 的每帧信号随同落地**：`r4_R_render.cpp` 在 `CRender::Render()` 里调用 `compat::Tick()` → `scope_lense_ads_is_pip` 跟随 SecondVP。
- ✅ **额外清点**：把整合包所有脚本/MCM 索取的命令名全扫一遍 → 除上面两页外**没有别的页面会崩**；`r4_snow_*`(13) 与 `r__flora_style` 有 MOD 侧守护（功能隐藏，不崩），见 Track D。
- ✅ **Task 3 已完成**：`Dlss/*`(8) + `sdk/DLSS`(16) + `sdk/FSR3`(463) + `xrRender/RenderDimensions.h` 全部**逐字节**移植（与 pipdlss 0 差异）；另需 `xrEngine/RendererError.h` 与 `Stats.cpp` 的 3 个 hunk（`SetRendererError`/`DrawRendererError`）也已移植；`xrRender_R4.vcxproj` 接线（Dlss 源、FSR3 include/lib、`fsr3_dx11.props`）完成；构建 0 错误。
  - **为保持 pipdlss 源码零改动**：在 `src/xrEngine/device.h` 补上 `CRenderDevice::true_pip_on` 与 `CSecondVPParams::m_render_pass_is_svp`（名字/类型照抄 pipdlss），每帧由本引擎 SecondVP 状态喂值（`r4_R_render.cpp`）。理由：pipdlss 的 `IsSVPFrame()` 是混合版，`true_pip_on == false` 时即本引擎行为。
- 🔵 **Task 4 进行中（batch 1 完成）**：R4 `stdafx.h` 包含 `Dlss/DlssIntegration.h`；`dx10HW.cpp` 放开 `D3D_FEATURE_LEVEL_11_1` + `E_INVALIDARG` 重试 + `dlss::ShutdownDevice()`；`blender_nightvision.cpp`(7 处)/`blender_gasmask_drops.cpp`/`blender_gasmask_dudv.cpp` 换用 `$user$dlss_post_source`。构建 0 错误，exe 24.7 → 28.2 MB。
  - **剩余**：`Blender_Recorder_StandartBinding.cpp`（jitter/MV/重投影常量）、`r__dsgraph_render.cpp`（native UI 抽取，v1 延后）、`r4_rendertarget.cpp/.h`（dlss RT 家族 + `CBlender_upscaler_camera_mv`）、`r4_R_render.cpp`（Begin/EndScene、SceneDepth 等）、`r4_rendertarget_phase_PP.cpp`（`dlss::Evaluate`）、`r4_rendertarget_phase_combine.cpp`/`phase_ssao.cpp`（`dlss::Configured()` 守卫）、`r4.cpp`。

### Task 4 批次 2/3（已完成，构建 0 错误）- `r4_rendertarget.h/.cpp`：5 个 DLSS blender（`dlss_postprocess`/`dlss_menu`/`dlss_copy`/`dlss_ui_depth`/`upscaler_camera_mv`）、`u_SceneDepth()`、各 `u_setrt` 的深度替换、构造/析构里的 `dlss::InitializeTargets/ReleaseTargets`、DLSS 的 RT/纹理/sampler 家族（显示分辨率）、菜单 shader 切换；**场景 RT 尺寸改为 `dlss::RenderWidth/Height`**（关时等于 `Device.dw*`，零行为变化）。
- `r4_rendertarget_phase_PP.cpp`：`PostprocessColor()`/`PostprocessSource()`、`phase_upscaler_camera_mv()`（相机 MV 回退）、`phase_dlss_postprocess()`（`dlss::Evaluate` + 解析 + 显示分辨率叠加层）、`phase_pp()` 接入。
- 共享叠加层 phase（`rendertarget_phase_gasmask_drops/dudv/nightvision`）：改为显示分辨率路由（`RenderScreenWidth/Height` + `PostprocessColor/Source`，按 `USE_DX11` 保护，R1/R2/R3 行为不变）。
- `r4_rendertarget_phase_combine.cpp`：叠加层与 SMAA 加 `!dlss::Configured()` / `!dlss::Active()` 守卫（启用 DLSS 时改由 post 阶段在显示分辨率下绘制）。

### Task 4 收尾（已完成，构建 0 错误）

- `r4_R_render.cpp`：`dlss::EndScene()` / `EndScene(true)` / `BeginScene()` 生命周期、菜单与无场景路径的 `RenderScreenWidth/Height` + `u_SceneDepth()`、汉化/夜视/3DSS 的深度来源改用 `u_SceneDepth()`、Bloom emissive 深度同样替换。
- `Blender_Recorder_StandartBinding.cpp`：`cl_texgen`/`cl_VPtexgen` 加入 DLSS 栅格抖动（未启用时抖动为 0，值不变）、`ssfx_jitter` 在 `dlss::Configured()` 时输出 `dlss::ClipJitter`、新增三个常量 setup（`dlss_params`/`dlss_temporal_jitter`/`m_upscaler_reproject`）并注册。
  - 适配点：`m_upscaler_reproject` 用 `RCache.xforms.m_vp_prev`（本引擎是单个矩阵；pipdlss 为 SVP 改成了 `[2]`）。
- `phase_scene`/`draw_rain`/`phase_ssao`/`r4.cpp`：场景侧尺寸统一改为 `RenderScreenWidth/Height`（= `dlss::RenderWidth/Height`，未启用时等于 `Device.dw*`）、`r4.cpp` 的投影常量加入抖动、`FluidManager` 用渲染尺寸。

### Task 5（已完成并部署）

- 下载 NVIDIA **nvngx_dlss.dll 310.9.1**（官方 raw 链接），SHA256 = `3975567B…563983` 与 `tools/dlss/README.txt` 记录**完全一致**。
- 部署进 `MODS\MT-CN`：`bin\AnomalyDX11AVX.exe`（28,293,632 B）、`bin\nvngx_dlss.dll`、`gamedata\shaders\r3\*`（16 个，含 `dlss_copy/sharpen/ui_depth/upscaler_camera_mv` + SSS/Atmospherics/NVG 的 MV 补丁）、`gamedata\scripts\modxml_dlss_options.script`（Video>Basic 的 Upscaling 页）、`gamedata\configs\text\eng\st_monolith_dlss.xml`。
- **已清空 `appdata\shaders_cache`**（1584 → 0）并更新 `MODS\MT-CN.sha256.txt`（22 条）。
- 待用户游戏内验证；**失败会以红字屏上警告 + 日志给出原因**（`SetRendererError` 已移植）。

## Review Focus

1. ∫ 打开 MCM 的 `anthology_upscaler` 页与 `rak_pip_quality` 页——**必须不再**出现 `_g.script:787 attempt to compare nil with number`（因任一 cvar 缺失/类型不符）。
2. `console:get_integer("scope_lense_ads_is_pip")` 在 ADS 一把 PiP 镜时必须为 1——否则 MOD 桥接逻辑把 PiP 当普通镜（HUD/设备行为错乱）。
3. 着色器包缺运动矢量时开启 DLSS——必须走 camera-MV 回退（`ps_r__upscaler_camera_mv`）而不是拖影/编译失败。
4. DLSS/FSR3 降分辨率渲染时，UI/HUD/后处理/夜视热成像——必须仍按显示分辨率合成（否则 UI 模糊、双影）。
5. DLSS 与 PiP（SecondVP）同帧共存——镜内 pass 必须绑定自己的深度/尺寸（否则镜内冻结或串图）。

---

## Task 0: 基线冻结与工作区记录

**Files:** 不新建源码；记录与备份。

- [ ] **Step 1: 记录基线**

```powershell
cd C:\game\xray-monolith-mt
git status --short            # 记录未提交的 9 处修复
git rev-parse HEAD            # 记录本分支提交
```
预期：`git status --short` 只列出我们自己的改动（`dx10HW.cpp`、`UIActorMenuInventory.cpp`、`UIPdaWnd.cpp`、`profiler.h`、`UIMotionIcon.cpp`、`Envelope.cpp`、3 处 `(unsigned)sizeof`、2 处 `sscanf_s`、`libjpeg` 工程）。

- [ ] **Step 2: 备份当前可用的部署包**

```powershell
$inst = "C:\game\ANTHOLOGY\Anomaly-1.5.3-Anthology 2.1"
Copy-Item "$inst\MODS\MT-CN\bin\AnomalyDX11AVX.exe" "C:\game\ANTHOLOGY\_engine_backup_20261016_2208\MT-CN_before_port.exe"
```
预期：备份文件存在且大小 ≈ 24,733,184 B。

---

## Task 1: 注册 MOD 名 cvar（消除 MCM FATAL）

**Files:**
- Create: `src/Layers/xrRender/UpscalerCompat.h`（仅声明；**不新建 .cpp** —— `xrRender_console.cpp` 被 R1/R2/R3/R4 四个工程共同编译，另开 .cpp 就得给四个工程都接线）
- Modify: `src/Layers/xrRender/xrRender_console.cpp`（命名空间实现 + 在 `xrRender_initconsole()` 末尾调用 `compat::RegisterConsoleVars()`）

**Interfaces:**
- `namespace compat { void RegisterConsoleVars(); void Tick(); }`，外加 21 个 `extern` 变量：`u32 r4_upscaler; u32 r4_upscaler_quality; float r4_upscaler_custom_scale; float r4_upscaler_sharpness; int scope_lense_live_effects; int scope_lense_temporal_mode; int scope_lense_quality_percent; int scope_lense_update_interval; float scope_lense_aim_sensitivity; int scope_lense_allow_nvg; int scope_lense_allow_thermal; float scope_lense_brightness; float scope_lense_contrast; float scope_lense_saturation; float scope_lense_gamma; float scope_lense_sharpness; float scope_lense_grain; float scope_lense_dirt; int scope_lense_tint; float scope_lense_tint_intensity; int scope_lense_ads_is_pip;`。Task 6/8 直接读这些 extern 变量。
- 注意：`CCC_Token` 构造签名是 `(LPCSTR, u32*, xr_token*)` → 两个 token 变量必须是 `u32`；其余 `CCC_Integer`/`CCC_Float` 用 `int*`/`float*`。

**cvar 表（名字/类型/范围/默认值必须逐字实现）:**

| cvar | 类型 | 范围 | 默认 |
|---|---|---|---|
| `r4_upscaler` | CCC_Token `{"off",0},{"fsr3",1},{"dlss",2}` | — | off |
| `r4_upscaler_quality` | CCC_Token `{"native",0},{"quality",1},{"balanced",2},{"performance",3},{"ultra_performance",4},{"custom",5}` | — | native |
| `r4_upscaler_custom_scale` | CCC_Float | 0.33–1.00 | 1.00 |
| `r4_upscaler_sharpness` | CCC_Float | 0–1 | 0.0 |
| `scope_lense_live_effects` | CCC_Integer | 0–1 | 1 |
| `scope_lense_temporal_mode` | CCC_Integer | 0–2 | 2 |
| `scope_lense_quality_percent` | CCC_Integer | 25–100 | 100 |
| `scope_lense_update_interval` | CCC_Integer | 1–8 | 2 |
| `scope_lense_aim_sensitivity` | CCC_Float | 0.25–2.0 | 1.0 |
| `scope_lense_allow_nvg` | CCC_Integer | 0–1 | 1 |
| `scope_lense_allow_thermal` | CCC_Integer | 0–1 | 1 |
| `scope_lense_brightness` | CCC_Float | 0.5–1.5 | 1.0 |
| `scope_lense_contrast` | CCC_Float | 0.5–1.5 | 1.0 |
| `scope_lense_saturation` | CCC_Float | 0.0–2.0 | 1.0 |
| `scope_lense_gamma` | CCC_Float | 0.5–1.5 | 1.0 |
| `scope_lense_sharpness` | CCC_Float | 0.0–2.0 | 1.0 |
| `scope_lense_grain` | CCC_Float | 0.0–1.0 | 0.0 |
| `scope_lense_dirt` | CCC_Float | 0.0–2.0 | 0.65 |
| `scope_lense_tint` | CCC_Integer | 0–4 | 0 |
| `scope_lense_tint_intensity` | CCC_Float | 0.0–1.0 | 0.3 |
| `scope_lense_ads_is_pip` | CCC_Integer | 0–1 | 0 |

- [ ] **Step 1: 写头文件接口**

`UpscalerCompat.h` 声明上面的 `compat` 命名空间函数（全部 `ECORE_API`，全部 inline 返回值，变量定义在 `.cpp`）。

- [ ] **Step 2: 实现注册**

`UpscalerCompat.cpp`：静态变量 + `xr_token` 数组 + `CMD3`/`CMD4` 注册 + getter；`RegisterConsoleVars()` 幂等（`static bool done`）。

- [ ] **Step 3: 接线与工程**

`xrRender_console.cpp` 末尾 `compat::RegisterConsoleVars();`；`xrRender.vcxproj` 增加 `UpscalerCompat.cpp/.h`。

- [ ] **Step 4: 构建**

```powershell
msbuild /p:Configuration=Release-AVX C:\game\xray-monolith-mt\src\engine-vs2022.sln
```
预期：`0 Error(s)`。

- [ ] **Step 5: 部署并验证不崩（用户执行）**

部署 exe 到 `MODS\MT-CN\bin\` → 启动 → 打开 MCM 的 `anthology_upscaler` 页与 `rak_pip_quality` 页。
预期：两页均正常显示；`appdata\logs\xray_admin.log` 中 `[SCRIPT ERROR]` 计数为 0；无新 `.mdmp`。

---

## Task 2: PiP 运行时真实信号

**Files:**
- Modify: `src/Layers/xrRender/UpscalerCompat.h`（`Tick()` 的语义注释）
- Modify: `src/Layers/xrRenderPC_R4/r4_R_render.cpp`（在 `CRender::Render()` 里 `rmNormal();` 之后调用 `compat::Tick()`）

**Interfaces:**
- Consumes: `compat::Tick()`（Task 1 已在 `xrRender_console.cpp` 中实现，改由这里每帧调用）。
- Produces: `scope_lense_ads_is_pip` 每帧等于 SecondVP 是否激活。

**为什么不在 `Weapon.cpp` 里推**：xrGame 与渲染器是两个 DLL，xrGame 不链接渲染器符号（只能经 xrEngine 的 `Device`/`Console`）。所以改为渲染器每帧轮询 `Device.m_SecondViewport.IsSVPActive()`。

- [ ] **Step 1: 挂上每帧调用**

调用点：`CRender::Render()` 开头、`rmNormal();` 之后一行 `compat::Tick();`。

- [ ] **Step 2: 构建 + 游戏内断言（用户执行）**

ADS 一把 `scope_lense_fov` 非零的镜；控制台 `scope_lense_ads_is_pip` 预期为 `1`；放开后为 `0`。

---

## Task 3: 引入 DLSS/FSR3 代码与 SDK（不含管线接入）

**Files:**
- Create（逐字复制 pipdlss @`dc43c41f`）: `src/Layers/xrRenderPC_R4/Dlss/{DlssD3D11.cpp,DlssD3D11.h,DlssIntegration.cpp,DlssIntegration.h,Fsr3D3D11.cpp,Fsr3D3D11.h,DlssTemporal.h,DlssNativeUi.h}`
- Create: `sdk/DLSS/**`（含 `lib/nvsdk_ngx_d.lib`）、`sdk/FSR3/**`（含 `fsr3_dx11.props`）
- Modify: `src/Layers/xrRenderPC_R4/xrRender_R4.vcxproj`（新增 `<ClCompile>`/`<ClInclude>`；`AdditionalIncludeDirectories` 追加 `$(MSBuildThisFileDirectory)..\..\..\sdk\FSR3\include`；`AdditionalDependencies` 追加 `$(xrSdkDir)DLSS\lib\nvsdk_ngx_d.lib`；`<Import Project="..\..\..\sdk\FSR3\fsr3_dx11.props" />`）

- [ ] **Step 1: 复制文件**

```powershell
$src="C:\game\xray-monolith-pipdlss"; $dst="C:\game\xray-monolith-mt"
Copy-Item "$src\src\Layers\xrRenderPC_R4\Dlss" "$dst\src\Layers\xrRenderPC_R4\Dlss" -Recurse
Copy-Item "$src\sdk\DLSS" "$dst\sdk\DLSS" -Recurse
Copy-Item "$src\sdk\FSR3" "$dst\sdk\FSR3" -Recurse
```

- [ ] **Step 2: 工程接线（按上面的 Modify 列表）**

- [ ] **Step 3: 构建（此时尚未被调用）**

预期：`0 Error(s)`；`DlssIntegration.cpp`/`Fsr3D3D11.cpp` 出现在编译输出中。

---

## Task 4: 主视口管线接入 DLSS/FSR3

**Files（按 pipdlss 的 `dlss::` 调用点清单逐文件应用 feature hunk）:**
- Modify: `src/Layers/xrRenderPC_R4/stdafx.h`（`#include "Dlss/DlssIntegration.h"`）
- Modify: `src/Layers/xrRender/Blender_Recorder_StandartBinding.cpp`
- Modify: `src/Layers/xrRender/r__dsgraph_render.cpp`
- Modify: `src/Layers/xrRenderDX10/dx10HW.cpp`（`dlss::ShutdownDevice()`）
- Modify: `src/Layers/xrRender/xrRender_console.cpp`（`r_upscaler`/`r_dlss_quality`/`r_fsr3_quality`/`r_dlss_preset`/`r_dlss_sharpness`/`r__upscaler_jitter`/`upscaler_camera_mv` + `r4_hdr10_*` 无关，勿动）
- Modify: `src/Layers/xrRender/xrRender_console.h`
- Modify: `src/Layers/xrRenderPC_R4/r4.cpp`、`r4_R_render.cpp`、`r4_rendertarget.cpp/.h`、`r4_rendertarget_phase_PP.cpp`、`r4_rendertarget_phase_combine.cpp`、`r4_rendertarget_phase_scene.cpp`、`r4_rendertarget_draw_rain.cpp`、`blender_nightvision.cpp`、`blender_gasmask_drops.cpp`、`blender_gasmask_dudv.cpp`

**Interfaces (Consumes):** `dlss::{InitializeTargets,ReleaseTargets,ShutdownDevice,BeginScene,EndScene,Active,Configured,RenderWidth,RenderHeight,RasterJitter,PreviousRasterJitter,SceneDepth,SceneDepthTexture,Output,Evaluate}`（Task 3）。
**约束：** 与我们既有的 SecondVP（`Device.m_SecondViewport.IsSVPFrame()/IsSVPActive()`）共存；SVP pass 期间 `dlss::RenderWidth/Height` 不生效（参见 pipdlss `DlssIntegration.cpp:49-53` 的处理）。

- [ ] **Step 1: 提取每个文件的 feature diff**

```powershell
cd C:\game\xray-monolith-mt
git diff dc43c41f~1 dc43c41f -- <单个文件>   # pipdlss 提交已在本地 remote 'pipdlss'
```
（`git remote add pipdlss C:\game\xray-monolith-pipdlss` 已完成；不可整文件覆盖，只取 feature hunk。）

- [ ] **Step 2: 逐文件应用并解决冲突**（保留我们的 `c_storage` 修复与 SecondVP 判断）

- [ ] **Step 3: 构建**

预期：`0 Error(s)`；与 Task 0 基线相比警告数不显著增加。

- [ ] **Step 4: 运行验证（用户执行）**

控制台 `r_upscaler dlss` + `r_dlss_quality quality` → 日志出现 DLSS 初始化；画面分辨率下降并被重建；切换 `r_upscaler fsr3` 同样生效；`vid_restart` 后仍生效。另两条必须逐条确认（Review Focus 3/5）：
  - **MV 回退**：控制台 `upscaler_camera_mv` 为 `1`（默认）时，移动/转向画面无拖影、无双影（着色器包未提供 MV 时走相机 MV）。
  - **与 PiP 共存**：DLSS 开启状态下 ADS 一把 PiP 镜（`scope_lense_fov` 非零）→ 镜内画面正常（不冻结、不串成主画面），退出 ADS 后主画面正常。

---

## Task 5: 运行期资源（着色器覆盖 + nvngx_dlss.dll）

**Files:**
- Create: `MODS\MT-CN\gamedata\shaders\r3\*`（来自 pipdlss `extras/dlss_gamma/gamedata/shaders/r3/*`：`deffer_grass.vs`、`deffer_impl_flat.ps`、`dlss_cas.h`、`dlss_contract.h`、`dlss_copy.ps`、`dlss_sharpen.ps`、`dlss_ui_depth.ps`、`night_vision.h`、`screenspace_mvectors.h`、`ssfx_*.ps`、`upscaler_camera_mv.ps`、`wd_body.vs`）
- Create: `MODS\MT-CN\bin\nvngx_dlss.dll`（310.9.1，58,956,912 B，SHA256 见 `tools/dlss/README.txt`）

- [ ] **Step 1: 复制着色器覆盖与 dll**

- [ ] **Step 2: 清空着色器缓存后运行（用户执行）**

```powershell
Remove-Item "C:\game\ANTHOLOGY\Anomaly-1.5.3-Anthology 2.1\appdata\shaders_cache" -Recurse -Force
```
预期：启动后 `~Shader cache source CRC mismatch` 建议为 0；无 `Can't compile shader`；DLSS 成功加载（日志中出现 NGX/ngx 相关行）。

- [ ] **Step 3: 记录着色器缓存文件数**（用于确认覆盖生效）

---

## Task 6: `r4_upscaler*` 别名 ↔ 真实 upscaler 双向同步

**Files:**
- Modify: `src/Layers/xrRender/UpscalerCompat.cpp`（同步逻辑，每帧一次或变更时）
- Modify: `src/Layers/xrRender/Blender_Recorder_StandartBinding.cpp` 或 `xrRender_console.cpp`（挂一个每帧钩子；优先复用既有 per-frame 常量回调点）

**Interfaces:**
- `r4_upscaler`(token) → `ps_r_upscaler`；`r4_upscaler_quality` → `ps_r_dlss_quality`（dlss）/`ps_r_fsr3_quality`（fsr3），`custom` → `r4_upscaler_custom_scale`；`r4_upscaler_sharpness` → `ps_r_dlss_sharpness`。
- 反向：`r_upscaler` 被控制台/GAMMA 配置修改时，`r4_upscaler` 同步（MCM 显示一致）。

**映射表（写入实现，勿自行发明）:**

| MOD 名 | 值 | 引擎原始名 |
|---|---|---|
| `r4_upscaler` | off/fsr3/dlss | `r_upscaler` = off/fsr3/dlss（token 名一致） |
| `r4_upscaler_quality` | native/quality/balanced/performance/ultra_performance | `r_dlss_quality`（dlss: off…ultra_performance 按名映射）、`r_fsr3_quality`（native_aa/quality/balanced/performance/ultra_performance） |
| `r4_upscaler_quality` | custom | 使用 `r4_upscaler_custom_scale` 作为渲染比例（需在 `DlssIntegration` 支持自定义比例；若原始实现无此能力，则回退到最接近的 quality 档并记日志） |

- [ ] **Step 1: 实现正向同步**（MCM → 引擎）
- [ ] **Step 2: 实现反向同步**（引擎 → MCM 显示）
- [ ] **Step 3: 构建 + 游戏内验证（用户执行）**：MCM 改 dlss/fsr3 立即生效；改 `r_upscaler` 后 MCM 显示同步。

---

## Task 7: PiP 契约清单（反查 MOD 着色器）

**Files:** 只读 `MODS\..\R.A.K …PiP 3DSS…\gamedata\shaders\r4\*`。

**已知契约（实测，实施时以此为准并补全）:**

| 常量/纹理 | 类型 | 语义 | 来源 |
|---|---|---|---|
| `scope_lense_color` | float4 | brightness, contrast, saturation, gamma | 4 个 `scope_lense_{brightness,contrast,saturation,gamma}` |
| `scope_lense_surface` | float4 | grain, dirt, tint preset, tint intensity | `scope_lense_{grain,dirt,tint,tint_intensity}` |
| `scope_lense_imaging` | float4 | PiP NVG 激活, thermal 激活 | `scope_lense_allow_{nvg,thermal}` × 头戴设备状态 |
| `scope_lense_reproject` | float4x4 / float4 | 重投影矩阵 | 引擎相机（与 DLSS MV/上一帧矩阵同源） |
| `scope_lense_detail` | float4 | x = 锐度 | `scope_lense_sharpness` |
| `scope_lense_quality` | float4 | yz = 镜内渲染尺寸（texel 倒数用） | `scope_lense_quality_percent` |
| `scope_lense_main_view` | float4 | 主视口 UV 的 scale/offset | 引擎（主视口与镜内 UV 映射） |
| `scope_lense_live_effects` | float4 | x>0.5 启用实时效果；yz = UV 偏移 | `scope_lense_live_effects` |
| `scope_lense_motion` | float4 | x = 运动矢量可用标志；yzw = 容差 | 引擎（MV 可用性/`scope_lense_temporal_mode`） |
| `scope_svp` | float? | 镜内 SVP 状态/比例 | 引擎 SecondVP 状态 |
| `s_pip_main_effects` | texture | 主视口效果缓冲（供镜内"实时效果"采样） | 引擎主视口 effect RT |

- [ ] **Step 1: 用 grep 断言覆盖完整**

```powershell
Get-ChildItem "<mods>\...\gamedata\shaders\r4" -File -Include *.ps,*.h |
  Select-String -Pattern 'scope_lense[_a-zA-Z0-9]*|scope_svp|s_pip_[a-z_]*' -AllMatches |
  ForEach-Object { $_.Matches } | ForEach-Object { $_.Value } | Sort-Object -Unique
```
预期：输出全部出现在上表（若出现表外名字，先补表再进入 Task 8）。

---

## Task 8: 下发 `scope_lense_*` 常量

**Files:**
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp`（或 reticle 模型绘制点）注册 `r_Constant("scope_lense_*", ...)`
- Modify: `src/Layers/xrRender/Blender_Recorder_StandartBinding.cpp`（常量 setup 类，与既有 `ssfx_issvp` 同风格）
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget.cpp`（必要时把主视口 effect RT 绑定为 `s_pip_main_effects`）

**Interfaces (Consumes):** Task 1 的 `compat::Lense*()`；Task 4 的 `dlss::RasterJitter()`/`PreviousRasterJitter()`（重投影矩阵同源）。

- [ ] **Step 1: 实现 10 个常量的 per-frame setup**
- [ ] **Step 2: 实现 `s_pip_main_effects` 绑定**（若 `scope_lense_live_effects` 关闭则绑主视口颜色 RT 作为退化）
- [ ] **Step 3: 构建 + 游戏内验证（用户执行）**：ADS PiP 镜内可见亮度/对比/饱和/灰度/锐度/颗粒/污渍/色调变化，且随 MCM 实时改变；无 shader 编译错误。

---

## Task 9: `svp_*.ps` 辅助 pass（按需）

**Files:** `MODS\...\gamedata\shaders\r4\{svp_quality.ps,svp_motion_map.ps,svp_motion_depth.ps}`（MOD 提供）；引擎侧新增调用点。

- [ ] **Step 1: 确认这些 pass 是否由引擎按名字调用**

```powershell
Select-String -Path C:\game\xray-monolith-pipdlss\src\**\*.cpp -Pattern 'svp_quality|svp_motion_map|svp_motion_depth'
```
预期：若 pipdlss/上游均无引用，则说明这是 MOD 侧着色器、由引擎的通用 phase 机制加载；据结果决定是否需要在引擎里新增 pass 调用（记录结论到本任务）。

- [ ] **Step 2: 若需要，实现调用与所需 RT，然后构建**
- [ ] **Step 3: 游戏内验证**：`scope_lense_quality.yz` 反映 `scope_lense_quality_percent`；镜内时间累积生效（`temporal_mode` 三档可区分）。

---

## Task 10: 打包与端到端验收

**Files:** `MODS\MT-CN\`（`bin\AnomalyDX11AVX.exe`、`bin\nvngx_dlss.dll`、`gamedata\shaders\r3\*`、`db\mods\00_modded_exes_gamedata.db0`）、`MODS\MT-CN.sha256.txt`。

- [ ] **Step 1: 出全部配置并取 DX11-AVX**（其余配置只在用户要求时一并出）
- [ ] **Step 2: 清着色器缓存 + 打包 + 出校验单**
- [ ] **Step 3: 端到端验收（用户执行，逐条记录）**
  - MCM `anthology_upscaler` 页 / `rak_pip_quality` 页：不崩
  - DLSS 与 FSR3 均可开启且画面正确（>Task 4 Step 4）
  - PiP 镜效果可见并可调（>Task 8 Step 3）
  - `appdata\logs\*.mdmp` 无新增；`[SCRIPT ERROR]` 为 0
  - FPS/卡顿与基线（`scheduler_batch_size 512`）对比不劣化
- [ ] **Step 4: 提交（**仅在用户明确批准后**）**

```powershell
git add <本计划涉及的文件>
git commit -m "feat(render): native DLSS/FSR3 + mod-name compat cvars + PiP(3DSS) shader contract"
```

---

## Track D（可选，清点时发现）：雪 / 植被季节契约

**不会崩**（MOD 侧都有守护），只是对应功能在整合包里被隐藏。要不要做由用户决定。

- **Files（只读，用于反查）**：`db_root_extract_scripts\scripts\anthology_runtime_seasons_mcm.script`（`r4_snow_*` 13 个的 MCM 声明与守护）、`anthology_runtime_seasons.script`（`r__flora_style`）、`anthology_snow_interaction.script`；MOD 侧着色器 `[GFX] ScreenSpaceShaders 23.5` 的 `anthology_snow_ground.h`、`anthology_snow_step.ps`、`anthology_runtime_seasons_ground_sload.h`、`deffer_anthology_terrain_*.ps`。
- **已知契约（清点结果）**：
  - `r4_snow_enabled`(list 0/1)、`r4_snow_height`(0–1.2)、`r4_snow_distance`(5–60)、`r4_snow_density`(0–1)、`r4_snow_variation`(0–1)、`r4_snow_drift_size`(0.5–20)、`r4_snow_resistance`(0–0.9)、`r4_snow_fall_enabled`(list 0/1)、`r4_snow_flake_size`(0.25–2)、`r4_snow_fall_speed`(0.25–4)、`r4_snow_fall_density`(0–2)、`r4_snow_fall_distance`(5–60)、`r4_snow_wind`(0–2)
  - `r__flora_style`（季节风格，`anthology_runtime_seasons.script` 用 `console:get_integer`/`execute`，取值 1..4）
- **风险点**：只要注册了 `r4_snow_*`，季节 MCM 的"冬季"子树就会**出现**；若此时引擎还没有把值喂给 MOD 着色器，用户会看到一套"能调但没效果"的选项（比现在隐藏更差）。所以 Track D 必须与"雪/季节渲染契约（引擎调用 `anthology_snow_step.ps` 等 + 下发常量）"一起做。
- **建议**：排在 Track B/C 之后；先确认用户是否需要季节/雪功能。

---

## 回滚

- 引擎 exe：`C:\game\ANTHOLOGY\_engine_backup_20261016_2208\MT-CN_before_port.exe`
- 配置：`appdata\user.ltx.bak_20261017`、`gamedata\configs\axr_options.ltx.bak_20261017`（改前值 128）
- 分发前删 `MODS\MT-CN\gamedata\shaders\r3\*` 与 `bin\nvngx_dlss.dll` 即回到纯 MT-CN。
