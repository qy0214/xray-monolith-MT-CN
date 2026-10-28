# 2026-10-26 — "第二光源"再归因：影子方向 / 光柱锚定（MT-CN 引擎）

> 状态：**未闭环**。玩家明确拆成两件独立问题：先"影子错位"，再"光柱锚定"。
> 当日已部署一个新的诊断构建（见 §4），明天的实测任务是 §3 的 A/B/C。
> 本文所有角度/像素均由引擎日志的相机与 `E.sun_dir` 计算，不再用肉眼判断太阳位置。

## 1. 已排除（不要再重复验证）

| 假设 | 证据 |
|---|---|
| 屏幕空间层（SSS/AO/IL/雾/光柱/光晕） | 全关截图（01:54:04/12/19）地面暗斑仍在 |
| 风动造成"影子跟着视角走" | `ssfx_wind_trees (0,0,0,0)` + 草 0 后影子停止晃动，但错位仍在 |
| DLSS/TAA、PiP、点/聚光、AO | 前几轮逐项关闭无效；`LP_normal point=0 spot=0` |
| 太阳与光照方向不一致 | 02:31 dump：`sun_adapted->dir == E.sun_dir`；太阳屏幕位置由相机矩阵可复现 |
| 阴影方向镜像（左右翻转） | 渲染与采样共用同一 `m_shadow`；`m_TexelAdjust` 标准；smap 视口无翻转；`shadow.h` 采样标准 |

决定性引擎数据（02:31 dump，1920 逻辑视口 / 960×600 dump）：

```
cam pos = (-48.55, 5.23, -97.46)
cam dir = (-1.000, -0.009, -0.023)      # 沿 -X，基本水平
sun_dir = (-0.6559, -0.6390, 0.4018)    # 太阳在相机背后偏左，仰角 ~39.7°，离视线 130°
```

## 2. 已测量

- **02:14:26 视图的那条"错位影子"**：亮路面上一条细长直影，端点约 `(547,915)→(770,792)`（1920×1200）。
  - 引擎预测的平坦地面影子方向（同点、同帧相机+太阳）：**+16.1°**（右上）
  - 实测影带轴向：**+28.9°**（右上）
  - 差 12.8°：同象限、同走向；差值可由路面横坡（影带在路肩）与截图(10:09)到 dump 的太阳位移解释。
  - 结论：该影子**背离**太阳（太阳在背后偏左），不是"右上第二光源"打出来的；投影物应在相机背后约 10 m、高约 10 m 处（被视锥裁掉）。
- **01:24 dump 帧的太阳投影** = uv `(0.289, 0.020)`（画面顶边），与可见泛光位置一致。
- 静态相机 8 s 对比（01:27:17 vs 01:27:25）：树干/卡车/左地面 `dx=dy=0`，树冠与右坡地面暗斑大幅变化（平均 |Δ|=15.5/255）→ 变化部分是风动枝叶影。
- 枯松树冠左半比右半亮（均值 72 vs 57，01:03:09）；右坡灌木亮面在左上 → 可见面光照与太阳一致。

## 3. 明天要做的实测（游戏已部署新构建）

### A. 验证玩家提出的"影子绑定在角色视角上"

1. 站到亮路面，朝 A 方向（地面影子可见）；`r__dump_shadow_buffers 1`；F12。
2. **人不动**，原地转 ~50°（同一片地面尽量留在画面里）；再次 `r__dump_shadow_buffers 1`；F12。
3. 退出游戏（日志只在退出时刷盘）。

分析：用两次 `dbg_position.bmp` 还原每个像素的视空间位置/法线 → 找两次都可见的**同一世界地面点** → 比较明暗。
- 只因为转相机就从亮变暗 → **视角绑定确诊**；
- 同一世界点明暗一致 → 世界锚定，错位只能来自投影/投影物，转入逐级联真值。

### B. 判断"第二光源"是否为天空烘焙太阳

同一批 dump 会生成 `dbg_color_sun.bmp`：引擎按相机矩阵 + `E.sun_dir` 在原始帧上画**黄色十字**。
- 十字与可见太阳圆盘不重合 → 天空贴图里烘焙的太阳不跟随动态太阳，这就是"第二光源"（引擎侧可修：让天空/日盘跟随 `E.sun_dir`）；
- 重合 → 天空与光照一致，问题回到 A。

### C. 光柱锚定（独立问题，A/B 之后再动）

打开光柱 + 雾散射，拍一张**太阳完整在画面里、光柱可见**的截图。量：光柱汇聚点像素 vs 太阳圆盘中心像素。
- 不重合 → 光柱锚点 bug（与 2026-10-25 那批"同帧双相机 × 按帧缓存状态"同类）；
- 重合 → 光柱几何正确，只是屏幕空间特性（太阳出画时应淡出/贴边）。

## 4. 诊断构建（已部署）

- 部署：`MODS\MT-CN\bin\AnomalyDX11AVX.exe`，**2026-10-26 08:54，28,666,368 B，md5 `7bb53dd4f87c0fbf17d2705004afd1ef`**（每次 dump 的文件名带序号后缀 `_<n>`，两次 dump 不会互相覆盖）。
- 上一版（02:42，md5 `729eb8a6…`）只改诊断；本版在它基础上加了 dump 序号。
- 构建：`build_mtcn.cmd AVX NOPAUSE`，DX11-AVX，0 错误。
- 新增诊断（仅诊断，不改渲染行为）：
  - `dbg_position_<n>.bmp`：`rt_Position`（A16B16G16R16F；xy=打包法线，z=视空间深度），供世界点/法线还原；
  - `dbg_color_sun_<n>.bmp`：`rt_Generic_0` 副本 + 引擎太阳位置黄色十字；
  - 日志新增 `[DBG] sun screen ndc=(...) uv_topLeft=(...)`。
- 未提交代码：`r4_R_render.cpp`（dump 扩展 + 序号）、`Light_DB.cpp`（`sun_original->set_shadow(false)` 诊断）、`rendertarget_phase_blur.cpp`、`xrRender_console.cpp`。

## 5. 约束与卫生

- 不改整合包；只部署 `MODS\MT-CN`，不碰游戏自带 `bin`；每项改动需实机验收。
- 未经批准不提交/不推送；不删除未跟踪的 `_crop_patch.png` / `dist/MT-CN.rar`。
- 日志只在**退出游戏**时刷盘（可能截断在半行）；BMP/F12 立即落盘。
- 当天的测试状态：风 0 + 草 0，光柱/光晕/雾关，SSS/AO/IL 关；测试后按 01:26 清单还原或重进游戏让 MCM 重发。

## 6. 证据文件

- 截图：`appdata/screenshots/ss_admin_10-26-26_02-14-26_(way_in_zone).jpg`（本次争议视图）
- dump：`appdata/logs/dbg_color.bmp`（02:31 帧）、`dbg_position.bmp`、`dbg_color_sun.bmp`
- 几何叠加：`_verify_road_compass.png`（青=实测影带 +28.9°，品红=引擎预测 +16.1°，黄=预测影子场）
- 相机验证图：`_compass_armview_final.png`（01:24 帧，太阳 uv(0.289,0.020)，地面影子场）

## 7. 2026-10-26 08:49 双视角实测（重要结果）

玩家在同一位置朝两个方向各做了一次 `r__dump_shadow_buffers 1` + F12（`ss_admin_10-26-26_08-49-10/08-49-27`，游戏时间 08:39/08:41）。相机从日志取得：

| | cam dir | E.sun_dir | 太阳屏幕位置 |
|---|---|---|---|
| 视角 1 | `(0.844, -0.027, -0.536)` | `(-0.8809, -0.4442, 0.1633)` | 预测 uv `(0.2997, 0.0529)` = 像素 `(575.4, 63.5)` |
| 视角 2 | `(-0.246, 0.007, -0.969)` | `(-0.8777, -0.4487, 0.1685)` | 在画面左缘外（视空间 z=-0.05，约 93°） |

**决定性结果 1（太阳位置）**：视角 1 的 F12 里可见太阳圆盘的过曝像素质心 = **(576.8, 62.8)**，引擎按日志相机+`E.sun_dir` 投影 = **(575.4, 63.5)** → **误差 1.4 px**。据此，"天空里烘焙的太阳不跟随动态太阳"这一假设在本帧被**否定**：可见太阳与光照方向一致。

**决定性结果 2（影子方向）**：两个视角各自的平地影子方向场（3 m 箭头）如下；画面里的实际阴影与其一致：
- 视角 1（太阳在画面上方偏左）：卡车**左侧被照亮**，车影朝**右**铺开；右侧大树的影子也朝右；贴图见 `_verify_view1_compass.png`。
- 视角 2（太阳在画面左缘外）：树影、灌木亮面在左上、影子朝右上；贴图见 `_verify_view2_compass.png`。
- 两视角相距 72°，影子在**屏幕上的方向**随之改变 → 没有"屏幕锁定"的固定方向；"视角绑定"假设在两视角对比下不成立（硬证据仍需 rt_Position 世界点比对，见下）。

**注意：本次双视角测试跑的是旧 exe（游戏自带 `bin`，10-25 23:30 版本），所以没有产出 `dbg_position`/`dbg_color_sun`，两次 dump 的旧文件名也互相覆盖了。** 新构建（08:54，带序号后缀）已放在 `MODS\MT-CN\bin`，需要在启动器里停用→启用 MT-CN 模组（把新 exe 挂到游戏 `bin`）后再做一次双视角测试，才能完成世界点比对。

## 8. 2026-10-26 09:04 双视角实测（新构建已挂载）

运行环境：游戏 `bin` 与新 `MODS\MT-CN\bin` 同为 28,666,368 B（08:55），确认新 exe 已挂载；dump 文件名带 `_1`/`_2`，两次没有互相覆盖；F12 为 1920×1200，dump 1 为 1440×900、dump 2 为 960×600。

相机与太阳（日志只留下 dump 1 的完整块）：

```
cam pos=(-48.57 5.24 -97.44) dir=(-0.960 -0.097 -0.261) fov=65
E.sun_dir=(-0.8840 -0.4399 0.1581)      # 仰角 26.1°，光行进方向
to_sun·view = -0.850                     # 太阳在相机背后 148°（背后偏左）
sun_adapted dir == E.sun_dir, shadow=1   # 太阳灯方向正确
```

**结论 1（图层定性）**：视角 1（09:04:20）画面中心偏左的长暗带（dump 坐标上端≈(370,465)、下端≈(393,710)，宽 25→80 px）在 `dbg_accum_1.bmp`（`rt_Accumulator`，原始太阳 pass）里**同样存在** → 它是 **CSM 太阳阴影本身**，不是 AO/SSS/IL/雾/光柱。屏幕空间层对这一条暗带可以正式排除。

**结论 2（方向比对）**：按相机+`E.sun_dir` 计算，平地的影子方向场灭点在 dump `(1056,381)`（右上）；把暗带两端反投影到水平面，世界水平轴向 `(0.743,0.669)`，与太阳地面方向 `L_h=(-0.984,0.176)` 夹角 **127.9°**（把下端当投影物根部则是 52.1°）。暗带位于左侧陡坡脚：若坡面朝相机倾 45–60°，同一太阳可以把影子扭到接近屏幕向下，所以**这条带子单独不能证明 CSM 方向被旋转**。需要平地+已知投影物做无坡度对照。

**结论 3（08:49 平地对照仍成立）**：太阳入画的平地上（卡车）影子朝右、朝阳面在上方偏左，与引擎太阳一致 → CSM 至少在该帧方向正确，问题不是全局翻转。

**结论 4（本次未完成世界点 A/B）**：
- 09:04:43 的第二视角转了约 100°，画面里没有与视角 1 共享的地面 → 无法做同一世界点明暗对照；
- dump 2 的相机行在退出刷盘时被截断（日志尾 `* [DBG] ca`），视角 2 的相机丢失；
- `dbg_position_<n>.bmp` 不可用于世界点恢复：BMP 写入把 16F 钳到 0..1（深度丢失），且 dump 时刻 `rt_Position` 内容无法按场景法线解码（G 通道全 0，疑似被后续 pass 复用）。

**下一轮最小实验（一次判定）**：
1. 站到平坦空地/公路，让一个干净投影物（卡车、电线杆或单独一棵树）的**根部与影子同时入画**；F12 + `r__dump_shadow_buffers 1`。
2. 原地转 40–60°，**让同一片地面和投影物仍在画面里**；F12 + dump。
3. 退出游戏刷日志。
判定：平地上两视角的"影子轴向 vs 太阳预测"仍差 >20° → 确诊 CSM 方向问题；一致 → 做逐级联真值（把 3 个级联矩阵与世界点投影写进日志），重点查近景级联。

**构建改进（下一版）**：相机 pos/dir 与 sun_dir 直接写进 dump 文件名，不再依赖退出时日志刷盘；为 `rt_Position` 增加深度专用灰度转储（z 映射到 0..100 m）。

