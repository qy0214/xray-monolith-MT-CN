# 3DSS 与 PiP 3DSS：两个 MOD 的机制与引擎契约

> 本文由**直接阅读 MOD 自带文件**得出（着色器源码、`.s` 绑定、pack 配置），不是推测。
> 结论先给：**两个 MOD 的 `models_scope_reticle.ps` 逐字节相同**，唯一差别是一张纹理绑定；
> 引擎要做的事因此非常明确。

---

## 一、两个 MOD 是什么关系

| | 3DSS（原版镜） | PiP 3DSS（本工程要支持的）|
|---|---|---|
| 目录 | `…R.A.K 武器包适配 3DSS (OBT)` | `…R.A.K 武器包适配 PiP 3DSS (OBT)` |
| 镜内图像来源 | **`s_prev_frame` = `$user$generic_temp`** | **`s_second_vp` = `$user$viewport2`** |
| 清晰度（开 DLSS 时）| 上一帧整屏被放大 ⇒ **模糊** | 引擎单独渲染的窄视场视图 ⇒ **清晰** |
| 帧成本 | 无额外场景渲染 | 多一遍窄视场场景渲染（应**按镜片圆盘定尺寸**，见下）|

两者**共用同一份镜片着色器**（`models_scope_reticle.ps` / `_precise.ps`，SHA256 相同）：

```hlsl
// models_scope_reticle.ps（3DSS 与 PiP 完全一致）
uniform int scope_svp;                                   // 只读 .x
float4 sample_back_image(float2 uv)
{
    if (scope_svp > 0) return sample_second_vp(uv);       // ← PiP 分支：采样引擎的第二视角
    return s_prev_frame.Sample(smp_base, uv);             // ← 3DSS 分支：采样上一帧画面
}
```

**⇒ 切换两种机制的，是引擎提供的 `scope_svp`，不是两套着色器。**

`.s` 绑定的唯一差别（PiP 多一行）：

```
3DSS: s_prev_frame = $user$generic_temp
PiP : s_prev_frame = $user$generic_temp
      s_second_vp  = $user$viewport2      ← 仅 PiP MOD 有
```

---

## 二、引擎必须提供什么（契约）

### 2.1 无论如何都要提供（3DSS 路径依赖）

| 名字 | 内容 | 引擎中的提供者 |
|---|---|---|
| `$user$generic_temp` | **上一帧的场景图**（combine 时拷贝）| `r4_rendertarget_phase_combine.cpp`：`CopyResource(rt_Generic_temp, rt_Generic_0)` |
| `$user$heat` | 热成像遮罩 | 每套 `createUnique(r2_RT_heat)` + 几何的热通道 |
| `$user$generic2` | 位置缓冲 | 每套 `createUnique(r2_RT_generic2)` |
| `$user$tonemap` | 色调映射查找 | 引擎统一提供 |
| `scope_svp` | **0/1 开关** | `Blender_Recorder_StandartBinding.cpp`：`binder_scope_svp_state` |
| `m_hud_params.y` | **镜内 FOV（度）** | `Actor.cpp`：由武器发布（`hud_params.y`）|
| `ogse_c_screen.x` | **屏幕 FOV** | `Blender_Recorder_StandartBinding.cpp`：`Device.fFOV` |
| `ps_r2_heatvision` | 热成像总开关 | **引擎必须为热成像瞄具打开**（见 2.4）|

### 2.2 PiP 额外必须提供

| 名字 | 内容 | 引擎中的提供者 |
|---|---|---|
| `$user$viewport2` | **引擎渲染的镜内视图**（显示尺寸的一整张图）| `CRender::Render()` 镜头 pass → `phase_lens_postprocess()` 写入主套 `rt_secondVP` |
| `scope_svp = 1` | 告诉着色器"这张就是引擎给的镜内图" | `binder_scope_svp_state`：`IsSVPActive()`（**帧级**，非遍级）|
| `scope_lense_reproject` | 主视角 NDC → **采样 `$user$viewport2` 的 UV** 的矩阵（见 `anthology_pip_temporal.h` 的 `pip_project_capture()`）| **整帧捕获时就是恒等**（本引擎即如此）：过圆盘看出去，视在角 `t` 处看到世界方向 `t/mag`，而镜头相机 FOV = `fFOV/mag` ⇒ 同一方向在镜头里的 NDC 仍等于该屏幕点的 NDC ⇒ 采样自己的 uv。参考实现需要真矩阵，只是因为它把镜内渲染成**裁剪子矩形**再贴进显示尺寸纹理 |
| `scope_lense_main_view` | 镜头 uv ↔ 主画面 uv：`0.5 + (uv-0.5)/mag`，`mag = fFOV/lensFov`（引擎绑定器自算）| 与"镜头相机 FOV = 授权角度"精确吻合（本提交）|
| `scope_lense_motion` | 重投影开关（`.x`）| 本引擎**每帧渲染镜头** ⇒ `x = 0`（走直接投影 ✓）|

