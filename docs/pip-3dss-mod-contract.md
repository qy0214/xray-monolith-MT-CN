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
| `scope_lense_reproject` | 主视角 UV → 第二视角 UV 的投影矩阵 | 同帧实现必须用**当前帧**镜头 VP：`vp_lens × inv(vp_main)` |
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
（它只是控制台变量 `r__heatvision`）⇒ 3DSS 时代由 pack 自己驱动，PiP 下必须由引擎补上。
本工程按 pack 的 `scope_lense_thermal` 事实驱动（举热成像镜时打开、退出后恢复玩家原设置）。

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
| `$user$viewport2` = 引擎镜内图 | 镜头 pass 渲染 → `phase_lens_postprocess()` 写主套 `rt_secondVP` | ✓ |
| 同帧投影矩阵 | `svp_vp_lens`（当前帧镜头 VP）| ✓ |
| 镜内只画世界（不含武器/瞄具）| 镜头 pass 跳过 `render_hud` 等 | ✓ |
| 镜头可见性用宽视锥 | 镜头 pass 用主相机 `mFullMatrix` 建 `ViewBase` | ✓ |
| 镜内目标按圆盘定尺寸（省帧）| `EnsureTargetSVP()` 以显示高度封顶（1200×750）| ✓ |
| 镜头色调映射（否则近黑）| `phase_lens_postprocess()` | 待验证（post 探针）|
| 热成像由 pack 事实驱动 | `svp_optic_thermal` → `ps_r2_heatvision` | 待验证（`heat=` 探针）|
| 变倍镜当前 FOV | `clamp(ADS_FOV, base, max)` | ✓（本提交）|
| 两种 fov 拼写 | `readOpticFacts()` 两者都读 | ✓（本提交）|
| 政策 `scope_lense_enabled` | — | **缺口** |

---

## 四、已知缺口 / 待决

1. **政策文件未解析**（2.5）：折叠放大镜类可能被 PiP 接管；需要 token 解析规则（pack 未在配置里显式给出 token → 段名映射）。
2. **镜头后处理是否生效**：`pip_*.bmp` 与 `pre/post postprocess` 探针可判定；若未生效需查 `s_postprocess` 的 `s_image` 绑定与参数。
3. **热成像是否触发**：看 `publish thermal=1` 与 `heat=1`。
4. **可变倍**：`clamp(ADS_FOV, 两端)` 依赖引擎 ADS FOV 对这些瞄具有效（M82 等个别瞄具 ADS 状态退化 ⇒ 会被夹到 pack 两端之一，属预期）。
5. 92+ 个提交尚未推送；需要 `git bundle` 或网络恢复。