## 9. 2026-10-26 三轮技能审查 + 独立复核（纯代码，无游戏测试）

方法：`diagnose` / `bug-review` / `code-review-analyzer` 三轮 + 两个独立子代理对抗复核。**重要更正**：此前"暗带出现在 `rt_Accumulator` 就是 CSM"的结论是错的。

### 9.1 决定性发现：accum 里的太阳被乘上了屏幕空间 SSS 遮罩

- `accum_sun_near.ps` / `accum_sun_far.ps` 结尾：`s *= saturate(s_ssfx_sss.Sample(smp_nofilter, I.tc.xy/I.tc.w).b + ssfx_issvp.x);` —— 主视口 `ssfx_issvp.x = 0`（`Blender_Recorder_StandartBinding.cpp:1207-1213`），所以乘数就是 `.b`。
- `.b` 不是常量：`ssfx_sss.ps` 先写 `float4(sss.rg, 1, ...)`，随后 `rendertarget_phase_blur.cpp:596-633` 用 `ssfx_sss_blur.ps` 跑两遍模糊（`blur_setup (0,1,0,0)` → 模糊 R 写入 B；再 `(1,0,2,0)` → 模糊 B），最后拷回 `rt_ssfx_sss`。**`.b` = 两遍模糊后的直接 SSS 遮罩**。
- SSS 遮罩本身是视角绑定的：`screenspace_shadows.h` 从每个像素沿 `-L_sun_dir_e` 在屏幕空间 march（出屏 `return 1`），命中遮挡物就返回 `saturate(1-G_SSS_INTENSITY)` = **0**（`settings_screenspace_SSS.h: G_SSS_INTENSITY=1.0`），再按 0.95 历史权重做时间累积（`ssfx_sss.ps`），再两遍模糊。→ 屏幕上出现与真实投影物不对应的、连贯的、随视角走的暗影，且被乘进太阳 pass 后与 CSM 影无法区分。
- 09:04 会话日志证明 SSS/风都是开着的，此前的"清场测试状态"并不成立：
  ```
  ssfx_sss (1.000000, 1.000000, 0.000000, 0.000000)      # 直接 SSS 长度=1
  ssfx_sss_quality (12.000000, 4.000000, 1.000000, 1.000000)  # 直接+omni SSS 均开
  ssfx_wind_trees (11.100000, 0.170000, 0.900000, 0.100000)   # 风开
  ```
  因此 09:04 的暗带是 **CSM + 屏幕空间 SSS** 的混合；其中 band_mid 处 SSS `.b`≈254（无 SSS 影）→ 该段是 CSM；band_bot 处 SSS `.b`≈1（SSS 影）→ 该段至少部分是 SSS。整条带不能单独归因给 CSM。

### 9.2 CSM 方向链复核结论（两个独立复核一致）

- 渲染与采样同源：`R_sun.cpp:304` 把 `fuckingsun->X.D.combine = cull_xform`，`:314-316` 用同一矩阵渲染；`r4_rendertarget_accum_direct.cpp:483-500` 采样矩阵 `m_shadow = TexelAdjust · X.D.combine · Device.mInvView`；着色器 `gbd.P` 是视空间（`gbuffer_stage.h:144`），`m_invView` 精确互逆（`CameraManager.cpp:491-492`）。符号/手性/矩阵乘法顺序/正交投影/深度翻转/`m_VP` 绑定全部核对无误。
- 日志 `sun_adapted->dir == E.sun_dir`；树干亮边在左侧，与 `to_sun` 的横向分量一致 → 直接光照方向也正确。
- **结论：CSM 矩阵链在数学上不可能让影子方向相对 `fuckingsun->direction` 旋转。**

### 9.3 顺带查出的两个真实 CSM 缺陷（不是方向问题，但会制造"错位影"）

1. **FAR 级联体积用错矩阵**：`r4_rendertarget_accum_direct.cpp:569-572`，`SE_SUN_FAR` 的累加体积用 `xform_prev`（中景级联）而 `m_shadow` 用的是远景级联 → 远景光照/阴影覆盖与采样不匹配。2019 年 Anomaly 老代码，非本 fork 引入。
2. **无投影物时不清空阴影图**：`R_sun.cpp:311-317` 的 `phase_smap_direct`（含 `ClearDepthStencilView`）只在 `bDeffered_Shadows || bForward_Shadows` 时调用，但 `:357-365` 的 `accum_direct_cascade` 无条件执行 → 某级联无投影物时会拿"上一级联/上一帧"的旧阴影图配当前级联矩阵采样，运动中产生随视角漂移的错影。**这是真正的 CSM 代码缺陷，修法：把清空移出 if。**

### 9.4 修复方案（引擎侧，不改整合包）

- **A（先做，可逆）**：在 `rendertarget_phase_blur.cpp` 绑定 `ssfx_sss` 时提供引擎开关，把第 1 分量（Dir Len）置 0 → SSS 射线长度 0 → 遮罩恒 1 → accum 的乘数变 1，一帧内把屏幕空间 SSS 影从太阳 pass 里分离，无需玩家操作。
- **B**：把 `phase_smap_direct` 的清空移出投影物条件（9.3-2）。
- **C**：FAR 级联体积改用 `xform`（9.3-1），需单独评估远景光照变化。

### 9.5 历史复核

- `1cf2b14a7`（dedup R2/R3/R4 sun）只改了未使用的旧凸体代码；`ad40ac3e8`（1 纹素对齐）只改 `align_granularity`；`9dc5a5b58`（DLSS 移植）在 accum_direct 里只把 `Device.dwWidth/Height` 换成 `RenderScreenWidth/Height`。**fork 没有引入 CSM 朝向变化。**

## 10. 两个 CSM 真缺陷已修（2026-10-26，构建通过，未部署/未测试）

按玩家要求先修 9.3 的两项，纯引擎侧、不改整合包：

1. **无投影物时清空阴影图**（`R_sun.cpp:308-325`）：`phase_smap_direct` + 阴影 pass 的 view/project 设置移出 `if (bDeffered_Shadows || bForward_Shadows)`，现在每个级联都无条件 bind+clear；只有投影物绘制仍条件执行。副作用：`X.D.transluent` 在无投影物时保持上一级联值，但它只被 point/spot accum 读取（`r4_rendertarget_accum_point.cpp:87`、`r4_rendertarget_accum_spot.cpp:147`），不影响太阳 pass。
2. **FAR 级联体积用自身矩阵**（`r4_rendertarget_accum_direct.cpp:566-574`）：恢复被注释掉的 `R2FLAGEXT_SUN_ZCULLING` 条件。该 flag 默认关闭（`xrRender_console.cpp:236` 默认值不含它），因此默认行为改为 FAR 用 `xform`（自己的 160 m 体积）；玩家/MCM 若打开 `r2_shadow_cascede_zcul` 则保留旧的 `xform_prev` 行为。

构建：`build_mtcn.cmd AVX NOPAUSE` → **0 错误**，输出 `_build\_game\bin_dbg\AnomalyDX11AVX.exe`，2026-10-26 09:56，28,666,368 B，md5 `2ba29c96eb2ab559d8cb98034728477b`。**已按玩家要求暂存到 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`**（09:56，同 md5）；旧的 08:54 诊断构建保留为同目录 `AnomalyDX11AVX_diag0854.exe`（28,666,368 B，md5 `7bb53dd4…`）作回滚。**未进游戏**（玩家将真机测试）。SSS 层（9.4-A）尚未改动。

当前挂载状态（2026-10-26 10:00 检查）：游戏自带 `bin\AnomalyDX11AVX.exe` = **48,067,584 B，2026-10-03 03:01，md5 `f983ce27…`**，即 MT-CN 当前**未挂载**（JSGME 已还原原版 exe）。测试前需在启动器里启用 MT-CN，使 `bin\AnomalyDX11AVX.exe` 变为 28,666,368 B / md5 `2ba29c96…` 再启动。

真机观察点：① 远景（>60 m）太阳光照/阴影覆盖是否更完整、20/60/160 m 级联交界有无新的台阶/亮暗环；② 移动/转身时是否还有"跟在相机后面"的旧影（修复 1 针对项）；③ 方向错位若仍在，重点看它是否随视角走（那是 SSS 层，方案 A 待做），并按需在异常点执行 `r__dump_shadow_buffers 1` + 退出刷日志。

## 11. 配置修正（2026-10-26，按玩家要求）

检查了 `MODS\MT-CN` 的 `appdata/user.ltx`、`gamedata/configs/axr_options.ltx`（MCM）以及游戏当前生效的两份同名文件。改动（MOD 副本与游戏当前生效副本同步改，保证挂载/不挂载都一致）：

1. **截图键 → F12**：`bind screenshot kADD` → `kF12`；删除 `bind_sec screenshot kADD`。原因：MCM 里 `beef_nvg/keybind_nv_up = 78`（DIK 78 = 数字键盘 +）与 `kADD` 撞键，按小键盘 + 会同时截图和调 NVG 亮度。F12（DIK 88）在 user.ltx 与 MCM 键位里都未被占用。
2. **关闭屏幕空间直接 SSS 影**：`ssfx_sss_quality (12,4,1,1)` → `(12,4,0,1)`；MCM `ssfx_module/sss/enable_dir_mcm true → false`（MCM 会在启动时覆盖 user.ltx，所以两处都要改）。原因：这是 §9 证实的视角绑定错位影来源；关掉后太阳 pass 只含 CSM，便于本次真机验收修复 1/2。omni SSS 保留（`.w=1`）。
3. **保留玩家的季节风格**：把当前生效 MCM 里的 `anthology_runtime_seasons/style = 6` 补进 MOD 副本，挂载时不会被重置。
4. 保留未动：`r2_shadow_cascede_zcul` 未设置（默认关 → 修复 2 生效）；MCM 级联尺寸 20/60/160 与引擎一致；`r2_sun_near 20`/`r2_sun_far 100` 与引擎一致；风/光柱/雾/光晕/AO/IL 均为玩家正常视觉设置，未改。MCM 与 user.ltx 结构审计无重复节/键、无非法行；`[modded_exes] version` 与 `session_id/session_start` 为运行时字段，未动。未处理的次要重叠：`beef_nvg/keybind_nv_down = 74`（小键盘 -）与 `bind custom6 kSUBTRACT` 重叠，按需再调。

## 12. 关闭 SSS 导致全屏变暗的引擎缺陷（2026-10-26 10:19 构建）

玩家实测：按 §11 从启动起关闭直接 SSS 后，**整个屏幕明显变暗**（太阳在画面里时地面/树也黑）。

**根因**：`r4_R_render.cpp` 的关闭路径只在"本进程渲染过 SSS 之后"才把 `rt_ssfx_sss` 清成中性值：
```cpp
if (ps_ssfx_sss_quality.z > 0) { phase_ssfx_sss(); sss_rendered = true; }
else { if (sss_rendered) { clear to 1 } }
```
从启动起 `z=0` → `sss_rendered` 永远 false → 从不清空；`rt_ssfx_sss` 是 A8R8G8B8，创建值为 0；而 `accum_sun_near/far.ps` 结尾 `s *= saturate(s_ssfx_sss.Sample(...).b + ssfx_issvp.x)` → 太阳贡献被乘 0 → 全屏暗。

**修复**：关闭对应 pass 时无条件把 `rt_ssfx_sss`、`rt_ssfx_sss_tmp` 清成 `(1,1,1,1)`（主视口 else 分支与 PiP lens 分支都改）；打开时行为不变。

构建：10:19，28,665,856 B，md5 `ae2b9ef2d05dd1b89b61978c4af825df`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；09:56 的 CSM 修复构建保留为同目录 `AnomalyDX11AVX_csmfix0956.exe` 作回滚。**待玩家重启/重挂验证亮度恢复。**

**玩家 10:12/10:14 两张截图观察**：10:12 太阳在画面内，太阳周围放射状光柱=体积光（正常）；10:14 太阳在背后，山坡上出现大片斜向宽带——`ssfx_fog_scattering.ps` 根本不读太阳方向（只按雾量混合模糊后的屏幕）→ 这是屏幕空间雾散射，天然视角绑定；体积太阳光柱（`accum_volumetric_sun.ps`，用 `L_sun_dir_e`/`L_sun_dir_w`）当前在太阳背后时也没有做可见性门控。定位顺序：先 `ssfx_fog_scattering 0` 看宽带是否消失；再 `r2_sunshafts_mode off`。若确认是体积太阳光柱，下一步在 `accum_direct_cascade` 的 volumetric 调用前加"太阳在相机前方/画面内"门控与边缘淡出。

## 13. 体积太阳光柱加太阳可见性门控（2026-10-26 10:22 构建）

玩家结论（10:2x）：错位影子本应是太阳照射世界物体的影子却被绑到位置；应与太阳绑定的可关闭光柱也没绑太阳。核查：CSM 是世界锚定（相机无关）；剩余"跟视角走"的都是屏幕空间层。体积太阳光柱此前**不检查太阳是否在相机前方**——`need_to_render_sunshafts()` 只看 `ps_sunshafts_mode` 与 `m_fSunShaftsIntensity`，太阳在背后时仍会 ray-march，产生随视角转动的宽带。

修复：
- `r4_rendertarget.cpp:need_to_render_sunshafts()` 增加太阳可见性门控：`sun_adapted->direction` 取反 → 经 `Device.mView.transform_dir` 到视空间；`vs.z <= 0.05`（太阳在视平面后方）→ 返回 false。
- `r4_rendertarget_accum_direct.cpp:accum_direct_volumetric()` 开头也调用 `need_to_render_sunshafts()`，保证体积 pass 与级联链（`b_need_to_render_sunshafts`/minmax）一致。

构建：10:22，28,665,856 B，md5 `4f46cd1849542467d429dcf5789a4e9d`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`（10:19 的暗屏修复构建保留为 `AnomalyDX11AVX_sssclear1019.exe`，09:56 的 CSM 修复构建保留为 `AnomalyDX11AVX_csmfix0956.exe`）。**待玩家重启/重挂验证**：亮度应恢复；太阳在画面内时才有光柱；太阳转到背后时光柱应消失。若仍有随视角的宽带，先 `ssfx_fog_scattering 0`（该层完全不读太阳方向，不是太阳光柱），再 `r__lens_flares off`。