### 2.3 pack 配置里的权威数据（引擎必须按它工作）

`mod_system_weapon_scopes_pip_for_3dss.ltx` 开头即写明：

```
; PiP scope lens FOV overrides for g_fov = 65
; Magnification formula: x = 65 / scope_lens_fov
```

| 键 | 含义 | 例（1P59，可变倍）|
|---|---|---|
| `scope_lens_fov` / `scope_lense_fov` | **最高倍**时的镜内 FOV（度）| `6.5` ⇒ 10× |
| `scope_lens_fov_base` / `scope_lense_fov_base` | **最低倍**时的镜内 FOV | `21.6667` ⇒ 3× |
| `scope_zoom_factor` / `zoom_step_count` | 变倍范围与级数 | `6.5` / `6` |
| `scope_dynamic_zoom` | 是否可变倍 | `true` |
| `scope_lense_zoom_only` | **放大只在镜内，主画面不变倍** | `true`（47 处）|
| `scope_lense_thermal` (+ `_mode`) | 该瞄具是热成像及其模式 | 少量瞄具 |
| `scope_lense_enabled` | 政策文件中的总开关（见下）| `![echo1] = true` |

> **注意两种拼写**：pack 同时写 `scope_lens_fov` 与 `scope_lense_fov`，引擎两个都要读。

**变倍镜的当前倍率**：pack 只给两端，而**引擎的原生 ADS FOV 恰好落在两端之间且随级数变化**
⇒ 引擎应取 `clamp(ADS_FOV, 两端)` 作为当前镜内 FOV（本工程已按此实现）。

### 2.4 热成像

`Blender_Recorder_StandartBinding.cpp`：

```cpp
const float thermal = (ps_r2_heatvision > 0 && compat::scope_lense_allow_thermal) ? 1.0f : 0.0f;
```

⇒ 镜片着色器是否出热成像，取决于 **`ps_r2_heatvision`**；而**引擎里没有任何代码会为装备的瞄具设置它**
（它只是控制台变量 `r__heatvision`）⇒ 3DSS 时代由 pack 自己驱动。

**PiP 下不能用它**（实测结论，已回退）：`heatvision.ps` 是**佩戴设备**的全屏效应 ——
`compute_lens_mask()` 只接受 1/2/4 管的小圆遮罩，其它一律 `return 0`（**全黑**），而
`num_tubes = frac(heat_vision_args_1.x)*10`，**没有佩戴设备时是 0** ⇒ 整帧全黑。
这也解释了历史上"强开 `r__heatvision` = 黑屏"。遮罩几何（`flip_down` 同时缩放 y、`params2.x` 同时是
模糊采样数）**无法**安全铺满整帧。

⇒ **热成像瞄具（`s3ds_image_type ≥ 2`）需要的是一张全帧 GBuffer 热成像图**，也就是 pack 着色器在
**3DSS 分支**里自己做的那条路：`infrared(gbuffer_load_data(...))`。

**结论（已实现的方案 A）**：**热成像瞄具不进入第二视图** ⇒ 该瞄具走 **pack 自己的 3DSS 分支**，
由 mod 着色器渲染热成像（`infrared()` + `heat_map` 色表 + 像素化 ✓），**引擎侧不新增任何文件** ✓。
依据：
1. pack 的 PiP 分支在 `optical_thermal` 时**跳过自己的成像**，把"让第二视图变热"的责任交给引擎；
2. 引擎唯一的"让画面变热"的手段是 `phase_heatvision()`，而它是**佩戴设备的遮罩效应**
   （`compute_lens_mask()` 只认 1/2/4 管；无设备 ⇒ `num_tubes = 0` ⇒ **整幅全黑**，已实测），
   给镜头跑它只会得到黑镜；
3. 原始引擎（我们仓库历史 `d404b95d9`/`10ed04ea1`）同样只有 `if (ps_r2_heatvision > 0) phase_heatvision();`
   且由 **pack 脚本**打开该开关（`z_heatvision.script` = 设备；`zzz_anthology_pip_device_bridge.script` = 设备+MCM），
   **没有任何脚本为"只是瞄具"打开它** ⇒ 妙具侧的热成像原本就只存在于 3DSS 分支。

⇒ 状态机里体现为：`SvpScopeInputs::optic_thermal_image` ⇒ `lens_rendered = false`（`xrCore/svp_scope_state.h` ✓，含主机测试）。

**已知限制**：`[vulcan]` 是唯一"NVG(`s3ds_image_type=1`) + `scope_lense_thermal`"的双通道镜 ⇒ 其**热通道**
依赖引擎的 `phase_heatvision`（由佩戴设备脚本驱动），而该 shader 在 pack 里被热成像 MOD 覆盖成遮罩版
⇒ 无设备时与**原始引擎行为一致**（拿不到热画面）。

### 2.5 政策文件（`mod_system_pip_scope_policy.ltx`）

```
; The engine resolves the actual optic token and reads this explicit policy
; before activating SecondVP or applying PiP mouse sensitivity.
![echo1]  scope_lense_enabled = true     ; 允许 PiP（46 个瞄具）
![1p87]   scope_lense_enabled = false    ; 非 PiP 准直器 / 折叠放大镜
```

引擎语义：**解析实际瞄具 token → 读此政策 → 再决定是否激活 SecondVP**。
当前实现用"瞄具是否真的在放大 + PiP 模块是否存在"作为等价门槛（非放大准直器自然为 false），
**差异**：少数"折叠放大镜"被政策标 false 但仍会放大 ⇒ 目前会被 PiP 接管（已知缺口，见第四节）。

---

## 三、引擎实现映射（本工程）

| 契约要求 | 实现位置 | 状态 |
|---|---|---|
| 帧级 `scope_svp`（只读 `.x`）| `binder_scope_svp_state` = `IsSVPActive()` | ✓ |
| `$user$viewport2` = 引擎镜内图 | 镜头 pass 渲染 → `CRender::Render` 拷进主套 `rt_secondVP` | ✓ |
| 采样映射 = 恒等（整帧捕获）| `svp_vp_lens`（`r__svp_debug` 探针直接验证 `ndc(0.5)→uv(0.75)`）| ✓（本提交）|
| 镜内只画世界（不含武器/瞄具）| 镜头 pass 跳过 `render_hud` 等 | ✓ |
| 镜头可见性用宽视锥 | 镜头 pass 用主相机 `mFullMatrix` 建 `ViewBase` | ✓ |
| 镜内目标尺寸保持显示尺寸/比例 | `EnsureTargetSVP()`（曾试过高度封顶，会变矩形裁剪，已回退）| ✓ |
| 镜头曝光（否则近黑）| 组合阶段 `t_LUM_dest`（= `$user$tonemap` = `rt_LUM_pool[1]`）改用**主套**的值：镜头 pass 不跑 `phase_pp`（曝光在那里算）| ✓（本提交）|
| 镜头相机 FOV = pack 授权角度 | `DeriveZoom()` 直接按角度重建投影（`_22=1/tan(fov/2)`、`_11=aspect·_22`），不再用比值缩放 | ✓（本提交）|
| 热成像瞄具 | 状态机排除（`optic_thermal_image`）⇒ 该瞄具走 pack 的 3DSS 热成像分支 | ✓（方案 A，见 2.4）|
| 变倍镜当前 FOV | `clamp(ADS_FOV, 两端)`，并记录 `ads=`/`end=` 供判定 | ✓（本提交）|
| 两种 fov 拼写 | `readOpticFacts()` 两者都读 | ✓ |
| 政策 `scope_lense_enabled` | — | **缺口** |

---

## 四、已知缺口 / 待决

1. **政策文件未解析**（2.5）：折叠放大镜类可能被 PiP 接管；需要 token 解析规则（pack 未在配置里显式给出 token → 段名映射）。
2. **热成像瞄具**（2.4）：**已解决（方案 A，不新增文件）** —— 热成像瞄具不进第二视图，改走 pack 自己的 3DSS
   热成像分支。剩余的是 `[vulcan]` 双通道镜的热通道，属 **pack 自身打包问题**（热 shader 被覆盖成设备遮罩版）。
   若将来仍想在 PiP 下保留热成像瞄具的镜内锐度，才需要自建一支无遮罩的全帧 GBuffer 热成像 pass
   （放 `MODS\MT-CN\gamedata\shaders\r3\`，与现有 DLSS/NVG shader 同目录 ✓；`R4::getShaderPath()` 返回 `"r3\\"` ✓）。
3. **变倍判定**：`clamp(ADS_FOV, 两端)` 依赖引擎 ADS FOV 对这些瞄具有效。M82 等个别瞄具
   `m_fRTZoomFactor` 会读到退化值（曾读到 96°）⇒ 被夹到 pack 较宽一端。日志现给出
   `fovMax/fovBase/ads/end` ⇒ 下一次实测可判定"是瞄具本身不可变"还是"被夹错端"。
4. **推送**：108 个提交未推送；需要 `git bundle` 或网络恢复。
5. **部署链路**：`bin\AnomalyDX11AVX.exe` 曾被替换成 MT 构建（见报告），
   需要确认玩家启动方式（MO2 覆盖 vs 拷进 bin），否则测试可能跑的不是最新构建 ——
   **判定方法**：日志启动戳的 `exeTime=/exeSize=` 必须与部署载荷一致。