## 14. 全文阅读结论（2026-10-26 下午，玩家要求）

已逐文件读完（非片段）：`R_sun.cpp` 377、`R_sun_support.h` 464、`r4_rendertarget_accum_direct.cpp` 1401、`r__dsgraph_render.cpp` 801、`r__dsgraph_build.cpp` 832、`FTreeVisual.cpp` 358、`R_Backend_xform.cpp`、`Light_DB.cpp`、`Dlss/DlssTemporal.h`、活动 shader（`accum_sun_near/far.ps`、`shadow.h`、`gbuffer_stage.h`、`screenspace_shadows.h`、`ssfx_sss*.ps`、`ssfx_fog_scattering.ps`）。要点：

- 静态投影物（树）在渲染图里以 `pMatrix=nullptr` 入队、以 `m_xform`（树文件变换）+ 烘焙世界顶点绘制；阴影图与采样共用 `X.D.combine`；`gbd.P` 是视空间 + `Device.mInvView`。全文没有任何把影子方向/位置与相机视角耦合的变换。
- `i_mask` 枚举顺序确认：`fl_deffered, fl_forward, fl_wmarks, fl_normal, fl_shmap, …`；级联图 mask `{true,false,false,false,true,false,false}` 的 `fl_normal=false`，所以 HOM/eNoShadow/FullDetail 剔除在阴影 pass 被正确关闭（此前怀疑的"主相机 HOM 剔除投影物"不成立）。
- 发现一个真实的 DLSS 级重建缺陷（非本次方向主因，量级亚像素）：`r4.cpp` 的 `pos_decompression_params.xy` 已含 `RasterJitter*sx`，而 `gbuffer_stage.h` 又对 `pos2d` 减了一次同一抖动 → 抖动被补偿两次；同时 `binder_pos_decompress_params2` 的 `screen_res` 需确认是渲染分辨率。该缺陷影响所有屏幕空间位置重建（含 CSM 采样点）。
- 玩家要求"不关光柱"。当前太阳 pass 里只剩 CSM + 体积光柱。已把最后的暗屏幕空间层 AO/IL 在 `user.ltx`（`ssfx_ao_quality 0`、`ssfx_il_quality 0`）和 MCM（`ssfx_module/ao|il/quality_mcm 0`）关闭；光柱保留。
- 若影子仍在（玩家结论：影子离开物体、随视角转），引擎侧下一步应针对体积光柱：保留光柱但去掉它用 CSM 阴影图做的遮挡调制（`accum_direct_volumetric` 把 `s_smap` 绑成中性纹理，或 shader 常量旁路），因为它是唯一剩下的屏幕空间层，能把阴影图以屏幕空间方式画成随视角移动的暗带。

## 15. 抖动双重补偿修复 + 光柱旁路撤销（2026-10-26 11:09 构建）

- **已修（引擎级）**：`r4.cpp` 的 `binder_pos_decompress_params` 不再把 `dlss::RasterJitter()` 加进 `pos_decompression_params.xy`。`gbuffer_stage.h` 已经从像素坐标里减过一次同一光栅抖动，再在常量里加一次会让所有屏幕空间世界位置（包括 CSM 阴影采样点）偏移一个采样。此改动不涉及 DLSS 自身：`dlss_params`、`dlss_temporal_jitter`、NGX 评估、相机运动矢量、投影抖动路径均未动，`[DLSS] ACTIVE` 仍应正常出现。
- **撤销的光柱旁路**：读完 `accum_volumetric_sun.ps` 后确认，光柱亮度主要由 `scatterEdge`（阴影图边缘）经 `gate = saturate(edgeFrac * kFillGate)` 驱动；把阴影坐标推成 OOB（全 lit）会使 `scatterEdge=0` → `gate=0` → 光柱**整个消失**。这违反玩家"不关光柱"，因此未采用，`accum_direct_volumetric` 保持原样（10:22 的太阳可见性门控仍在）。
- 构建：11:09，28,665,344 B，md5 `cf12f7d0173dd5e3cb431febfd43c320`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；10:22 构建备份为 `AnomalyDX11AVX_sungate1022.exe`。
- 配置：AO/IL 已关（`ssfx_ao_quality 0`、`ssfx_il_quality 0`，MCM 同步），直接 SSS 关，光柱保留；风/雾/光晕按玩家原设置。

## 16. 光柱模式切回经典 screen_space（2026-10-26）

玩家报告"两个问题依然都在"。查 `ps_sunshafts_mode` 默认值：引擎默认 **2 = R2SS_SCREEN_SPACE**（经典 OGSE 光柱，`ogse_sunshafts_mask/blur/final`，锚定太阳屏幕位置），而 MT-CN 的 `user.ltx` 把它设成了 **1 = R2SS_VOLUMETRIC**（SSFX `accum_volumetric_sun.ps`，屏幕空间阴影边缘光带——即"随视角走的阴影"来源）。已将 MOD 与游戏当前 `user.ltx` 的 `r2_sunshafts_mode volumetric` 改为 `screen_space`；光柱保留（换成经典太阳光柱），不关效果。无需重新构建；下次启动生效，控制台 `r2_sunshafts_mode screen_space` 可立即生效。

## 17. 引擎侧太阳锚定光柱 + 级联矩阵真值转储（2026-10-26 11:33 构建）

玩家补充：太阳自身已有光柱；被关闭的是"跟随视角转动"的那条（SSFX 体积光柱），要求把它绑定到太阳。实现（引擎侧，不改整合包 shader）：

- 新增引擎自有 shader `gamedata/shaders/r3/csm_sunmask.ps`（部署到 `MODS\MT-CN\gamedata\shaders\r3`）：读取体积缓冲，按太阳屏幕位置做径向遮罩（`sun_mask = uv, radius, aspect`）。
- 新增 `CBlender_sunmask`（`blender_ss_sunshafts.*`），在 `r4_rendertarget.cpp` 创建 `s_sunmask`。
- `apply_sun_volumetric_mask()`（`r4_rendertarget_accum_direct.cpp`）：在**远景级联的太阳体积光绘制之后**（太阳体积只在此前累积，点/聚光体积在其后加入，不受影响）把 `rt_Generic_2` 拷到 `rt_ssfx_accum`，再以 `csm_sunmask` 画回 `rt_Generic_2`。太阳在视平面后/画面外时遮罩半径为 0 → 体积光为 0，光柱只随太阳出现。MSAA 开启时跳过（保持原样）。
- 配置 `r2_sunshafts_mode` 已改回 `volumetric`，让这条体积光柱重新出现，但现在被太阳屏幕位置遮罩（绑定太阳）。
- 另加 **CSM 真值转储**：`r__dump_shadow_buffers 1` 现在除原有 dump 外，日志会输出三条 `* [DBG] cascadeN r0=... r1=... r2=... r3=...`，即阴影 pass 实际使用的级联矩阵，可与截图中的落点直接对照。
- 构建：11:33，28,669,952 B，md5 `dbd8116bee108449ba6dd37bf7034017`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；11:09 构建备份 `AnomalyDX11AVX_jitter1109.exe`。DLSS 路径未改。
- 风险：`csm_sunmask.ps` 首次运行时编译；若日志出现 shader 编译失败/stub_default，请把日志发来（该 pass 只影响体积光柱，不影响 CSM）。

## 18. 数值阴影探针（2026-10-26 11:51 构建，不做看图）

玩家要求不再靠截图判断。新增 `svp_probe_shadow_numeric()`：在 `r__dump_shadow_buffers 1` 时读回 `rt_Position`、`rt_Accumulator`、`rt_smap_depth`，对屏幕 60px 网格逐点：
- 从 G-buffer 深度重建世界点 `Pw`；
- 用三条级联矩阵算 `(u,v,dp)`；
- 读阴影图深度 `dm`；
- 判断 `sh = dm < dp`（该点是否被 CSM 判为阴影）；
- 由 `dm` 反投影出阴影图里遮挡物的世界坐标 `occ`，并算 `side = (occ-Pw)·to_sun_h`：**side<0 表示遮挡物在太阳的反方向（阴影图内容错了）**，side>0 表示遮挡物在太阳一侧（影子物理正确）；
- 同时记录该像素太阳 pass 亮度 `lum`（低=暗）。
日志行格式：`* [DBG] probe x=.. y=.. z=.. c=.. u=.. v=.. dm=.. dp=.. sh=.. lum=.. occ=(..) side=..`。构建 11:51，28,674,560 B，md5 `e4820ae2028c6148c0dd16b514e59a95`，已暂存；11:33 备份 `AnomalyDX11AVX_sunmask1133.exe`。

## 19. 探针读取修正：阴影图是 D24S8（2026-10-26 12:03 构建）

首次探针运行（11:51 构建）日志有 1968 行 `probe`，但全部 `dm=0.0`、`sh=1`、`occ` 落在太阳灯位置——原因是读回格式判断错误：`rt_smap_depth` 的格式是 **D24S8（DXGI fmt 44 = R24G8_TYPELESS）**，探针把 4 字节打包的 24 位 UNORM+8 位 stencil 当 float 读。修正：按 fmt 分支解码——`D32_FLOAT/R32_FLOAT` 读 float，`R24G8/D24_UNORM_S8` 读 `(raw & 0xFFFFFF)/16777215`。同时 probe 行增加 `d=<dumpSeq>` 与 `pw=(世界坐标)`，便于三次 dump 分组和同一世界点跨视角对照。

构建 12:03，28,675,072 B，md5 `98cfd1f0a4b11b657223023038b250b9`，已暂存；11:51 备份 `AnomalyDX11AVX_probe1151.exe`。**需要再跑一次 dump**（日志里的旧 probe 行无效）。判读不变：`side<0` = 阴影图投影物在太阳反方向（错）；`side>0` = 物理正确。

## 20. 逐级联阴影图捕获 + 探针（2026-10-26 12:35 构建）

问题：帧末探针只能读到"最后驻留"的 FAR 图，c0/c1 行都是拿 FAR 图配 NEAR/MIDDLE 矩阵算的，`u/v` 越界被跳过或结果无意义。修复：把捕获移到级联渲染内部。

- `R_sun.cpp`：`render_sun_cascade()` 在 `accum_direct_cascade()` 之后、下一级联重置 smap 之前调用 `DumpShadowMapCascade(cascade_ind)`（`#if RENDER==R_R4` + `extern bool g_svpDumpSmap`）；标志由帧末 dump 块置位，下一帧逐级联消费，c2 后清零。
- `r4_R_render.cpp`：`svp_dump_rt` 增加深度格式分支（D24S8 `(raw&0xFFFFFF)/16777215`、D32/R32 float），因此 `dbg_smap_c0/1/2.bmp` 是真正的深度图灰度；`DumpShadowMapCascade` 输出 `* [DBG] smap c=N seq=... cam ... sun_dir ...` + 该级联 `r0..r3`，并以 `onlyCascade=N` 调用探针；帧末探针改为只测 FAR（`mode=2`），避免无效的 c0/c1 帧末行。
- 构建 12:35，28,676,608 B，md5 `d10bbfe719f1bd9e77258568c1d2d6d8`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；12:03 探针构建备份 `AnomalyDX11AVX_probe1203.exe`。R2/R3 工程也编译 `R_sun.cpp`，调用点必须用 `RENDER==R_R4` 隔离（首次构建 C3861 即此原因）。
- **判读注意**：`side=(occ-Pw)·to_sun_h` 在与采样矩阵自洽时恒 >0（`dm<dp` ⇒ 反投影点必在光源一侧），**它不能单独证明影子方向正确**。真正有信息量的是：`occ` 的世界坐标/高度/到相机距离（该级联图里实际存的是哪个位置的遮挡物）、`dm` 与 `dp` 的差值（贴合/自阴影/远距离遮挡）、`sh` 与 `lum` 是否对应、以及两次不同朝向 dump 中同一 `pw` 世界点的 `sh` 是否稳定（世界锚定 vs 视角绑定）。

## 21. 12:45 重建：探针网格 40px + 每份 dump 自述当前图层

- 探针网格 60→40 px（单次 dump 采样点约 2.25 倍，日志仍是几百 KB 级），提高薄影带命中率。
- dump 块新增一行 `* [DBG] layers sss_q=(...) ao_q=.. il_q=.. fog_scatter=.. shafts_mode=.. wind_tree=(...)`。原因：MCM 在 11:42/12:09 会话里把 AO/IL 重新打开（启动 cvar 转储为 4/32），靠配置文件无法判断当帧真实状态；此行使每次 dump 自证图层。
- 构建 12:45，28,676,608 B，md5 `cbe68c7b060bc9f7dc5f15105b4c1cfd`，已覆盖暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；12:35 逐级联构建保留为 `AnomalyDX11AVX_percascade1235.exe`。

## 22. 12:49 最终构建（暂存文件名带 dump 序号）

- `dbg_smap_c*.bmp` 改为 `dbg_smap_d<seq>_c<N>.bmp`：一次会话做两次 dump（两个朝向）时第一组阴影图不再被覆盖。
- 构建 12:49，28,676,608 B，md5 `0d4a2a6e690943c8dea099f062efbff0`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；12:45 构建备份 `AnomalyDX11AVX_probe1245.exe`。

## 23. 12:59 真机两组 dump 判读 + 遮挡物可见性探针（2026-10-26 13:28 构建）

**会话确认**：日志 `[MT-CN] engine build ... exeSize=28676608`（12:49 构建）；`[DBG] layers sss_q=(12 4 0 1) ao_q=4 il_q=32 fog_scatter=1.00 shafts_mode=1 wind_tree=(11.10 0.17 0.90 0.10)` → 直接 SSS 已关，AO/IL 仍被 MCM 打开，体积光柱开，风开。

**两组 dump**：d1 1440x900，cam `(-48.57 5.24 -97.44)` dir `(-0.978 -0.169 -0.120)`，sun_dir `(-0.8845 -0.4393 0.1573)`；d2 960x600（内部分辨率变了），同位置 dir `(0.224 -0.236 -0.946)`，sun_dir `(-0.8812 -0.4438 0.1628)`；两次朝向差约 94°，可见地面几乎不重叠（世界 2m 网格重叠 2 格），无法做同世界点跨视角 A/B。

**级联方向（c0，近景）**：cascade0 的 z 列 `(r0[2],r1[2],r2[2])=(-0.001701,-0.000845,0.000303)`，归一化 `(-0.8841,-0.4392,0.1575)`，与 `sun_dir(-0.8845,-0.4393,0.1573)` 分量差 <0.0005 → 近级联光轴 = 太阳方向，再次证明 CSM 没有旋转/翻转。

**有效样本（过滤 dp 越界后）**：d1 c0 329 有效、175 阴影；d1 c1 602/385；d1 c2 1488/474；d2 c0 201/82、c1 245/158、c2 672/359。d1 c0 中 caster 高度 <1m 的 49 个（近地/地形级），≥5m 的 73 个（树冠）；d2 c0 82 个阴影中 81 个 caster ≥5m（全部树冠）。

**关键异常：d1 画面中央偏左有一条细影带（内部像素 x≈520，y≈520–600）**。三级联一致判为阴影；从阴影图反投影出的遮挡物是同一根竖直细物体：`occ≈(-58.3, 1.7–4.0, -95.8)`，距相机约 10m、位于正前方、直径约 0.3m。该遮挡物方向与太阳一致（影子朝远离太阳方向延伸，长度≈高度/ tan26°）。同列 G-buffer 采样 `(520,600),(520,640)` 显示的是 14–16m 处地面（`pw≈(-59.8,0.9,-95.5)`、`(-58.4,0.8,-95.8)`），即 **阴影在 10m 处的起点附近没有可见物体**。40px 网格可能从细树干旁 10px 处擦过，尚不能定量定案 → 新增遮挡物可见性探针。

**探针修正/扩展（12:49 → 13:28）**：
- `sh` 仅在 `dp∈[0,1]`（级联正交体内部）时判定；之前 `dp>1` 且 `dm=1.0` 的“阴影”是越界样本（光照 pass 被级联立方体裁掉，不参与渲染）。
- 新增 `cv=(u,v)` 与 `dOcc`：把 `occ` 投影到屏幕，读该像素的 G-buffer 世界点 `Pvis`，`dOcc=|Pvis-occ|`。真树干的相机面与光面差通常 <1–2m；投影落到 10m 外的地面 → 该“遮挡物”在可见画面里不存在。每轮输出 `probe summary ... sh1= caster_near= caster_far= caster_off=`。
- 构建 13:28，28,678,144 B，md5 `d5fa352c6a4131937fbfb08c495b50ad`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；12:49 构建备份 `AnomalyDX11AVX_cascadeprobe1249.exe`。

**待测**：同一观景位置再跑一次 `r__dump_shadow_buffers 1`，看 `mode=0` 里细影带那几行 `cv/dOcc`：`dOcc` 小=遮挡物可见（影子本身没错，只是杆子暗/被挡）；`dOcc` 大=阴影图里有画面中不存在的 caster（阴影 pass 内容/可见性剔除不一致）。同时 `caster_far > 0` 的具体行会直接指出其他类似“无物体影子”。

## 24. 13:47 dOcc 轮：探针投影矩阵自检失败 + 14:01 修正（2026-10-26）

**会话**：`exeSize=28678144`（13:28 构建）。两组 dump 均 1440x900；相机 `(-48.57,5.24,-97.44)`，d1 dir `(-0.982,-0.137,-0.129)`，d2 dir `(-0.838,-0.164,-0.520)`，sun_dir 约 `(-0.8856,-0.4377,0.1555)`。级联光轴仍与 sun_dir 一致。

**summary**：d1 c0 `sh1=142 near=0 far=37 off=105`，c1 `360/0/43/317`，c2 `448/0/65/383`；d2 c0 `108/1/17/90`，c1 `261/1/33/227`，c2 `329/1/51/277`。`caster_off` 占多数符合太阳在相机背后的几何（caster 多在相机后方，无法投影）。

**探针缺陷（本轮数据不可用，不是阴影结论）**：所有 `dOcc` 都是把 `occ` 投影到 **镜像后的屏幕 x** 上算的。证据：行 `x=880,y=520` 的 caster 距接收点仅 0.78m（`pw=(-67.59,0.45,-104.59)`、`occ=(-66.9,0.8,-104.7)`），按相机应为约 `u=0.62`，日志 `cv=(0.379,0.571)`＝`1-0.62` 的镜像；同类行（x=880, y=520/560/600）全部一致。原因：`Device.mFullTransform` 在探针时刻与 `Device.mProject*Device.mView` 不一致（view/PiP 状态下被改写或未恢复）。
**修正（14:01 构建）**：
- 探针自带 `Fmatrix camM; camM.mul(Device.mProject, Device.mView);`，不再信任 `Device.mFullTransform`；
- 每行增加 `rcv=(u,v)`＝用同一矩阵把接收点 `pw` 投回屏幕的自检；`rcv` 必须等于采样像素 `(x/pw, y/ph)`，否则 dOcc 不可信；
- 逐级联捕获只对 `TargetMain` 执行（`Target != TargetMain` 时直接返回，标志保留给主视图消费），避免 PiP lens 视图抢走 dump。
- 构建 14:01，28,679,168 B，md5 `a843041c8f71e8481a95db6178b368c4`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；13:28 构建备份 `AnomalyDX11AVX_docc1328.exe`。

**判读方法（下一轮）**：先看任意一行的 `rcv` 是否≈`(x/1440, y/900)`；是→`dOcc` 有效，再看细影带行（x≈520，y≈560）的 `dOcc` 与 summary 的 `caster_near/far`。否→投影仍未对齐，先修探针，不能拿 `dOcc` 下结论。

## 25. 14:34 轮：rcv 自检抓到探针投影镜像，改为重建公式的解析逆（14:5x 构建 879b40b8）

**14:34 会话**（`exeSize=28678144`? 实际 `28679168`，14:01 构建）：单组 dump，1440x900，cam `(-48.57,5.24,-97.44)` dir `(-0.973,-0.154,-0.173)`，sun_dir `(-0.8851,-0.4384,0.1563)`。summary：`mode=0 sh1=155 near=6 far=36 off=113`、`mode=1 353/6/41/306`、`mode=2 432/6/64/362`。

**自检结果：`rcv` 的 u 与采样像素 u 系统性互为镜像**——u 方向平均误差 0.509（最大 0.9997），v 方向误差 0.00025。例：像素 (0,0) 的 `rcv=(1.000,0.001)`、(40,0)→(0.972,0.001)。即 `rcv.u = 1 - x/W`，v 正确。所以本轮所有 `dOcc` 都是在镜像像素上读 G-buffer 得到的，**不能用于任何阴影结论**。

**根因**：探针用 `mFullTransform`（以及本地组合的 `V*P`）做正投影，而世界点重建用的是 `pos_decompression_params`（`HorzTan=-1/_11`、`VertTan=-1/_22`）的约定；两者对 x 的符号约定不一致（引擎 shader 侧使用 `mul(M,v)` 形式），因此正投影相对重建整体水平镜像。

**修正**：不再用任何矩阵做正投影，改为**重建公式的解析逆**：
`u = 0.5*(1 + view.x/(view.z*HorzTan))`，`v = 0.5*(1 + view.y/(view.z*VertTan))`，`view = Device.mView.transform_tiny(world)`。这与重建严格互逆，不可能再镜像。另在每次探针开头输出 `* [DBG] probe cam p11=.. p22=.. ht=.. vt=.. view=(..) target=active|other`，把矩阵实际数值与目标身份写进日志。
**独立验证判据**（不再依赖 rcv 自身）：修好后，树冠类 caster（up≥5m）的 `dOcc` 应普遍变小（near 明显增加）；若仍普遍 far，说明投影还有问题，不能下阴影结论。
构建 14:5x，28,679,168 B，md5 `879b40b893b2d5c2ed3e38e335bc0547`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；14:01 构建备份 `AnomalyDX11AVX_rcv1401.exe`。

## 26. 14:54 轮：探针已自检通过，抓到“无主遮挡物”复现（15:0x 构建 764b9760 加 nearvis）

**自检**：`rcv` 与采样像素最大误差 0.00046（平均 0.00025）→ 解析逆投影与重建严格互逆，探针可用。日志头 `* [DBG] probe cam p11=0.9811 p22=1.5697 ht=-1.0193 vt=-0.6371 view=(-0.1483 0.9921 -0.1471) target=active`（注意 `HorzTan` 为负：引擎 shader 侧重建约定与 `mFullTransform` 正投影在 x 上反号，这就是上一轮镜像的根因）。

**本轮 summary**：`mode=0 sh1=149 near=22 far=16 off=111`、`mode=1 370/23/27/320`、`mode=2 449/17/48/384`。c0 的 38 个可投影阴影样本，**遮挡物全部比接收点更靠近相机**（不存在“被接收点自身遮挡”的解释）。

**分高度 dOcc（c0/c1/c2）**：
- `up<1m`：c0 可投影 32，near 22 / far 10，中位 1.8m → 地形级遮挡物确实可见，探针判读正确。
- `up 1–5m`：c0 5 个 near 0；c1 13 个 near 1；c2 17 个 near 1，中位 8.9–16.8m。
- `up≥5m`：c0 1 个、c1 5 个、c2 20 个，near 全 0，中位 31.8–90.8m（高树冠被近处地形挡住属正常，单独不作证据）。

**关键复现（x=480 列，c0）**：`occ=(-58.2, 5.9/-95.5)`、`(-58.2,3.9,-95.5)`、`(-58.2,2.5,-95.6)`，距相机 9.8/9.9/10.2 m，而该像素 G-buffer 可见面距离 `dOcc=90.8 / 21.7 / 6.4` m。与 12:59 轮细影带的遮挡物 `(-58.3, 1.7–4.0, -95.8)` 是**同一世界位置**（相机只偏了几度）。两次独立运行 + 已自检的探针 → 阴影图里存在一根相机前约 10 m、离地 2–6 m 的竖直遮挡物，而 colour pass 在该处看不到任何东西。

**下一步工具**：新增 `CRender::DumpNearbyVisuals(35m, 400)`，在 dump 时遍历各 sector 的静态 visual 层级，输出 `* [DBG] nearvis i v=.. type=.. name=<.ogf> s=(..) r=.. d=.. box=.. sh=<shader>`。用它直接判定 (-58.2,~1–6,-95.5) 处到底有没有模型：有 → 被可见 pass 剔除（HOM / 全细节 / SSA 差异）；没有 → caster 来自静态层级之外（动态或 `STYPE_RENDERABLESHADOW`），改查投影物提交。构建 15:0x，28,684,800 B，md5 `764b97602cec00757682532a2315dd34`，已暂存；14:49 构建备份 `AnomalyDX11AVX_doccfixed1449.exe`。

## 27. 15:16 轮：无主遮挡物对上静态模型（#0 MT_TREE_PM），下一步查是哪道剔除吃掉它

**自检**：`rcv` 最大误差 0.00046（与上一轮一致），dOcc 可信。summary：`mode=0 sh1=155 near=23 far=17 off=115`、`mode=1 387/23/27/337`、`mode=2 461/24/46/391`。

**nearvis 生效**（`dbg_name`/`dbg_shader` 在 release 里是 `(null)`，但 type+box 足够定位）。把 `mode=0` 里 `dOcc>3` 的遮挡物点逐个与 93 个静态 visual 的 AABB 求交：

| 采样像素 | 遮挡物世界点 | up | dOcc | 包含它的静态 visual |
|---|---|---|---|---|
| (520,520) | (-58.5, 5.1, -95.9) | 4.2 | **111.1 m** | **#0 type=11(MT_TREE_PM)**、#10、#84 |
| (520,560) | (-58.4, 3.4, -95.9) | 2.5 | 13.9 m | **#0**、#2、#10、#84 |
| (520,600) | (-58.3, 2.2, -95.9) | 1.3 | 4.6 m | **#0**、#2、#10、#84 |

`#0`：`s=(-58.8 16.8 -95.5) r=16.4 box=(-60.2 0.4 -97.1)-(-55.6 33.2 -91.9)`，即相机正前方约 10 m、高 33 m 的树。它的 AABB 完整包住这根"无主遮挡物"，阴影图也真的画了它，但 colour pass 在该像素看到的是 4.6–111 m 之外的表面 → **该树被可见 pass 吃掉、却仍进阴影图**。

其余 `dOcc>3` 的行（x=1000–1200、up≈0.8–1.4 m、dOcc 3.3–8.7 m）只被地形 visual（#10 MT_PROGRESSIVE、#83/#84 MT_NORMAL）包住，属于贴地起伏处"相机看到的是台阶近侧"的正常情形，不作异常证据。`dOcc<=1.1` 的对照组全部 up≈0.3–0.4 m，判为可见，探针判读方向正确。

**机制候选**（shadow pass 与 colour pass 的剔除差异，代码上只有这两处不对称）：
1. `add_Static` 里 `if (i_mask[fl_normal]) if (!HOM.visible(vis)) return;` —— 级联 mask 的 fl_normal=false，**阴影 pass 不做 HOM**；
2. `R4FullDetailRejectStatic(pVisual, i_mask[fl_normal])` —— 同样只在可见 pass 生效（`r4_full_detail_distance_scale 0.5`）。

**下一步工具（构建 15:2x，28,685,312 B，md5 `ddf595c0741c0be779622425b4073425`，已暂存）**：`nearvis` 每行增加可见 pass 的判定结果 `ssa=<值>(disc <r_ssaDISCARD>) frust=<0/1> hom=<0/1> fdrej=<0/1>`（frustum 用 `Device.mFullTransform` 重建、HOM 用无副作用的 `visible(Fbox3&)`、fdrej 按 `R4FullDetailRejectStatic` 同式复算）。判读：
- `hom=0` → HOM 误剔，修 HOM/其包围体测试；
- `fdrej=1` → 全细节剔除与阴影 pass 不一致，把同一判定也用于级联（或修其距离条件）；
- `frust=0` → 相机视锥本就不收它（但同像素 G-buffer 又无物体，需继续查投影物提交）；
- 三项都放行 → 模型确实进了可见图元队列，问题在绘制（MT_TREE_PM 的 progressive LOD / alpha 丢弃），转查 LOD 路径。

## 28. 15:33 轮：HOM / 全细节 / SSA 全部排除，剩下"是否进队"（15:5x 构建 9a2707d2 加 subC/subS）

**自检**：`rcv` 最大误差 0.00046。本轮跑了三次 dump（d1/d2/d3），`mode=0` 分别 `sh1=155/…/52`，`nearvis` 每次 93 条。

**判定结果（93 个静态 visual）**：`(frust,hom,fdrej)` 统计 = **92 × (1,1,0)**，1 × (0,1,0)（唯一 frust=0 的是 #1，`s=(-41.3,19.9,-92.6) r=20.0`，包围盒大部分在画面上方之外，属正常视锥外）。`r_ssaDISCARD` 实测 **0.000** → SSA 丢弃本会话根本没开。

**ghost 树复现且未被任何剔除解释**：
- d1 (320,360)：`occ=(-58.3,5.1,-95.3)`，up=4.2，`dOcc=59.7 m`，包含者 `#0 t11 fr1 hom1 fd0 ssa1.108`；
- d1 (320,400)：`occ=(-58.3,2.7,-95.3)`，up=1.8，`dOcc=7.1 m`，同样 `#0`；
- d2 (320,400)：`occ=(-58.5,2.6,-95.2)`，`dOcc=6.6 m`，同样 `#0`。
即相机前约 10 m 的那棵 `MT_TREE_PM`（box y 0.4→33.2）在三次运行里稳定给出"阴影图有、画面没有"的遮挡物，而它通过了我能算的全部剔除判定。

其余 `dOcc>3` 行（x=600–800，up≈0.4–1.3 m，dOcc 4.0–12.1 m）只被地形 visual（#10 `MT_PROGRESSIVE`、#83/#84 `MT_NORMAL`）包含，属贴地台阶"相机看到近侧表面"的正常情形，不作异常证据。

**结论**：HOM 误剔、全细节距离剔除、SSA 丢弃三条全部排除。剩下两种可能，只能靠"是否真的进队"区分：
1. 颜色 pass 根本没把这棵树排进图元队列（`m_static_seen` 去重、sector/portal 遍历、`rimp_select_sh_static` 选空、`MT_TREE_PM` 的 LOD 分支）；
2. 排进了队列但绘制阶段没出像素（progressive LOD 细节、alpha 丢弃、渲染状态）。

**新工具（构建 15:5x，28,687,360 B，md5 `9a2707d29aee66b64c5a38ed803920ba`，已暂存）**：`r__dsgraph_build.cpp` 增加按帧清零的两张集合 `g_submitted_colour` / `g_submitted_shadow`，在 `r_dsgraph_insert_static` 走到入队前记录（`fl_normal` → 颜色，`fl_shmap` → 级联），`nearvis` 每行输出 `subC=<0/1> subS=<0/1>`。上一版备份 `AnomalyDX11AVX_verdict152x.exe`。

## 29. 15:49 轮 + 截图：ghost 树"入队但无像素"，把 subC 改成计数并加 shader 元素（16:0x 构建 888994c5）

**15:49 会话**（`exeSize=28687360`，一次 dump）：`mode=0 sh1=126 near=19 far=14 off=93`。ghost 复现：
- (480,560) `occ=(-58.2,4.0,-95.5)` up=3.1 `dOcc=22.8` 遮挡物投影像素 内部(468,459)/显示(624,612)，`dcam_occ=9.9 m`；
- (480,600) `occ=(-58.2,2.6,-95.5)` up=1.7 `dOcc=6.6` 投影 内部(474,562)/显示(632,749)，`dcam_occ=10.2 m`；
- 两行的包含者都是 `#0 type=11(MT_TREE_PM) frust=1 hom=1 fdrej=0 subC=1 subS=1`。
附近网格行的 G-buffer：(440,480)/(480,480) 画的是 27 m 处的地面，(440,440)/(480,440) 是 39 m 处地面 —— 预测树干部位没有 10 m 处的表面。

**截图 15:49:42 对照**：细影带下缘（显示 ~ (670,945)）到上缘（~(670,640)）之间是空地；显示 (660-700, 400-640) 有一棵**发暗的树干**，其根部在显示 y≈640 → 按投影约 24–27 m，正是 G-buffer 量到的那层地面，不是 10 m 的 #0。也就是说影子看起来"指向"那棵远处的暗树干，但它的真实投影物在 10 m 处的空位上 —— 与玩家"影子和树对不上"的描述一致。

**#0 的 AABB 是 4.6×32.8×5.2 m（y 0.4→33.2）的树**，进了颜色 pass 的插入流程、也进了阴影级联，但颜色画面里它自己位置上什么都没有 → 损失在"入队之后/绘制之中"，或它根本没被排进真正会被渲染的队列。

**仪表缺陷已修**：上一版 `subC` 是在 pass 循环之前打标记，`sh->passes[i]` 全为 null 时也会记 1，等于没证明任何事。现在：
- 计数点移到循环内 null 检查之后，`subC/subS` 变成**实际入队的 pass 数**；
- 每行再加 `se=<element> npass=<sh->passes.size()> prio=<iPriority> vs=<名> ps=<名>`（`rimp_select_sh_static` 在 `phase==PHASE_NORMAL` 下返回 `E[SE_R2_NORMAL_HQ/LQ]`，级联在 `PHASE_SMAP` 下返回 `E[SE_R2_SHADOW]`，两遍用的本来就不是同一个元素，所以颜色元素是否为空必须直接看）。
- 注意：`bStrictB2F`/`isWater`/`bWmark` 分支在插入函数里提前 return，不走这条计数；若某树是 strict-B2F，`subC=0` 也可能已排在 sorted 队列里 —— 判读时结合 `prio/npass/vs/ps` 一起看。
构建 16:0x，28,689,920 B，md5 `888994c552293a673e05fc3ada3d890a`，已暂存；上一版备份 `AnomalyDX11AVX_subcs155x.exe`。

## 30. r__sun_shadow_casters 开关 + shoffset 影子偏移量测（2026-10-26 16:3x 构建 be19e90d）

按玩家要求把"分类 A/B"和"数值 Δ"合进同一轮。

**1) 投影物开关**（只作用于太阳阴影级联，颜色 pass 不受影响，因此它不可能让物体消失，只能让它的影子消失）
- `src/Layers/xrRender/xrRender_console.cpp`：`ps_r__sun_shadow_casters`（0..3），命令 `r__sun_shadow_casters`；
- `r__dsgraph_build.cpp`：`CDSGraphManager::sun_caster_allowed(pVisual, sh)`，仅当 `i_mask[fl_shmap] && !i_mask[fl_normal]`（太阳级联图）时生效；分类：树=`MT_TREE_ST/MT_TREE_PM`，地形=`sh->flags.isLandscape/bLandscape` 或 `shader->E[0]->flags.isLandscape`；
- 档位：`0 all`、`1 no_trees`、`2 terrain_only`、`3 none`；
- 计数 `g_sun_caster_kept/skipped` 每帧在 `render_sun_cascades()` 开头清零，dump 时输出 `* [DBG] sun casters mode=.. kept=.. skipped=..`，开关是否真的生效由数字证明。

**2) shoffset：直接量"影子相对物体挪了多少"**（`r4_R_render.cpp`）
- `CollectNearbyVisuals(radius, out)`（从 `DumpNearbyVisuals` 抽出）；`DumpShadowMapCascade` 每级联在探针之后再调 `svp_measure_shadow_offsets(...)`，此时 `rt_smap_depth` 正是该级联自己的图；
- 对每个"包围盒高 >8 m、距相机 <40 m"的静态物体：`base=盒底中心`，`L=h/tan(太阳仰角)`（影长），`probe=clamp(L/2,4,18)`，`expect=base+hdir*probe`（影子应在此处）；
- 横向扫描 ±10 m（0.4 m 步）：每个候选点先投影回屏幕、读 G-buffer 确认该处地面可见（`|Pvis-P|<=1.5 m`）才计入，再用该级联矩阵+阴影图判 `dm<dp-0.001`；取最长连续阴影段 → `band/dlat/width/dpos`；
- 再沿太阳方位从 `base` 扫到 `1.5L`（0.5 m 步）→ `along=first..last`（正确应约 `0..L`）；
- 输出 `* [DBG] shoffset d=.. c=.. v=.. h=.. base=(..) L=.. probe=.. ground=.. band=.. dlat=.. width=.. dpos=(..) along=..` 与汇总 `scanned/reported`。
- 判读：`dlat` 对所有物体近似相同且明显 >1 m，或 `along` 起点系统性 >0 → 共用变换错（A 类）；只有树类物体异常、地形/岩石 `dlat≈0 along≈0..L` → 树的投影物路径错（B 类）；`band=0` 且 `ground` 很小 → 该处地面被遮，样本无效，不作证据。

构建 16:3x，28,696,576 B，md5 `be19e90d2b0bfca1aabf10856e2930e6`，已暂存 `MODS\MT-CN\bin`；上一版备份 `AnomalyDX11AVX_shaderelem160x.exe`。

## 31. 17:47 轮 shoffset v2 判读 + 单机位 caster 开关（2026-10-26 18:1x 构建 b0024a5d）

**17:47 轮（构建 c5ec7592，exeSize=28696576，模式 0）**：`shoffset` 有 `groundY=` 了，量法成立但**不足以定案**。
- c0（size 20 m）12 行全部 `band=0 dlat=0.00` —— probe 距离被夹到 20 m，正好在 20 m 级联边缘外，这一档对本次量法没有信号；
- c1/c2 一致的数：`band=1` 的 7 行 `dlat=0.20/0.40/3.00/3.20/3.40/4.20/4.60`，`along` 起点 0.0/0.3/5.4/6.4/20.3。
- **为什么不能定案**：`dlat` 是"阴影带中心到物体包围盒中轴的水平距离"。森林 probe 点上 ±14 m 的窗口里，任何一棵邻树的树影都能占据那一段，所以 `dlat=3~4.6 m` 既可能是错位，也可能只是"旁边那棵树的影子"。它不能区分这两种情况，因此我不再往这个量法上加精度。

**离线复核（`_smap_tree_check.py`，读 `dbg_smap_d1_c0/1/2.bmp`）**：把 46/66 棵高树的盒顶中心用日志里的 `r0..r3` 投进各自的图，取回该纹素的深度与 `dp` 比较。
- 纹序约定由写入端确定：BMP 底朝上、内存行序即纹素行序 ⇒ PIL 数组 `row=纹素 v`、`col=纹素 u`（与探针 `v=0.5-0.5*ndc_y` 一致，D3D 里 ndc_y=+1 落在第 0 行）；
- c1 多数树顶处 `dm-dp≈+0.11`，折成光线长度 0.11*560=**62 m**，正好是 32.8/0.4384 的树高差 —— 即"该处只有地面，没有这棵树"；另一部分行 `dm<dp`（比树顶还近），说明那一列里是**别的**树冠；
- 但这台仪器本身有硬限制，见下条，所以"偏移了多少纹素"没有从图里给出可信数字。

**关于"会不会读的是模板不是深度"**：读的是深度平面（`raw & 0x00FFFFFF`，fmt=44 = R24G8_TYPELESS，低 24 位深度、高 8 位模板），模板位被丢弃。三点依据说明这不影响判读：① D3D11 着色器不能采样模板，`accum_sun_*` 只做 `dm<dp`；② 实测 `r__sun_shadow_casters 3` 时三档 `sh1=0`，影子完全由深度图内容决定；③ 本仓库 CSM 路径（`R_sun.cpp`、smap pass）没有模板写入，用模板的只有 `r4_R_render.cpp:1646/1746`（MSAA 边缘/emissive，属 G-buffer）。
真正丢的是**精度**：`col3` 模长反出三档深度范围 521/560/660 m，8 位灰度 ⇒ 每级 2.0–2.6 m；树冠与地面的落差（62 m）够看，亚米级偏移不够看。另外 c1 里 `dm=1.0000` 不是"空"，是那块地面本身在远平面外被夹到 1.0。

**结论：换仪器。** 加 `r__sun_shadow_casters 4` —— 整张太阳阴影图里只允许一棵树当投影物，于是图里"比地面近"的唯一连通块必然属于它，偏移量可以唯一归属，不再受邻树污染。
- `xrRender_console.cpp`：命令范围 0..3 → 0..4，注释加 `4 only_one_tree`；
- `r__dsgraph_build.cpp`：新增全局 `dxRender_Visual* g_sun_caster_only`，`case 4: keep = (pVisual == g_sun_caster_only)`；
- `r4_R_render.cpp`：dump 块里在 **arm 帧的帧末**从 `CollectNearbyVisuals(45)` 选"最高的树"作为唯一投影物，打印 `* [DBG] single caster v=.. h=.. box=(..)-(..) dist=..`（下一帧的级联构建就用它过滤）；
- 新增 `svp_single_caster_blob(...)`，在 `DumpShadowMapCascade` 里按级联调用，**用完整 24 位深度**（不是 BMP）算：全图深度直方图取 90 分位为 `ground`、`thr=ground-0.02`、掩码质心 `(u,v)`、最近深度纹素及其位置，并把该树盒的 8 角用同一矩阵投成 `predict=(u,v)`；
  输出 `* [DBG] blob d=.. c=.. v=.. ground=.. thr=.. n=.. centroid=(..) predict=(..) dtex=(..,..) dmin=.. at=(..) top_pred=(..,.. d..) corners=..`。
- 判读：`dtex` 三档一致（按各档 size 折算成米后相同）→ 世界/矩阵侧共用变换错；`dtex` 随相机转向变 → 与视角耦合的采样错；`dtex≈0` → 阴影图本身是对的，问题在累加/采样链，方向立刻改到 `accum_sun_*`。

构建 18:1x，28,700,160 B，md5 `b0024a5d76b9bba4f6b18d6ae6c0a700`，已暂存 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；上一版备份 `AnomalyDX11AVX_shoffset2_1735.exe`（`c5ec7592…`）。注意备份 `_shoffset1_1706.exe` 与它 md5 不同，是 16:3x 那次（`be19e90d…`）。

## 32. 18:24 轮：mode 4 无效的两个原因 + 草阴影绕过过滤 + CSM 贡献量级

**构建 b0024a5d 的 mode 4 没有量到东西，两个原因都查清了：**
1. `* [DBG] sun casters mode=4 kept=0 skipped=1640/1700` —— 从扇区根列表选出的树指针 `v=FD57E6040 h=40.7 box=(-72.9 -4.5 -80.5)-(-68.0 36.3 -73.7)` 在级联插入路径里**一次都没出现**。扇区列表给的是组级 visual，`sun_caster_allowed` 收到的是另一层指针（组 vs 部件），身份比较必然失败 → mode 4 退化成 mode 3。修法：**候选从过滤器内部记录**（ scouting 帧放行所有树并记录 `pVisual`+盒高，`render_sun_cascades()` 结束时选最高者锁定），新增 `R_dsgraph_pick_single_sun_caster()` 与 `* [DBG] single caster picked v=.. h=.. from N candidates (box ..)`；锁定物若某帧 `kept==0` 则自动重新侦察，并每帧打印 `sun casters mode=4 frame=.. kept=.. skipped=.. locked=..`。
2. **草阴影绕过投影物过滤**：`R_sun.cpp:336` 的 `Details->Render()`（`rsGrassShadow && cascade_ind <= ps_ssfx_grass_shadows.x`）不走 `r_dsgraph_insert_*`，所以 mode 3/4 都不能把它排除。证据：kept=0 时 c1 仍有 28569、c2 有 4463 个非清除纹素（`dmin=0.91334`，最近纹素在 (579,0) 边缘）；d=2 转向后为 0。**现成命令可控**：`r__enable_grass_shadow on/off`（`CCC_Mask`，`xrRender_console.cpp:2154`）→ 实测时先 `off`，图里就只剩被过滤的那一个 caster。blob 行已加 `grass=%d` 自证。

**18:24 轮相机核对**：d=1 `dir=(-0.968 -0.212 -0.138)`、d=2 `dir=(-0.340 -0.122 -0.933)`（同位置转向约 44°），两轮都 kept=0 → 本轮 blob 数字全部作废。

**17:23 轮四张 A/B 截图逐像素相减（同相机 pos/dir 完全一致，`_caster_ab_diff.py`，屏蔽小地图/HUD 后 1,976,832 px）**：
| 对比 | 变化 >10 亮度的像素 | 这些像素平均 \|Δ\| | 带符号均值 | 画面平均亮度 |
|---|---|---|---|---|
| mode0 − mode3（全部 CSM） | 62.9% | 41.2 | −20.6 | 37.4 → 50.3 |
| mode0 − mode1（树贡献） | 51.0% | 46.3 | −21.6 | 37.4 → 47.7 |
| mode0 − mode2（非地形贡献） | 51.4% | 40.0 | −9.7 | 37.4 → 42.4 |
- mode 3（无任何投影物）画面仍有一大块明暗结构（亮度 50.3），说明"路面亮带/两侧暗"里有相当一部分不是 CSM；但 CSM 打开后全画面 63% 像素改变、平均暗 20.6 亮度 —— **太阳阴影图是地面观感的主导项**，与"整个世界影子都错"的体感一致；
- 树的贡献（51% 像素、平均 46 亮度）远大于此前"细影带+树冠斑点"的定性描述，两类投影物各自都能改变半屏。

构建 18:4x，28,701,696 B，md5 `56ffe07c08396d4381f60c9192f466a7`，已暂存 `MODS\MT-CN\bin`；18:12 那版备份 `AnomalyDX11AVX_blob1_1812.exe`。备份改名对齐时间线：`_shoffset1a_1635`（be19e90d）/`_shoffset1b_1706`（c5ec7592）。

## 33. 19:40 静态闭环深挖：全世界影子错位 + 影子绑视角（DLSS 关也错位）

**背景与前提修正**：用户两次纠正焦点——不是 ghost 树单例，是石头/建筑/树等**所有物体**的影子整体错位；用户判断「CSM 不是某个参数给错，而是把所有物体影子给到了角色视角，并不是物体本身位置」。用户要求「读代码不要只读一段，有关代码要全文读」。19:36 日志（`xray_admin.log`）确认：测试构建 = 19:18 部署版（`exeSize=28701696`，含 mFullTransform 顺序修复）；`[Upscaler] Off` 在 7377 行运行时生效（**DLSS 真关**，native 1920x1200）；`ssfx_sss_quality (12,4,0,1)` z=0（**SSS 方向层真关**）；TAA 13:05 轮已真关排除。→ DLSS/SSS/TAA/omni(0/8)/rain/sun_original 全部洗清；`r__sun_shadow_casters 3` 时影子全无 → 影子 **100% 来自 CSM 深度图**。

**本节全文读完的代码**（非片段）：
- 引擎侧：`R_sun.cpp` 1-405；`accum_direct_cascade` 364-729；`blender_light_direct.cpp`（R4 全）；`CameraManager::ApplyDevice` 全文 + ActorCameras.cpp:699-706 调用树；`r__dsgraph_render` 矩阵序列；`u_compute_texgen_screen`；`pos_decompress` binder（r4.cpp:127-164 全）；shader 缓存 CRC 机制（r4.cpp:2100-2210）；SUN_QUALITY 映射（`st_opt_medium=1`）。
- 着色器侧（Anomaly 整合包真文件；**CCCC 版作废——CCCC 是 OGSR 游戏**）：`accum_sun_near.ps`（SSFX 23.5 版 + Enhanced Shaders 版，两版核心链一致）；`gbuffer_stage.h` 180 行全；`shadow.h` 全部（52-101/242-387/405-457）；`accum_volumetric.vs`；`check_screenspace.h`。

**「影子绑视角」的每种代码机制逐一排除**：
| 机制 | 排除依据 |
|---|---|
| mInvView 失步/单位阵 → 采样落视空间 | `Device.mView` 全库仅 3 个写入点（ApplyDevice 的 build_camera_dir + WallmarksEngine.cpp:474/605 同帧借还）；mInvView 只在 ApplyDevice 内同步 invert（CameraManager.cpp:492）；无 svpcam 污染路径 |
| smap 内容被相机矩阵污染 | `r__dsgraph_render_graph_sorted` 对每物体 `set_xform_world(*item.pMatrix)`，V/P 保持 R_sun.cpp:338-339 的 identity/combine → 顶点 = combine·(W_obj·v)，世界空间正确 |
| 矩阵顺序错 | 19:18 已修且全库统一（device.cpp:434 = mProject·mView） |
| G-buffer 重建双重逆变换 | `gbuffer_stage.h` 注释明确 `P = View space position`，`gbd.P = P.z·(pos2d·(sx,sy)-(HorzTan,VertTan))` 视空间重建；`I.tc.xy/I.tc.w` 精确还原屏幕 UV |
| pos_decompress FOV 不同源 | 从**激活投影** `mProject._22/_11` 推 tan（注释即为修 PiP FOV 不一致而写），与渲染 G-buffer 的投影同源 |
| PCSS 屏幕空间搜索半径 | SUN_QUALITY=1 走 `shadow_pcss` medium = 3x3 **固定 texel 核**（9 采样 ±2 texel），无 blocker search、无屏幕空间半径 |
| sunfilter 旁路链 | 仅 `-sunfilter` 命令行参数（`Core.ParamsData`），未用 → 死路径 |
| 旧缓存字节码 | r4.cpp:2192-2196 有**源码 CRC 校验**，不匹配重编译并 Msg |
| texel 对齐只作用一侧 | align（R_sun.cpp:257-297）直接作用于 `cull_xform`，`cascade.xform = cull_xform`（299），渲染/采样同元 |
| `ssfx_shadow_bias` 全局偏移 | (0.4, 0.03, 0) → 法线偏移仅 3cm，标准偏见 |

**结论：静态链 100% 闭环**（矩阵同元、G-buffer 语义、VS/PS、texgen、缓存机制、采样核全部自洽），但症状存在 → 只剩两种可能：**(a)「自有引擎无此问题」前提**（整个调查基石，从未严格控制验证）；**(b) 运行时 binder 值错误**（代码对但值错，静态阅读够不到）。附带：Weapon.cpp logScope 的 Msg 26 参数 vs 24 占位符错位（`end=` 起字段不可信，`zoomtype=-536870912` 实为 m_fRTZoomFactor 浮点位模式）——一直在给调查喂假数据。

## 34. 19:50 B 计划：`r__shadow_probe` 数值见证仪器（用户确认 A 前提成立）

用户确认 A 已验证（自有引擎 exe + MT-CN gamedata 同场景无问题）→ 病确实在我们引擎 → 执行 B：一次采集把「哪一环的值没跟上相机/世界」数值化。

**设计**（`r4_rendertarget_accum_direct.cpp`，`m_shadow` set_c 之后，纯只读）：对**固定世界点** P（首次启用时锁定 cam+dir×25），每 `r__shadow_probe_step` 帧（默认 30）每级联打印一行：
- `A = m_shadow·mInvView·P` —— 着色器实际会采用的阴影图坐标（相机相关）
- `B = TexelAdjust·combine·P` —— 相机无关的正确坐标（影子本该在的位置）
- `dUV` = A/B 在 sm UV 空间的欧氏距离 —— **就是错位量本身**
- 附 `cam / invT / xf_t / p22 / rsw / fov / sun`（mInvView 平移、级联盒平移、投影 _22、渲染分辨率、FOV、太阳位置）

**判据表**：dUV 随**旋转**增长 → mInvView 旋转陈旧；随**平移**增长 → mInvView 平移陈旧（ApplyDevice 时机）；`xf_t` 不随相机 → 级联盒未重拟合；`A≈B` 全程 → 采样链数值正确，转查 smap 内容/累加盒。far 遍 dUV 含 tsm bias 小常量偏移（设计使然，只看趋势）。

**构建与部署**：`build_mtcn.cmd AVX NOPAUSE` 1m02s 0 错误；28,704,256 B，md5 `c269fb397ecedfadff00acbed5c4cf68`（19:54），已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；mFullTransform 修复版（66308baf，28,701,696 B）备份 `AnomalyDX11AVX_mtfx1918_1954.exe`。**测试前需 JSGME 停用→启用 MT-CN**（重挂后 exeSize=28704256）。

**测试协议**（一次运行约 2 分钟）：进游戏到老位置腰射站定 → `r__shadow_probe 1` → 原地站立 2s → 右移 1m 停 2s → 右转 30° 停 2s → `r__shadow_probe 0` 退出 → 日志判读。

## 35. 20:03 probe v1 判读：引擎矩阵链数值正常，dUV 是仪器自身 bug；v2 攻 G-buffer 重建链

**v1 数据**：175 条 probe 行（f=8070→10440，约 4 分半；相机从 (-48.57,5.24,-97.44) 移动到 (-50.06,0.37,-89.93) 并旋转；本次 DLSS 开着——`[DLSS] ACTIVE frame=320 scene=1440x900 jitter=(0,-1/6)`，quality 档，`rsw=1440`；SSS z=0；级联 20/60/160）。

**v1 有效结论（矩阵链数值体检通过）**：
- `invT` 恒等于 `cam`（如 (-48.57,5.24,-97.44)）且随相机移动同步更新 → **mInvView 无陈旧、ApplyDevice 每帧正常调用**（排除「ApplyDevice 时机/条件断」）；
- `xf_t` 随太阳位置合理微动（(6.2,9.2,0.9)→(6.2,8.8,0.9)，sun=(393,225,-177)）→ 盒拟合链在跟踪；
- `p22=1.5697` 与 `fov=65°` 自洽（1/tan(32.5°)=1.5696）。

**v1 的 dUV 巨大（near 6.41 / mid 2.14 / far 0.80 UV 单位）是仪器自身的实现错误，不是引擎缺陷**：v1 把**世界点**喂给 `mInvView`（相机→世界变换），得到 `cam + R·P_world`——一个模约 127 的伪向量（P 距世界原点 127 m），再喂进期待「盒内 10 m」的 combine，UV 超界到 (4.11,-4.43)，与观测量级完全吻合。正确写法是 `m_shadow·(mView·P_world)`；而按矩阵结合律 `TexelAdjust·combine·mInvView·mView·P = TexelAdjust·combine·P`，**修正后的 A 与 B 数学恒等**——v1 的 A/B 对比设计本身不独立（教训：设计数值见证时必须先检查两个被比较量是否数学独立）。

**附带排除（本轮全文复现验证）**：`ssfx_jitter` binder 逻辑（Blender_Recorder_StandartBinding.cpp:1261-1296）——DLSS 路径 `ClipJitter(RasterJitter(),{RenderWidth,RenderHeight})` 优先 return；TAA 路径 `TAA_Offset[dwFrame%4]/Device.dwWidth×ps_ssfx_taa.y`（显示尺寸基准）；两条路径的施加（`hpos+j·w`）与移除（`pos2d-=j·(screen_res·0.5)`）各自单位自洽 → jitter 链排除。

**v2 设计（r__shadow_probe 升级，20:09 构建）**——三个新指标：
1. `dUV`（mView 版）：A=m_shadow·mView·P vs B=TexelAdjust·combine·P 自检，应≈0（非 0 = m_shadow 构建有暗伤）；
2. **`gbrErr`（核心新指标）**：复现 `gbuffer_stage.h` 的位置重建——用 probe 点的屏幕 UV（mFullTransform 投影）+ 激活投影推的 tan + `RenderScreenWidth/Height` + 当前 jitter，算出引擎侧 gbd.P，与真实 `mView·P` 对比。**这是唯一还没数值化的运行时链**（引擎矩阵全对，但若重建出的视空间位置错，采样照样全错，且误差天然「绑视角」）。判据：gbrErr 随分辨率比例缩放 → screen_res/RT 尺寸不一致；仅举镜出现 → 镜头遍投影不一致；随 yaw 旋转 → jitter/视图矩阵不一致；
3. 三方尺寸对账：`RenderScreenWidth/Height` vs RT 对象 `get_width/height` vs `Device.dwWidth/Height`。

**构建与部署**：`build_mtcn.cmd AVX NOPAUSE` 1m00s 0 错误；28,705,280 B，md5 `0fd1961ef556508ee5c3cf2b2676ac2c`，已覆盖 `MODS\MT-CN\bin\AnomalyDX11AVX.exe`；v1 probe 构建备份 `AnomalyDX11AVX_probev1_1954.exe`（c269fb39）。**测试前需再次 JSGME 停用→启用 MT-CN**（游戏 bin 还是 v1）。测试协议同 v1：站立 2s→右移 1m 停 2s→右转 30° 停 2s，`r__shadow_probe 1` 后跑。

## 36. 21:39 轮判读：**根因找到** —— R4 `pos_decompression_params.HorzTan` 符号反了，屏幕空间重建 x 轴镜像

**仪器本轮首次有效**：`single caster picked v=FDC04DC60 h=40.9 from 1149 candidates`、随后每帧 `mode=4 ... kept=1 skipped=1732 locked=FDC04DC60`（上一轮的"扇区指针≠级联指针"已解决）；`blob d=1 c=2 ... n=1304 centroid=(1971.0 1789.1) predict=(1855.7 1717.3) dtex=(115.4 71.8) dmin=0.53111 top_pred=(1882.4 1676.4 d0.53005) corners=4`，`grass=0` 自证草已关。c0/c1 `corners=0` 是因为选中的树在 165 m 外（20/60 m 级联不覆盖）——选优条件要加"盒中心距相机 <25 m"。

**probe v2 的 144 行给出决定性数值**：`dUV=0.00000`（`m_shadow` 构造无暗伤），而 `gbrErr` 逐条精确等于 `2×Pview.x`：
| f | Pview | gbrErr | 2·Pview.x |
|---|---|---|---|
| 20970（站定，屏幕中心） | (0.00,0.00,25.00) | 0.0164(0.0088,-0.0138) | 0（残差=jitter 项） |
| 21300 | (-0.78,-0.18,25.00) | 1.6042(1.5644,0.3551) | 1.56 |
| 21900 | (-14.21,0.22,21.33) | 28.4331(28.4295,-0.4514) | 28.42 |
| 22140 | (-23.18,-1.56,10.83) | 46.4727(46.3680,3.1168) | 46.36 |
即**重建出的视空间 x 是真值的镜像**（误差=2|x|，中心为 0、越偏边越大、随视角搬家）。

**代码层闭环（三方对齐）**
- 着色器（Anomaly 整合包三份 `gbuffer_stage.h` 完全一致，SSFX 23.5 / 热成像 / R.A.K）：`gbd.P = P.z*(pos2d*pos_decompression_params.zw - pos_decompression_params.xy)`，`pos2d = I.tc*screen_res`（x 左→右，y 上→下）。
- 引擎 binder 三版对照：R2 `r2.cpp:99-102` 与 R3 `r3.cpp:102-105` 都是 `VertTan=-tan(fov/2)`、`HorzTan=-VertTan/fASPECT`＝**正**；R4 `r4.cpp` 现在是 `-1/_11`＝**负**。因 `_11=_22*fASPECT`，原式的投影等价形是 **+1/_11** —— 幅值相同、只差符号。
- 代入：HorzTan 取正时 `P.x = z*h*(2px/W-1) = z*h*ndc.x` 与 `view.x = ndc.x*z/_11` **完全相等**；取负时整体镜像。y 的符号两版一致，故只有 x 被镜像。

**引入点（git 证据）**：`d404b95d9`（初始 MT 引擎）与 `9dc5a5b58`（DLSS 移植）均为正号；`8710b80c4 fix(pip): Publish each view's FOV, and derive the position decompression from the projection` 把 `-VertTan/fASPECT` 直译成 `-1/_11`，符号翻错。这也解释了 §33 的"静态全文闭环却症状仍在"：每个文件单独看都自洽，错在跨层符号约定；且**镜像对往返自检不可见**——14:0x 那轮"mFullTransform 投影出镜像 u、改用重建公式解析逆"其实是把引擎缺陷当成了仪器约定，反而把它藏了起来（已在该处注释改正）。

**修复（构建 `bc0f2568…`，28,705,280 B，已暂存 `MODS\MT-CN\bin`；probe v2 备份 `AnomalyDX11AVX_probev2_2009.exe`）**
1. `r4.cpp`：`HorzTan = +1.0f/Device.mProject._11`（fallback 保持 `-VertTan/fASPECT`），并写明与 R2/R3 同号的原因；
2. `r4_rendertarget_accum_direct.cpp`：probe 同步取正号；`uvy` 改为 `0.5-0.5*ndc.y`（pos2d.y 自上而下）；去掉 probe 自己的 jitter 再减一次（`mFullTransform` 本就未加抖动，减两次正是双重补偿）；
3. `r4_R_render.cpp`：诊断侧 `HorzTan`/`R.horzTan` 同步取正，注释改为记录"镜像是引擎缺陷不是仪器问题"。
影响面：R4 下所有读 `pos_decompression_params` 的屏幕空间效果（太阳阴影采样、SSAO、IL、SSR、雾散射、 decals 定位）此前 x 均镜像，修复后一起变正——若某个效果修完反而"看着变了"，那是同一常量被纠正，不是新问题。

**下一步（一次验证即可）**：重挂 → 进同一片林地看影子是否已经贴住物体；同时 `r__shadow_probe 1` 跑 3 个动作，期望 `gbrErr` 在 x/y 上都只剩 jitter 量级（≈0.01–0.02 m）且不再随视角增长。

## 37. 22:52 轮：M82A1M 准星变粗定因 + 外骨骼手表黑屏定因（两项以 MT-CN 覆盖文件交付）

**工具生效**：`r__dump_section` 一次运行拿到 603 行解析后配置（含每键来源文件），绕开了 `db\*.xdb` 的字典压缩（实测非 zlib/lzma/bz2，外部无法解）。

### 1) M82A1M（段 `wpn_m82a2`）镜内准星随放大变粗
`wpn_m82`(207 键) vs `wpn_m82a2`(210 键) 全量 diff，来源同为 `w_m82.ltx`：**放大相关键完全相同**（`scope_lense_fov=2.7083`、`fov_base=21.6667`、`zoom_step_count=6`、`scope_dynamic_zoom=true`），M82A1M 只多 3 个键：
`s3ds_reticle_type = 5`、`ms_count = 3`、`ms_colors = scope_black,scope_red_dim,scope_red`；其余差异是名字/图标/格位。
机制：`z_3d_scopes.script:152` 以 `get(info,"s3ds_reticle_type","0")` 取值拼进 `s3ds_param_3` 送着色器 → **缺省 0**。所以 M82 走 `reticle_type=0`（准星烤在瞄准镜贴图里，随贴图缩放，视觉恒定），M82A1M 走 `5`（着色器按**角度**画线）→ 放大到 2.7° 时同角度线宽占的像素约 ×8 → 十字变粗。`mod_system_s3ds_defaults.ltx` 默认 `s3ds_reticle_size=4`、`s3ds_reticle_type=0`。
可选修法：① 给 `wpn_m82a2` 去掉 `s3ds_reticle_type`（一行覆盖，立刻同 M82，但图案也换成贴图那款）；② 调 `s3ds_reticle_size`（治不了随放大变化）；③ 改 3DSS 着色器让线宽按 FOV 归一（保留 mil-spec 图案、根治，需先取到归档内的着色器源码 → 计划加 `r__dump_file <路径>` 走引擎 FS 落盘）。

### 2) 穿外骨骼后手表屏幕黑掉
`wd_exo.script:51`：`set_screen_hidden(name, spec.hide_screen and worn or blocked)`（Lua 结合为 `(hide_screen and worn) or blocked`）。而 `[PATCH] BHS 外骨骼动画 + 可穿戴设备` 自带 `mod_system_zzz_wd_exo_promin_off.ltx` 写 `[wd_exo] promin_screen = false` → `get_exo_kind()` 对任何 exo 返回 `"blocked"` → **所有**腕戴设备的屏幕被隐藏（键名叫 promin_screen，作用域却是全部）。手表没有外骨骼内置屏可替代 → 黑。日志佐证：`[WD GFY Exo] Enabled v1.2.0: GFY/MHP built-in suit monitor + native Promin UI.`、`outfit=kristiano_kx1_exo_outfit visual=wpn_hand_exo pack=gfy mode=promin attached=true`。
**已交付**：`MODS\MT-CN\gamedata\scripts\wd_exo.script`（原文件逐字节副本 + 一处收窄 `blocked and spec.id == "promin"` + 载入标记 `printf("[MT-CN] wd_exo override active...")`）。设备 id 实测为 `promin/watch/vektor/bracer`，`vektor` 带 `hide_screen=true`。整合包该模块在 `db\scripts\*.xdb` 内、`gamedata\scripts\` 下无同名散件，故散件覆盖成立；标记行未打印即说明覆盖未生效。

### 3) Promin（矩形那块，用户称 PDA）任何护甲下都不显示内容
点亮条件 `d_promin.script:40`：`is_worn() and is_powered() and not is_screen_hidden()`。非外骨骼时 `blocked=false, worn=false` → `hidden` 必为 false，故黑屏只可能是另两门之一。
关键事实：**`wd_battery` 把物品耐久当电量** —— `condition = item:condition()`，`critical = power_critical`（`mod_system_promin.ltx:28` = 0.05），戴着就掉（`is_draining = is_worn()`，速率取 `![power_consumption] wd_promin = 0.02,0.002,0` / `AMPER_FACTOR=1000`），掉到 5% 以下 `is_dead` → 屏幕灭，**修耐久才回**。
裸手臂穿模：日志 `outfit=_none visual=wpn_hand_no_outfit pack=none mode=unsupported attached=false` —— BHS 补丁对无护甲手臂判 unsupported，模型退回 WD 默认偏移（`d_bracer_config.script` 的 `DEFAULTS.model_pos`），属该补丁未覆盖情形，非引擎问题。
**已交付**：`MODS\MT-CN\gamedata\scripts\zzz_mtcn_promin_probe.script` —— 每秒读 `d_promin.is_worn/is_powered/get_power_ratio/get_page` 与 `wd_attach.is_screen_hidden("wd_promin")`，仅在组合变化时打印 `[MT-CN] promin screen gates: worn= powered= hidden= ratio= page= lit=`，一次运行即可指名是哪一道门关着。本轮无需任何控制台命令。

## 38. 23:30 轮：覆盖生效、耐久假设被证伪、Promin 黑屏收敛到"门全开之后"

**覆盖文件确认可用**：`[MT-CN] wd_exo override active` 在两次脚本初始化时都打印（同一日志里 `engine build` 只有 1 次 → 是脚本被重新加载，不是两次开机），且没有 Lua 语法错误 → 散件 `gamedata\scripts\wd_exo.script` 压过了 `db\scripts\*.xdb` 里的同名模块，这条覆盖路径成立（以后 mod 脚本都能这么改）。

**上一条"耐久当电量"的推断被本轮数据否定**：日志 `Spawned item [wd_bracer]/[wd_promin_2]/[wd_vektor_2] inside invetory` 说明设备是新刷出来的（condition=1.0），探针也报 `powered=1`。所以 Promin 黑屏与 `power_critical` 无关，该假设对本用例不成立（机制本身仍真：`wd_battery` 确实用 `item:condition()` 当电量）。

**本轮探针读数**：`worn=0 powered=1 hidden=0`（22:52 那次后期出现过 `worn=1`）。同时 BHS 补丁 `zzzz_wd_gfy_exo_screen.script:160` 的 mode 语义是 `not profile → unsupported / worn → promin / 否则 builtin`，本轮日志正是 `mode=builtin attached=true` → **那次运行里 Promin 根本没被穿戴**，外骨骼显示的是内置空屏（`wd\promin\promin_t1` + `wd_gfy_exo_blank` 空着色器），黑屏属预期，不算故障。
→ 因此"门全开（22:52 `worn=1 powered=1 hidden=0` + `mode=promin`）时屏幕是否仍无内容"才是待判的病例；若是，故障在门之后：`d_promin` 的 UI 模块/挂点，或外骨骼玻璃材质替换链（`GLASS_TEXTURE=act\cod_ghosts_exo_computer_glass`、`BLANK_SHADER=wd_gfy_exo_blank`）。

**仪器修正**：引擎 Lua 的 `printf` 只替换 `%s`，v1 传了 `%.3f`/`%d` 被原样打印（`ratio=%.3f ... lit=%d`），`ratio/page/lit` 三值丢失。探针 v2 全部改用 `string.format`/`table.concat` 后以单个 `%s` 输出，并新增：`attached`（`wd_attach.has_attachment("wd_promin")`）、`uiModule`（`d_promin_config.get_ui_module()`）、`uiGet`（`_G[module].get_ui` 是否为函数）、手表侧 `watch hidden/attached`（直接验证外骨骼修复是否放开了 `wd_metro_watch`）、`exo`（`wd_exo.is_exo()`）。另在覆盖的 `apply_hide_flags()` 里按设备打印 `wd_exo hide: kind= device= name= model= screen=`（只在换护甲时触发，量极小）。

## 39. 23:46 轮：探针 v2 全量读数 —— 症状 1 的真因改判为 `set_model_hidden`

换到外骨骼那一刻的逐设备决定（覆盖脚本内打印）：
```
kind=blocked device=watch  name=wd_metro_watch model=true screen=false
kind=blocked device=promin name=wd_promin      model=true screen=true
kind=blocked device=vektor name=wd_vektor      model=true screen=true
kind=blocked device=bracer name=wd_bracer      model=true screen=false
```
- **收窄生效**：`kind=blocked` 时手表 `screen=false`（原版会把它一起屏蔽），只有 promin/vektor 被屏蔽。
- **但"外骨骼下手表消失"的主因是同一函数上一行的 `set_model_hidden(name, worn)`**：它对所有设备隐藏模型（`model=true`）。设计意图是外骨骼自带前臂硬件、由 BHS 补丁重建 Promin 那块屏，而手表没有替代物 → 直接看不见。也就是说 screen 屏蔽只是次要一半，**model 隐藏才是主因**。是否同样收窄到 promin 待定（代价：外骨骼前臂上 WD 手表模型可能与自带硬件重叠穿模）。

Promin 侧五道检查全通过：`worn=1 powered=1 hidden=0 ratio=1.000 page=health attached=1 uiModule=d_promin_ui uiGet=true lit=1`，非外骨骼时 `model=false`（模型可见）。所以"无内容"不在这些门里，剩余嫌疑是外骨骼材质链（`wd_gfy_exo_blank` 空着色器 + `act\cod_ghosts_exo_computer_glass`）与 mode 选择（`worn→promin / 否则 builtin`）。

**穿模原因（并更正一处我的错误猜测）**：`d_bracer_config.PATCHES[community]` 是**阵营贴花纹理**，与位置无关。真正的偏移来自 `wd_config.new{global_key="bracer"/"watch", family_fields={model_pos,model_rot}, defaults=DEFAULTS}`：非外骨骼手臂只有**一套 DEFAULTS**（手表 `model_pos={0.0002,-0.0077,0.1237}`），BHS 只为外骨骼视觉给了 per-model profiles（`wd_gfy_exo_profiles`）→ 换手臂模型就没有匹配偏移 → 穿模。属该 mod 的适配缺口，非引擎渲染问题。修法两条：往 `store[global_key]` 写按护甲的偏移（经 `wd_transforms.apply/serialize`），或把 BHS 的 profile 表扩到非外骨骼手臂。

## 40. 10-27 00:2x：`r__dump_file`（走引擎 FS 读归档内文件 + 报来源层）

热成像一事先搁置（用户决定）。手表/PDA 继续查必须读着色器，而磁盘上确认没有散件：`find` 过 `crt.ps/crt.s/crt_vektor*` 与任何 `shaders/**/wd/` 目录 → **全部只存在于 `.xdb` 字典压缩归档内**，外部工具（含 `C:\STALKER-HH`，实为翻译插件工具包）都读不了。

**已知要读的目标**（由 WD 脚本给出，不是猜的）：`d_promin_config.script:5 SCREEN_SHADER = "wd\crt"`、`d_vektor_config.script:2 SCREEN_SHADER = "wd\crt_vektor"`、`wd_screen.script:1 SHADER = "wd\ui"`；绑定方式 `wd_screen.new_binder(shader)` → `wnd:InitTextureEx(tex, shader)`。BHS 侧另有 `wd_gfy_exo_blank.ps/.s`（散件，可作 source=loose 的自证样本）。

**新命令** `src/xrGame/console_commands.cpp`：`r__dump_file <fs_root> <relative\path>`
- `FS.r_open(root, rel)` 取覆盖顺序胜出者 → `fwrite` 到 `$logs$\dbg_file_<basename>`；
- 同时用 `FS.exist(resolved, root, rel)` 拿 `file::vfs`：`-1` 报 `loose`，否则在 `FS.m_archives` 里按 `vfs_idx` 找到并打印**归档路径**；再加长度与 FNV-1a crc，用于区分两个 mod 的同名副本；
- 主菜单即可执行，不必进关卡。
构建 `2fc55ba6db4deb3a932aa21af990765e`（28,717,568 B）已暂存 `MODS\MT-CN\bin`；上一版备份 `AnomalyDX11AVX_dumpsec_2151.exe`（`65e1eb8e`，含 `r__dump_section`）。

**编译期踩到的两处**（本仓库类型习惯）：`xr_char` 不是可见类型名（用 `char[]`/`string32`）；`string_path` 是 `char[1024]`，**没有 `.c_str()`**（`FS.update_path`/`Msg` 直接传数组）。

## 41. `r__dump_file` 两次崩溃的真因：`IReader::close()` + `FS.r_close()` 双释放

用户更正崩溃命令是**带路径那条**（不是 usage），据此重查：`CLocatorAPI::r_close(IReader*&)` 在 `LocatorAPI.cpp:1389-1395` 里做的是 `xr_delete(fs)`；我在 `FS.r_close(R)` 之前又调了一次 `R->close()`，等于释放两遍。堆破坏不会立即崩，**要到下一次分配才炸**，所以栈顶指向 `Msg`（log.cpp:42），把我前两次"修复"全带偏（先怀疑 `*cName`、再怀疑 `pointer()` 越界——`*cName` 确实是类型错、`pointer()` 也确实不可用，但都不是这两次崩溃的原因）。本仓库能跑的老代码（`svp_dump_shader_provenance`）只调 `close()` 不调 `r_close`，我照抄 `close()` 又"规范化"补了 `r_close`，两者不可并存。

现在：只用 `FS.r_close(R)`；并且**每一步落盘到 `appdata\logs\dbg_file_steps.txt`**（`fclose` 即刷新）——日志是缓冲的、崩溃时不刷盘，这正是前两次连一行 `[FILE]` 都没留下的原因。构建 `e1d65578bff1c629e721318b0bb80375`（28,717,056 B）。`r__dump_section` 不碰 reader，不受影响。
