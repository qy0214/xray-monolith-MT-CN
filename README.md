# MT-CN 分支：面向 **Anomaly 1.5.3「Anthology 2.1 剧情整合包」** 的引擎适配

**本分支 = 上游 X-Ray Monolith (MT) 引擎 + 本仓库自己的适配层。**

**目标**：让整合包（剧情 / 武器 / 画面类 MOD）在该引擎上**全部可用**，并在其上提供 **DLSS / FSR3 原生上采样** 与 **同帧 PiP 3D 镜内视图**。

**部署方式**：仅写入整合包的 `MODS\MT-CN\`（由 MO2 覆盖）；**不写入整合包自带 `bin\`，不修改任何 MOD 文件**。

## 一、新增功能（简要）

| 功能 | 说明 |
|---|---|
| **原生 DLSS / FSR3 上采样** | 引擎内直接接入（NGX / FidelityFX FSR3）：抖动(jitter)、运动矢量、历史失效，以及"双渲染目标 + 镜内视图"下的正确降级 |
| **同帧 PiP 3D 镜内视图（3DSS / PiP 3DSS）** | 镜内与主画面**同一帧**渲染（第二套渲染目标），镜内永远是最新画面；镜头相机 FOV、曝光归属、后处理归属均有明确契约 |
| **整合包兼容层（compat）** | 读取并驱动 MOD 自己约定的 cvar：`scope_lense_*`、`r__svp_*`、`r4_upscaler_*`、`s3ds_*` 等（整合包 MCM 可直接调整） |
| **渲染统计面板** | 分阶段 CPU 耗时 + 上采样器实际输入/输出尺寸（含"举镜提升内部分辨率"状态） |
| **诊断开关** | `r__svp_debug 0/1/2`（默认 0 = 零开销）、`r__svp_lens_power` 等；|

## 二、DLSS / FSR3 出处

| | 出处 | 位置 |
|---|---|---|
| **DLSS** | **NVIDIA DLSS Super Resolution（NGX SDK）**：经 `NVSDK_NGX_D3D11_Init_with_ProjectID` / `NVSDK_NGX_*` 接入；头文件为仓库内 vendored 的 NVIDIA NGX SDK（`sdk/DLSS/include/nvsdk_ngx*.h`，许可证见 `sdk/DLSS/LICENSE.txt`）；**运行时**为游戏目录中的 `bin\nvngx_dlss.dll` | `src\Layers\xrRenderPC_R4\Dlss\` |
| **FSR3** | **AMD FidelityFX FSR 3.1.2 Upscaler（MIT）+ 社区 D3D11 backend**：`sdk/FSR3/`（含 `LICENSE.txt`、`Third_party_notices.txt`；其 `README.txt` 注明 vendored 自 OGSR 依赖 `8adb7cf1…`，**非** AMD 官方 DX12/Vulkan backend）；**运行时**为 `bin\ffx_fsr3upscaler_x64.dll`、`bin\ffx_backend_dx11_x64.dll` | `src\Layers\xrRenderPC_R4\Dlss\` |

两者**均为可选**：未启用或加载失败时自动回退原生分辨率（原因在控制台与画面设置中可见）。

## 三、整合包各 MOD「对引擎的需求」及本分支的处置（简要）

| 整合包 MOD | 它对引擎的需求 | 本分支的处置 |
|---|---|---|
| **R.A.K 武器包适配 3DSS / PiP 3DSS**（镜片着色器 + 镜内 FOV 数据 + 政策文件） | ① 材质契约：`scope_svp`（引擎是否提供第二视图）、`s_second_vp = $user$viewport2`、`scope_lense_reproject`；② `s_position` 绑到 **`$user$generic2` = GBuffer 位置**；③ 镜片着色器写入的 **TAA 跳过掩码**（`SV_Target2`）必须落到运动矢量目标的 alpha；④ 镜片 FOV 语义：`scope_lens_fov` 为**度数**、倍率 = `65 / fov`，变倍镜给出两端与级数 | ② 在假镜 / 热成像路径**发布 GBuffer 位置**到 `$user$generic2`；③ 把**运动矢量目标**绑为该遍第 3 个 MRT（掩码入 alpha、速度为 0）；①④ 按 pack 语义实现镜头相机与采样映射（见第四节） |
| **热成像（Heat Vision）/ 夜视（Beef NVGs）** | 这两者是**佩戴设备**的全屏效应（管状遮罩，无设备时遮罩=0 ⇒ 整幅黑） | **不用 `r__heatvision` 驱动"瞄具"**（实测会整幅黑）；热成像瞄具改走 pack 自己的 **3DSS 分支 `infrared()`** ⇒ 引擎**撤回第二视图**让它运行；`scope_lense_allow_nvg/thermal` 与设备桥脚本照常生效 |
| **屏幕空间着色器 23.5（SSS）** | `ssfx_fog / ao / il / bloom / taa / motionblur / water / rain` 均依赖**主相机**的深度、运动矢量与模糊链；`s_motion_vectors.a` 是 **TAA 掩码** | 第二视图（镜内）**跳过或清零**这些屏幕空间阶段（雾 / SSR / 水 / 雨 / 运动模糊 / 泛光 / AO / IL）；镜片掩码绑到 `s_motion_vectors` 的 alpha |
| **R.A.K 武器包（含永久变体）** | 瞄具参数位于 `wpn_<weapon>_<token>` 子段（无瞄具槽位时也要能解析） | 引擎**按 token 后缀走查**解析镜片参数（`scope_*` / `s3ds_*`），并读取 `scope_lense_quality_percent` 等 |
| **DLSS 适配 MOD（modxml_dlss_options）** | 通过 `user.ltx` 的 cvar 配置上采样 | 引擎提供并尊重 `r4_upscaler_*` / `r__upscaler_*` 等 cvar |
| **通用（所有依赖屏幕空间的 MOD）** | 引擎的 `Device.fFOV` 必须与"**当前正在渲染的视图**"一致 | 镜头遍**发布本视图的 FOV**（否则屏幕空间重建、SSAO、光源可见性、雨体投影、光照 LOD 都会按主画面计算） |

## 四、引擎侧契约与 BUG 修复（简要）

- **镜内曝光**：镜头遍必须使用**主套**曝光（`t_LUM_dest` ⇒ `$user$tonemap`）；此前镜头套自己那份恒为零 ⇒ 镜内近黑；
- **镜头相机 FOV**：改为按 pack 的**授权角度**精确重建投影（旧实现按倍率"比值"缩放，21.667° 的镜子被渲成 24.0°）；
- **镜内采样映射**：确认为**恒等**（同帧整幅捕获 ⇒ 无裁剪），并给出几何推导与日志判据；
- **`Device.fFOV` 随视图切换**：修好屏幕空间重建 / SSAO 噪声与半径 / 光源可见性锥 / 雨体投影 / 光照 LOD；
- **第二套渲染目标**：SSFX **AO/IL 清零**、雾 / SSR / 水 / 雨 / 运动模糊 / 泛光**按遍守卫**、模糊链归属明确；
- **名称重映射**：第二套目标全部经 `createUnique`，并把镜内图**发布到镜头材质真正采样的那张表面**；
- **镜内目标尺寸/格式**：保持显示尺寸与比例；HDR10 下格式不匹配时**拒绝创建**（避免静默失败）；
- **准星高倍尺度**：pack 声明的 FFP 在 PiP 下改用 SFP 分支，保持 3DSS 观感；
- **变倍镜**：

## 五、已知问题

1. **热成像镜快速转动时镜内出现一圈"光圈"**：**未解决**。已实测排除五条：镜片反射模型、上采样"屏幕锁定"判据、相机运动模糊、上采样器本身（关闭 DLSS 无效）、TAA 掩码路径。下一步建议：`r__svp_debug 2` 并在举镜扫视时抓取 `appdata\logs\pip_optics_screen.bmp`，按其形状定位。
2. **PiP 政策文件**（`mod_system_pip_scope_policy.ltx` 的 `scope_lense_enabled`）尚未解析；目前以"瞄具是否真的放大 + PiP 模块是否存在"作为等价门槛。
3. **部署方式**：整合包 `bin\AnomalyDX11AVX.exe` 的归属需玩家确认（MO2 覆盖 vs 拷入 `bin`）；可用日志启动戳的 `exeTime/exeSize` 判定实际运行的构建。

## 六、部署要求与运行时来源（配套归档：`dist/MT-CN/`）

本仓库的 `dist/MT-CN/` 是整合包 `MODS\MT-CN\` 的**可部署快照**（不含大体积二进制）；完整说明见 `dist/MT-CN/来源与要求.md`。

| 项 | 说明 |
|---|---|
| **`bin\nvngx_dlss.dll`**（未归档）| **NVIDIA DLSS 运行时**：产品 `NVIDIA Deep Learning SuperSampling`，版本 **310.9.1.0**（`NVIDIA DLSS - DVS PRODUCTION`），SHA-256 前缀 `3975567B8943`；与整合包 `bin\` 下为**同一份**。出处：**NVIDIA DLSS SDK（NGX）** <https://github.com/NVIDIA/DLSS> ／ <https://developer.nvidia.com/rtx/dlss> |
| **`bin\ffx_*.dll`**（未归档）| **AMD FidelityFX FSR 3.1.2** 运行时，由游戏安装提供；SDK 与许可证见仓库 `sdk/FSR3/` |
| **`bin\AnomalyDX11AVX.exe`**（未归档）| 由本仓库源码构建产出 |
| **`scheduler_batch_size = 512`（必须）**| 需同时满足两处：`appdata\user.ltx` 的 `scheduler_batch_size 512`，与 `gamedata\configs\axr_options.ltx` 的 `modded_exes/optimizations/scheduler_batch_size = 512`（整合包基座中亦已是 512）。本快照两份均为 **512**。 |

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/themrdemonized/xray-monolith)

# STALKER-Anomaly-modded-exes

这里是 Anomaly 1.5.3 用的 exe 文件集合，其中包含社区为让部分高级 mod 正常工作所需的全部引擎补丁。

# 版本

Modded Exes 分为标准版和 MT 版（目前处于测试阶段，标记为 MT-TEST）。

MT 版本集成了大量来自 [IX-Ray](https://github.com/ixray-team/ixray-1.6-stcop) 与 [OpenXRay](https://github.com/OpenXRay/xray-16) 的引擎性能改进。

平均可预期 **30%–50%** 的性能提升；如果你原本受限于 CPU，提升会更明显。

MT 版本打包在单独的 `STALKER-Anomaly-modded-exes-MT-TEST` 压缩包中，并在主菜单和日志里标记为 `MT-TEST`。

MT 版本包含下文所述标准版 Modded Exes 的全部功能，此外还有：

  * 重写的渲染图（render graph）、扇区（sector）与传送门（portal）遍历
  * 可选支持在 stalker、变异体及其他动态物体上留下弹痕（wallmark）
  * 帧间粒子插值，外观更平滑
  * 多线程化：
    * 资源加载（纹理、模型、CFORM（碰撞））
    * 草丛渲染
    * 雨
    * 粒子
    * 模型骨骼计算
    * 引擎调度器：实时更新的对象留在主线程，其余对象放到独立线程，并可配置每帧处理的批次数
    * AI 的 Feel 与 Vision
    * 任务管理器 Task Manager（默认关闭）
    * UI（默认关闭）
    * `CreateTimeEvent` 与 `AddUniqueCall` 命令的并行执行（默认关闭）
    * 日志器 Logger
  * 可在 Modded Exes 选项中开关的各个选项
  * 空闲时间并行 Lua GC
    * 渲染器准备帧时，反复以小步长调用 Lua GC，让它保持忙碌以减少后续的清理工作，从而减轻卡顿
    * 帧渲染完成后检查 Lua 内存占用是否正常，若不正常则按通常的 GC 步长执行一次大回收
  * 更新 Luabind 至来自 (https://github.com/ForserX/luabind-latest) 的最新版本
  * Lua 调用的 functor 缓存，默认关闭，实测没有性能差异
  * 增强 `smart_cast`，加入特化版本
  * 简化 `shared_str` 容器：分块字符串池、Robin Hood 哈希、更大的查找缓冲区
  * 显著缩短编译时间、减小 PDB 体积
  * 大量小修复与改进

未来的 MT 版本会加入 LuaJIT 2.1 的 64 位版本；由于它与现有存档不兼容，目前暂缓。

MT 版本的已知问题

  * 由于剔除较为激进，地图上个别位置可能出现渲染异常，例如新手村（Rookie Village）地下室入口后方
  * 加载整局游戏或读档时崩溃的概率有所上升
  * 树木可能有轻微闪烁，尤其是通过脚本修改天气参数的 mod
  * 偶发的视觉问题，例如少见的灯光、模型动画闪烁
  * DX8、DX9、DX10 版本基本未经测试，初看能正常加载与渲染
  * 部分 modpack 可能在加载时崩溃；仅在原版与 GAMMA 上测试过，二者均可正常运行

# 请务必阅读安装说明！！！

![изображение](https://github.com/user-attachments/assets/1b792ffc-127f-400f-8a2d-1f701803837d)

* **至少需要 Windows 10 1903 更新！！！**
* 安装最新的 Visual C++ 运行库：https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/
* 下载 `STALKER-Anomaly-modded-exes_<version>.zip` 压缩包。
* 把所有目录直接解压到 Anomaly 游戏根目录，提示覆盖时选择覆盖。
* 首次用新 exe 启动游戏前，先在启动器里删除着色器缓存。这一步只需要做一次。

# 疑难解答

* **如果你从早于 2025.04.21 的版本升级，并且使用了 Reshade，那么 Reshade 也需要一并升级**

* 问：如何从零重装 modded exes？
* 答：
  * 清空 `db/mods` 目录下的所有内容
  * 清空 `gamedata/configs` 目录下的所有内容，但保留 `localization.ltx`、`axr_options.ltx` 和 `cache_dbg.ltx"` 这三个文件
  * 如果存在以下目录，请删除：
    * `gamedata/materials`
    * `gamedata/scripts`
    * `gamedata/shaders`
  * 然后按上面的安装说明重新安装 modded exes

* 问：在 Linux 上 DX11 全屏会崩溃
* 答：需要在 `commandline.txt` 里加入 `--dxgi-old` 参数，或者给 exe 建一个快捷方式并在快捷方式里带上该参数

* 问：在篝火附近或开枪时帧数很低
* 答：关闭「体积光（Volumetric lighting）」，这是目前已知的问题：https://github.com/themrdemonized/xray-monolith/issues/94 https://github.com/themrdemonized/xray-monolith/issues/123

* 问：使用 Beef NVG、SSS 或 Enhanced Shaders 时出现着色器冲突、崩溃和 bug
* 答：如果你在使用这些 mod，请通过 MO2 安装这个包，并把它的优先级设置得比那些 mod 更高：https://github.com/deggua/xray-hdr10-shaders/releases/latest

## S.T.A.L.K.E.R. Anomaly 的 X-Ray Monolith 版本

----

* 基于 Open X-Ray Call of Chernobyl Edition
* 基于 X-Ray 1.6 引擎

本仓库包含基于 1.6.02 版本的 XRAY 引擎源码，以及 Call of Chernobyl 1.4.22 的改动。
原版引擎用于 GSC Game World 发行的游戏 S.T.A.L.K.E.R.: Call of Pripyat，对本引擎的任何改动仅允许用于***非商业***用途。

# 补丁清单

* 解决方案迁移到 Visual Studio 2022。如果遇到问题，请确认已安装最新的 Visual C++ 运行库。可在此处获取：https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/

* DLTX，作者 MerelyMezz，由 demonized 修改并修复 bug，与原版的差异：
  * 尝试覆盖不存在的 section 不再导致游戏崩溃，而是向日志输出信息。所有触发该错误的 section 都会被打印出来
  * 重复 section 的错误现在会打印出错误发生在哪个根文件，便于排查 mod_... ltx
  * DLTX 现在支持用同一个符号 `@` 来创建不存在的 section，或覆盖已存在的 section。
    下面是 `newsection` 未定义时的示例：先用 `override = false` 创建它，再用 `override = true` 覆盖它

  ```
  @[newsection]
  override = false

  @[newsection]
  override = true
  
  ```

  * 当参数形如 `name = item1, item2, item3` 时，DLTX 支持向该参数列表追加条目
  
  ```name = item1, item2, item3```
  
    * `>name = item4, item5` 会把 item4 和 item5 追加进列表，结果为 `name = item1, item2, item3, item4, item5`
    * `<name = item3` 会把 item3 从列表中移除，结果为 `name = item1, item2`
    * mod_system_...ltx 的示例：
    
    ```
      ![info_portions]
      >files                                    = ah_info, info_hidden_threat

      ![dialogs]
      >files                                    = AH_dialogs, dialogs_hidden_threat
      
      ![profiles]
      >files                                    = npc_profile_ah, npc_profile_hidden_threat
      >specific_characters_files                = character_desc_ah, character_desc_hidden_threat
    ```

  * 在此处可以获取 LTXDiff 工具及其脚本集，用于把普通 mod 转换成 DLTX 格式 (https://www.moddb.com/mods/stalker-anomaly/addons/dltxify-by-right-click-for-modders-tool)。

* DXML，作者 demonized
  * 可用 Lua 脚本在引擎处理 XML 文件之前修改其内容
  * 更多信息见 DXML.md 指南。

* Shader bus
  * 带 id 的 Float4 着色器常量，用来取代共享的 `shader_param_N` 槽位。脚本按名称注册一条通道（lane），通过 token 写入；任何着色器只要声明 `bus_<id>` 就能读取。原有的十二条旧命令仍然可用，同时日志会显示还有哪些脚本在写它们。
  * 更多信息见 docs/shader_bus.md 指南；Lua 模块与控制台命令列在 `lua_help_ex.script` 中。

* 支持在脚本加载前取消 Lua 变量的 local 化，使它们成为脚本命名空间内的全局变量
  * 关于如何在脚本中取消某个变量的 local 化，请参考 `gamedata/configs/unlocalizers` 目录中的测试文件说明

* 基于 Cribbledirge 的代码、由 demonized 修改的声音多普勒效应。
* 真实的第一人称死亡镜头：玩家死亡后镜头仍保留，并会随玩家头部的位置与旋转做出相应反应，其参数可调。
  * 已知 bug：
    * 如果玩家脸朝下直摔到地面，由于模型本身也会被裁剪，镜头会穿到地面以下

* 可选的玩法增强

* 更多函数与控制台命令见 `lua_help_ex.script`

* 更多回调见 `callbacks_gameobject.script`

* 由 demonized 提供的其他改动与 bug 修复
  
* 通过 StackWalker 库在日志中打印引擎调用栈 https://github.com/JochenKalmbach/StackWalker
 * 要让它生效，需要下载与你 DX/AVX 版本对应的 `pdb` 文件并放到与 `exe` 相同的位置。PDB 文件在此获取：https://github.com/themrdemonized/xray-monolith/releases/latest

* 由 Lucy 提供的修复与新功能
  * Reshade 着色器不再影响 UI，需要完整 addon 支持版的 Reshade（详见疑难解答）
  * 修复从 blender 导出的手部（不再需要重新指定 motion 引用）
  * 修复脚本中获取玩家精准度时的静默错误 / 脚本卡死
  * 动画修复（霰弹枪弹壳与装弹、关膛动画不同步）
  * 读档时物品 section 缺失不再导致游戏崩溃（现在大多数 mod 应该都能中途卸载了）
  * 修复变异体原地奔跑的问题（非常感谢 Arszi 发现）
  * 修复双手探测器 / 设备的动画（摇摆现在应用于双臂，而不再只作用于左臂）
  * 现在可以用 `:play(true)` / `:play_at_pos(pos, true)` 在 hud_mode 下播放脚本粒子特效
  * 因翻译字符串为空而崩溃时，游戏现在会显示崩溃信息
  * 脚本化调试渲染函数
  * 调试渲染器可在 DX10/11 上工作
    * 非常感谢 OpenXRay 与 OGSR 的作者：
    * https://github.com/OpenXRay/xray-16/commit/752cfddc09989b1f6545f420a5c76a3baf3004d7
    * https://github.com/OpenXRay/xray-16/commit/a1e581285c21f2d5fd59ffe8afb089fb7b2da154
    * https://github.com/OpenXRay/xray-16/commit/c17a38abf6318f1a8b8c09e9e68c188fe7b821c1
    * https://github.com/OGSR/OGSR-Engine/commit/d359bde2f1e5548a053faf0c5361520a55b0552c
  * 又向 lua 导出了一些向量与矩阵函数
  * world2ui 增加可选第三参数：即使目标在屏幕外也返回其位置，而不是返回 -9999
  * 统一了骨骼相关的 lua 函数，并提升了安全性
  * 现在可以在 lua 中获取玩家第一人称手部的骨骼数据

* 基于 LTX 的巡逻路径定义，作者 NLTP_ASHES：https://github.com/themrdemonized/xray-monolith/pull/61

* Redotix：支持 3D Shader scopes（3DSS）：https://github.com/themrdemonized/xray-monolith/pull/62

* deggua：DX11 渲染器支持 HDR10 输出：https://github.com/themrdemonized/xray-monolith/pull/63
  * 内置着色器可直接用于原版 Anomaly。若要与 SSS 和 GAMMA 兼容，请从这里下载 GAMMA 着色器 https://github.com/deggua/xray-hdr10-shaders/releases/latest

* 由 DPurple 提供的修复与新功能
  * 修复使用 `%c[color]` 标签搭配多字节字体时导致意外换行的问题
  * 崩溃前自动存档的能力，可用控制台命令 `crash_save 0` 关闭、`crash_save 1` 开启。最大存档数量可用命令 `crash_save_count <number>` 指定，取值范围 0 到 20（默认为 10）

* 更新速率可配置的平滑粒子（Smooth Particles），作者 vegeta1k95
  * 可在 .pe 文件中用 `update_step` 字段以毫秒为单位设置粒子更新步长，便于精细调节

* Shader Scopes，作者 CrookR，由 Edzan 增强，已整合进 Modded Exes

* OpenAL 1.23.1，支持 EFX，以及来自 IX-Ray 引擎的可工作声音环境

* 来自 IX-Ray 引擎的提交：https://github.com/ixray-team/ixray-1.6-stcop

<!----><a name="script_debugger_instructions"></a>
* 使用 VSCode 与 LuaPanda 调试脚本，由 IX-Ray Platform 提供支持。使用方法见：https://anomaly-modding-book.netlify.app/docs/tutorials/addons/lua-debugger

* 所有设置都可以在游戏选项的「Modded Exes」标签页中调整
![image](http://puu.sh/JC40Y/9315119150.jpg)

## 以下是为其他 mod 提供的补充编辑，mod 本体**并不包含**，请通过链接自行下载。如果这些 mod 自带 exe，你可以忽略它们 —— 本页已包含所有必需的编辑。

* BaS 引擎编辑，作者 Mortan (https://www.moddb.com/mods/stalker-anomaly/addons/boomsticks-and-sharpsticks)

* Screen Space Shaders，作者 Ascii1457 (https://www.moddb.com/mods/stalker-anomaly/addons/screen-space-shaders)

* Heatvision，作者 vegeta1k95 (https://www.moddb.com/mods/stalker-anomaly/addons/heatvision-v02-extension-for-beefs-nvg-dx11engine-mod/)

## 如何自行制作 modded exe？

如何编译 exe：
1. Fork 本 xray-monolith 仓库，主分支为 `all-in-one-vs2022-wpo`
2. 把 fork 下载到本机
3. 在下载后的目录中用 Git Bash 或终端执行 `git submodule update --init --recursive` 拉取 git 子模块
4. 用 VS2022 编译 engine-vs2022.sln 解决方案
5. 若要批量编译所有配置，使用 `batch_build.bat`
6. 要顺利编译，**需要最新且包含 MFC 与 ATL 库的生成工具**

### 在 VS2022 中使用 Hot Reload 特性以便快速反馈（作者 lulnope）：

1. 先按上面的「如何编译 exe」说明操作
2. 以管理员身份启动 VS2022 解决方案
3. 选择 VerifiedDX11 解决方案配置 **注意：该配置编译出的 .exe 处于极度未优化状态。启动游戏可能需要多达 20GB 空闲内存，游戏性能大约只有用 release 配置编译的 .exe 的 50%。好处是 Hot Reload 可用，调试也容易得多 —— 不会被优化掉的符号，也没有方法被内联，代码的执行方式与阅读方式完全一致**
4. VS2022 -> 顶部菜单栏 -> 生成 -> 重新生成解决方案
5. 编译成功后，日志输出里会给出类似这样的路径：`\xray-monolith\_build\_game\bin_dbg`
6. 把新生成的 .exe 和 .pdb 文件都复制到 Anomaly/bin 目录
7. 用新生成的 .exe 启动游戏；无论是原版 Anomaly、GAMMA 还是其他 modpack 都可以，只要用的是新编译的 .exe
8. 游戏启动后按 ALT+TAB 切回 VS2022，选择 调试 -> 附加到进程...（CTRL+ALT+P）
9. 附加到 AnomalyDX11.exe **该操作要求 VS2022 以管理员权限启动，如果没有，系统会提示你以管理员身份重新启动 VS2022**
10. 对代码做一处小改动，保存文件并执行 Hot Reload（VS2022 -> 顶部菜单栏 -> 调试 -> 应用代码更改 (ALT+F10)）**只有在成功附加调试器到运行中的进程、且 VS2022 切换到调试视图之后，Hot Reload 代码的选项才会出现**
11. 如果一切正常，你会看到一条进度条，表示正在重新编译并重载；完成后应能立刻观察到代码执行结果的差异
12. 祝你改 mod 愉快！请务必阅读 Hot Reload 文档：https://learn.microsoft.com/zh-cn/visualstudio/debugger/supported-code-changes-cpp?view=vs-2022
13. 整个流程的简短视频演示：https://youtu.be/MmZwyM2QO38

## 更新日志
**2026.10.11**
* 主分支与 MT：
  * DLTX：`reload system ini` 调用将导致整个缓存失效
  * axr_main（参见 `docs/axr_main.md）：
    * 可以更改执行顺序，在注册新回调时定义哪些回调函数应先执行或后执行。
    * 通过 `priority` 参数添加 Pre、Normal 和 Post 回调。Pre 将在所有 normal 回调之前运行，post 在其后。默认为 Normal
  * 为 Sidorovich/Forester 专门添加 `sid_forester_on_item_take` 和 `sid_forester_on_item_drop` 回调
  * 回滚 "为 "Glossy Puddles" 附加组件扩展 ps_ssfx_gloss_minmax（https://github.com/themrdemonized/xray-monolith/pull/659）"
  * `dump_cvar_enable` cvar 用于切换在启动或崩溃时转储 cvar，默认开启
  * 修复 https://github.com/themrdemonized/xray-monolith/issues/681
  * 修复 https://github.com/themrdemonized/xray-monolith/issues/729
  * damiansirbu:
    * jit.profile / jit.allocprof：加固以及 jit.util.gcstat（https://github.com/themrdemonized/xray-monolith/pull/670）
    * fix(imgui): 在调用 ImGui 之前将 InputText 缓冲区设为容量大小（https://github.com/themrdemonized/xray-monolith/pull/694）
    * fix(monsters): 防止错误的配置破坏奇美拉攻击几何体（https://github.com/themrdemonized/xray-monolith/pull/710）
    * feat(monsters): 添加逐怪物的 lua 战斗接缝接口（https://github.com/themrdemonized/xray-monolith/pull/711）
    * feat(monsters): 怪物战斗决策回调（https://github.com/themrdemonized/xray-monolith/pull/712）
    * feat(monsters): 可通过脚本为每个怪物启用闲置的后背旋转攻击（https://github.com/themrdemonized/xray-monolith/pull/715）
    * feat(monsters): 逐对象的战斗参数设置器（https://github.com/themrdemonized/xray-monolith/pull/721）
    * feat(monsters): 逐对象的 FSM 记忆读取器（https://github.com/themrdemonized/xray-monolith/pull/725）
    * feat(anim): 用 cvar 使 NPC 放松时的躯干待机动作偏离玩家姿势（https://github.com/themrdemonized/xray-monolith/pull/727）
  * damoldavskiy: 贴图从 .thm -&gt; .ltx 加载（https://github.com/themrdemonized/xray-monolith/pull/730）
  * erepb: 模块化 all.spawn 覆盖（https://github.com/themrdemonized/xray-monolith/pull/684）
  * GhenTuong: CRayPick：实现 get\_multiply\_add，修复 get\_normal（https://github.com/themrdemonized/xray-monolith/pull/704）
  * hardcoon: 支持 Pripyat Full 的大型 v13 AI 地图（https://github.com/themrdemonized/xray-monolith/pull/689）
  * lulnope: 允许控制脚步声音与粒子的播放距离（https://github.com/themrdemonized/xray-monolith/pull/720）
  * nltp-ashes:
    * feature(hud-snd-freq): 向回调添加 HUD 声音频率与延迟（https://github.com/themrdemonized/xray-monolith/pull/680）
    * fix(color-anm): 修复移除颜色动画（https://github.com/themrdemonized/xray-monolith/pull/682）
    * fix(actor-legs-shadow): 修复角色腿部阴影（https://github.com/themrdemonized/xray-monolith/pull/690）
    * feature(script-anm-shaders): 暴露脚本动画物品的 shader（https://github.com/themrdemonized/xray-monolith/pull/726）
  * NotifyDeadCushion: 添加 Lua 可访问的缩放惯性系数（https://github.com/themrdemonized/xray-monolith/pull/671）
  * TheLostInPlace:
    * 少量脚本 API 新增（关联 PR #1）（https://github.com/themrdemonized/xray-monolith/pull/674）
    * 地图与 UI：地图标记点回调、批量标记点读取、若干窗口新增以及供脚本使用的小地图控件（关联 PR # 3）（https://github.com/themrdemonized/xray-monolith/pull/676）
    * 弹匣式榴弹发射器：修复 CWeaponSSRS 中的缺陷（https://github.com/themrdemonized/xray-monolith/pull/713）
    * shader 总线：扩展、错误修复、版本号提升（https://github.com/themrdemonized/xray-monolith/pull/736）
  * Um-Yeah-No: 修复暂停时闪电闪光叠加（https://github.com/themrdemonized/xray-monolith/pull/733）
  * vacwave: 修复丢弃时物品与武器的位置和朝向（https://github.com/themrdemonized/xray-monolith/pull/734）
  * Verdatim25:
    * 反转 scale\_hud\_motion\_marks Cvar；叠加式动画；以及物品运动标记（https://github.com/themrdemonized/xray-monolith/pull/673）
    * 新增 Cvar（npc\_dont\_drop\_weapons\_on\_death），可禁用 NPC 掉落……（https://github.com/themrdemonized/xray-monolith/pull/716）
    * 修复 NPC 无法排除三态装填武器卡壳的问题（https://github.com/themrdemonized/xray-monolith/pull/717）

* MT：
  * hardcoon: 将 R4 全细节距离控制移植到 Anomaly MT（https://github.com/themrdemonized/xray-monolith/pull/683）
  * kovil-666:
    * 遍历时清除 mapLOD，修复太阳级联中 LOD 列表泄漏（https://github.com/themrdemonized/xray-monolith/pull/687）
    * 在 CScriptStorage::object 中直接查找（https://github.com/themrdemonized/xray-monolith/pull/688）
  * noisethanks:
    * 修复 vid\_modes\_string（get\_resolutions）中的释放后使用（https://github.com/themrdemonized/xray-monolith/pull/698）
    * 修复：迭代前在 creationGuard 下对 m\_textures 做快照 - #700（https://github.com/themrdemonized/xray-monolith/pull/706）
    * 修复：在各调用之间共享打开文件跟踪锁（https://github.com/themrdemonized/xray-monolith/pull/707）
    * 修复：为 CTextureDescrMngr 的 THM 加载线程实现真正的 join + 锁（https://github.com/themrdemonized/xray-monolith/pull/705）
  * TheLostInPlace: 修复脚本化挂件 UI 的渲染阶段顺序（https://github.com/themrdemonized/xray-monolith/pull/672）

**2026.09.12**
* 主分支与 MT：
  * 修复 https://github.com/themrdemonized/xray-monolith/issues/650
  * 可以在 hud 段中设置 `shell_particles` 覆盖
  * 修复 `ltx_help` 缺少新增字段
  * 新的武器升级字段，由于物品栏由脚本控制所以用处不大。
    * inv_grid_x, inv_grid_y - 物品栏图标位置，以网格单元为单位
    * inv_grid_width, inv_grid_height - 物品栏图标尺寸及占用空间，以网格单元为单位
    * upgr_icon_x, upgr_icon_y - 升级图标位置，以像素为单位
    * upgr_icon_width, upgr_icon_height - 升级图标尺寸，以像素为单位
  * damiansirbu:
    * 低开销的 CPU 与分配分析器（jit.profile、jit.allocprof）（https://github.com/themrdemonized/xray-monolith/pull/641）
    * 逐 NPC 的 npc:set_movement_hold 移动控制杆（https://github.com/themrdemonized/xray-monolith/pull/645）
    * 用于环境音床和关卡音乐的 Lua trace/veto 钩子（https://github.com/themrdemonized/xray-monolith/pull/644）
    * 用于脚本音效、天气效果、雷鸣声和雨的 Lua veto/volume 钩子（https://github.com/themrdemonized/xray-monolith/pull/661）
  * Verdatim25:
    * 允许通过脚本控制角色被变异体击中时的 time_to_lock 加速度（https://github.com/themrdemonized/xray-monolith/pull/642）
    * 允许配置脚本摄像机动画的速度（https://github.com/themrdemonized/xray-monolith/pull/643）
    * 添加一个命令，在启动游戏时禁用启动画面窗口（https://github.com/themrdemonized/xray-monolith/pull/649）
  * nltp-ashes:
    * 向 Lua 导出 CUIWindow::FindChild 并添加 UI 类型转换导出（https://github.com/themrdemonized/xray-monolith/pull/653）
    * 向 lua 导出 pp 函数（https://github.com/themrdemonized/xray-monolith/pull/667）
  * LVutner:
    * 向 shader 暴露帧计数（https://github.com/themrdemonized/xray-monolith/pull/658）
    * 为 "Glossy Puddles" 附加组件扩展 ps_ssfx_gloss_minmax（https://github.com/themrdemonized/xray-monolith/pull/659）
  * TheLostInPlace: Shader 总线 | 脚本与 Shader 之间的通道（https://github.com/themrdemonized/xray-monolith/pull/665）
  * erepb: 少量 UI 改进（https://github.com/themrdemonized/xray-monolith/pull/654）

* MT：
  * TheLostInPlace:
    * 修复 UI 标题静态量上的竞态（https://github.com/themrdemonized/xray-monolith/pull/655）
    * RenderDoc 集成（https://github.com/themrdemonized/xray-monolith/pull/662）
    * 修复 ComboBox 绘制在光标之上的问题（在 mt_ui 1 下）（https://github.com/themrdemonized/xray-monolith/pull/664）
  * JorgeVV: 在异步构建树之前发布 CFORM 数组（https://github.com/themrdemonized/xray-monolith/pull/657）
  * noisethanks: 修复/线程安全 子弹粒子（https://github.com/themrdemonized/xray-monolith/pull/660）

**2026.08.17**
* 主分支与 MT：
  * DXML: 回滚 "Malformed closing tags are silently accepted"，因为与 mod 一起会出错
  * erepb: 支持高分辨率字体（https://github.com/themrdemonized/xray-monolith/pull/623）

**2026.08.16**
* 主分支与 MT：
  * DXML 已修复的错误：
    * 格式错误的闭合标签被静默接受
    * 若干变更函数违反了它们自身的 DOM 不变量
    * 选择器求值存在正确性缺陷
  * erepb:
    * 修复使用更高分辨率时的一些视觉问题（https://github.com/themrdemonized/xray-monolith/pull/630）
    * PDA 标签页的修复与改进（https://github.com/themrdemonized/xray-monolith/pull/633）
  * Verdatim25: 新增 Cvar `scale_hud_motion_marks_by_speed` 和 `allow_accel_during_lookout`（https://github.com/themrdemonized/xray-monolith/pull/632）
  * nltp-ashes: feature(visibility-ui-to-actor): 移动可见性的权威数据源（https://github.com/themrdemonized/xray-monolith/pull/635）
  * damiansirbu:
    * 逐 NPC 的 npc:set_hit_redirect 选择控制杆（https://github.com/themrdemonized/xray-monolith/pull/636）
    * 逐 NPC 的 npc:set_visible_enemy_bias 选择控制杆（https://github.com/themrdemonized/xray-monolith/pull/637）

MT：
  * 修复崩溃 memory_manager.cpp (329): CMemoryManager::remove_links
  * 修复崩溃 PHMovementControl.cpp (167): CPHMovementControl::Calculate
  * 修复崩溃 alife_update_manager.cpp (118): CALifeUpdateManager::update

**2026.08.09**
* 主分支与 MT：
  * 在不使用 adopt 策略的情况下暴露 AttachChild（这样 lua 窗口可以挂到引擎窗口上），修复 https://github.com/themrdemonized/xray-monolith/issues/624
  * [功能请求] 在 CUIScrollView/CUIScrollBar 中暴露滚动钩子，修复 https://github.com/themrdemonized/xray-monolith/issues/621
  * damiansirbu:
    * 逐 NPC 的视距系数设置器（https://github.com/themrdemonized/xray-monolith/pull/627）
    * 逐 NPC 的被动生命恢复加成设置器（https://github.com/themrdemonized/xray-monolith/pull/628）
    * `npc:is_hit_anim_playing hit-flinch` 读取器（https://github.com/themrdemonized/xray-monolith/pull/629）
  * lulnope: 暴露脚本控制，以决定 move_to_slot 时是否激活物品（https://github.com/themrdemonized/xray-monolith/pull/631）

* MT：
  * 修复 SSS Bloom 影响 HUD 元素的问题

**2026.07.31**
* 主分支与 MT：
  * 修复问题 https://github.com/themrdemonized/xray-monolith/issues/622
  * 可以通过升级为武器添加新的可安装瞄具
  * erepb: 动态 PDA 标签页支持（https://github.com/themrdemonized/xray-monolith/pull/608）（https://github.com/themrdemonized/xray-monolith/pull/609）

**2026.07.22**
* 主分支与 MT：
  * 修复 `keypress_on_start 0` 时 `on_loading_screen_dismissed` 回调不工作的问题
  * 修复 https://github.com/themrdemonized/xray-monolith/pull/612 中提到的 `ammo_base` 缺失字段
  * Bookshelf9854: 在 anomaly_restrictor_update 中用预过滤的怪物注册表替换 level.iterate_nearest（https://github.com/themrdemonized/xray-monolith/pull/605）
  * damiansirbu:
    * 掩体重选否决回调（npc_on_best_cover_repick）（https://github.com/themrdemonized/xray-monolith/pull/607）
    * NPC 武器装填事件回调（npc_on_weapon_reload_start/stop）（https://github.com/themrdemonized/xray-monolith/pull/611）
    * 绑定 make_enemy_visible（强制潜行者记住视为已发现的敌人）（https://github.com/themrdemonized/xray-monolith/pull/613）
  * emgComplex: feat(HudItem):toggle fl_inertion_enable 的 lua 绑定（https://github.com/themrdemonized/xray-monolith/pull/619）
  * GhenTuong: 在 object 段中添加选项 "bullet_check_visual"，用于校验子弹命中。（https://github.com/themrdemonized/xray-monolith/pull/620）

* MT：
  * 可能修复了 LocatorAPI 中切换关卡时的崩溃
  * noisethanks: 通过使用 texture_load_tasks PPL 组解决重新加载被逐出贴图时的竞态条件。（https://github.com/themrdemonized/xray-monolith/pull/604）

**2026.07.13**
* 主分支与 MT：
  * erepb: fix: 点击时重放标记点添加属性...（https://github.com/themrdemonized/xray-monolith/pull/598）
  * Verdatim25: 新增 Cvar pseudogiant_dodge_stomp_while_falling（https://github.com/themrdemonized/xray-monolith/pull/599）
  * GhenTuong: CCar 开发。无人机、可视摄像机、摄像机瞄具（https://github.com/themrdemonized/xray-monolith/pull/600）
  * damiansirbu: feat(ai): 逐 NPC 的开火队列缩放设置器（https://github.com/themrdemonized/xray-monolith/pull/603）

**2026.07.06**
* 主分支与 MT：
  * 修复 `CPatrolPoint::load_from_config` 无法正常工作的问题
  * erepb:
    * feat: 从多个标记点打开的 PDA 上下文菜单（https://github.com/themrdemonized/xray-monolith/pull/585）
    * feat: 为 PDA 上下文菜单添加子菜单支持（https://github.com/themrdemonized/xray-monolith/pull/591）
  * damiansirbu:
    * feat(ai): 逐 NPC 的瞄准参数与视觉速度设置器（https://github.com/themrdemonized/xray-monolith/pull/594）
    * feat(ai): 战斗动作切换否决回调（npc_on_combat_action_switch）（https://github.com/themrdemonized/xray-monolith/pull/595）
    * feat(ai): 绑定 can_kill_enemy/member 和 fire_make_sense 开火门控（https://github.com/themrdemonized/xray-monolith/pull/596）

MT：
  * 处理崩溃 `burer_state_attack_inline.h (89): CStateBurerAttack<CBurer>::execute`
  * 在 `r__clear_resources_on_unload` 时不要清除 UI 和 `$user` 贴图
  * Leyths: alife: 在 spawn 图遍历中防范 `_SPAWN_ID(-1)`（https://github.com/themrdemonized/xray-monolith/pull/590）
  * noisethanks: feat(renderer): 会话中途贴图逐出系统（https://github.com/themrdemonized/xray-monolith/pull/592）

**2026.06.28**
* 主分支与 MT：
  * 减少 shader 缓存不匹配时的冗长日志
  * GhenTuong: 导出 CScriptGameObject 和 CWeapon（https://github.com/themrdemonized/xray-monolith/pull/584）
  * antglobes: 控制台支持小键盘（https://github.com/themrdemonized/xray-monolith/pull/583）
  * erepb: 统一军队与生态派系的地图标记点尺寸（https://github.com/themrdemonized/xray-monolith/pull/582）

**2026.06.21**
* 主分支与 MT：
  * 新增 Lua 导出，用于获取/设置 HUD 开火骨骼/位置（消音器）
  * 可以通过升级更改新字段：
fire_point, fire_point2, fire_point_silencer, hud_fire_point, hud_fire_point2, hud_fire_point_silencer
  * `motion_mark_reload` ltx 用于指定触发装填逻辑的运动标记，为空或未指定时与原版代码一样使用任意运动标记
  * shader 缓存：自动使 shader 缓存失效，无需手动清理。它会检查并保存源 CRC，若更改后 CRC 不匹配，shader 将被重新编译，灵感来自 openxray
  * Priler: 无限/超长投掷螺栓问题的临时解决方案（https://github.com/themrdemonized/xray-monolith/pull/575）
  * Leyths: CCar Lua 接口 + 针对 4 轮以上载具的物理岛选择性启用（https://github.com/themrdemonized/xray-monolith/pull/569）

* MT：
  * `r__hom_dynamic` 默认禁用
  * 处理崩溃 `stalker_movement_manager_base.cpp (308): stalker_movement_manager_base::setup_movement_params`，检查顶点有效性
  * gwalls: 修复 CCF_Skeleton::BuildState 中跨对象骨骼互斥锁死锁（https://github.com/themrdemonized/xray-monolith/pull/576）

**2026.06.12**
* 主分支与 MT：
  * 禁用信号灯与吊灯的阴影投射和体积光，与 SSS 冲突
  * 在游戏启动和崩溃时记录所有控制台变量
  * damiansirbu: `g_ai_unlimited_ammo` cvar 用于切换 NPC 的无限弹药（https://github.com/themrdemonized/xray-monolith/pull/557）
  * erepb: 修复肖像拉伸（https://github.com/themrdemonized/xray-monolith/pull/561）
  * TheLostInPlace:
    * 添加 `g_launcher_dynamic_range_zoom` 控制台变量，用于切换弹道瞄准辅助（https://github.com/themrdemonized/xray-monolith/pull/560）
    * Lua 战斗 AI 钩子系统（https://github.com/themrdemonized/xray-monolith/pull/563）
  * lulnope: 修复漂浮的神器（https://github.com/themrdemonized/xray-monolith/pull/562）
  * Leyths: 细节贴图：添加 v4 格式（14 位 id，最多 16383 个对象），读取 v3+v4（https://github.com/themrdemonized/xray-monolith/pull/564）
  * Priler: 添加 `level.remove_hud_motion_cam_effectors()`，用于取消逐动画的 HUD 摄像机效果器（https://github.com/themrdemonized/xray-monolith/pull/568）

* MT：
  * 处理崩溃 `xr_collide_form.cpp (263): CCF_Skeleton::_RayQuery`
  * 处理崩溃 `UIDialogHolder.cpp (266): CDialogHolder::OnFrame [error][      87] : The parameter is incorrect.`

**2026.06.05**
* 主分支与 MT：
  * 修复 Modded Exes 选项的错误
  * 向 Lua 导出 `db.actor:cast_Actor():conditions():GetAlcohol()`，用于获取当前酒精浓度
  * erepb: 修复 `[TR] can't switch child [%d] offline, it's null` 刷屏（https://github.com/themrdemonized/xray-monolith/pull/553）

**2026.06.01**

* 主分支与 MT：
  * 可以为 CHangingLamp 的灯光（如信号灯）设置 shadow、volumetric、ambient_shadow、ambient_volumetric 属性
  * 信号灯具有体积光并投射阴影，吊灯投射阴影
  * 用于空间限制器的 `obj:force_set_restrictor_type`，以及用于生物体的 `obj:invalidate_restrictions`
  * 为异常点导出的 `obj:is_enabled_anomaly`
  * `g_restriction_rebuild_frames` cvar 用于调整寻路重建等待的帧数，超过后会强制等待，默认 20
  * 修复 `ai_move_to_cover` Modded Exe 选项
  * 怪物躲避异常点：
    * 当怪物靠近任何异常点时，该异常点会将自身注册为空间限制器并强制怪物计算新路径
    * 取决于怪物的路径是否及时重算，它会绕过该异常点移动
  * 修复在控制台输入 `load`/`save` 时的卡顿
  * 更安全的转账脚本函数，感谢 `damiansirbu` 提供的想法
  * 使启动 logo 窗口不与其他窗口重叠，感谢 `AzuNar` 提供的想法
  * `CSE_ALifeOnlineOfflineGroup` 在 update 和 syncronize locations 调用之前强制 `actualize`
  * `xr_weapon_jam` 重构
    * 引擎调用 `GetConditionMisfireProbability`
    * `npc_get_misfire_probability` 回调用于设置概率
    * 旧代码已移植到新的回调系统
  * Leyths: PDA 地图：限制缩放缓动，并在用户平移时取消（https://github.com/themrdemonized/xray-monolith/pull/542）
  * erepb:
    * 修复为战争商人生成可用任务（https://github.com/themrdemonized/xray-monolith/pull/547）
    * 修复双重 online 切换崩溃（https://github.com/themrdemonized/xray-monolith/pull/552）
    * 修复 busyhands：移除不可保存的子项（https://github.com/themrdemonized/xray-monolith/pull/553）
    * 跳过无成员小队的更新（https://github.com/themrdemonized/xray-monolith/pull/554）
  * Skyki15: 重新启用 NPC 卡壳与描述修复（https://github.com/themrdemonized/xray-monolith/pull/550）

* MT：
  * 实现路径中无效 `level_vertex_id` 的自动修正（https://github.com/ixray-team/ixray-1.6-stcop/commit/a8d8d9bd97d77d90c8282bee41afaf6cea16c39c）
  * 修复进入 online 时无效位置的恢复（https://github.com/ixray-team/ixray-1.6-stcop/commit/5f8b2c47632e302730be1cede843d61c8e8aadd8）
  * `r__hom_dynamic` 命令用于为动态对象启用 HOM 可见性测试，默认禁用
  * `mt_SchedulerRT` 命令用于将角色及其他 RT 对象的调度器切换到第二个线程，会提升性能但会引发问题，默认禁用
  * 扩大了 `scheduler_batch_size` 的范围。
  * 默认批大小为 256。建议 GAMMA 玩家将批大小提高到 512 或更多
  * `level.scheduler_flush` 用于强制使用调度器批大小的最大值
  * 加载后的最初几秒，调度器将被强制为最大批大小，随后恢复为 `scheduler_batch_size` 的值，修复了某些 mod 偶发的错误
  * 通过内存预留优化粒子加载（https://github.com/ixray-team/ixray-1.6-stcop/commit/04459cdaf9665857a2353351ac8bd928d12306a8）
  * 可能修复了卡住的门
  * 改进粒子系统的等待循环
  * `ai_enhanced_vision` cvar 用于移除 AI 可见性的 "模糊" 检查。启用后它们的视野更激进，但可能导致穿墙看见。默认禁用

**2026.05.24**

* 主分支与 MT：
  * 回滚 https://github.com/themrdemonized/xray-monolith/pull/535 中引入的标签缩放行为变更，因为会在 GAMMA 中引发错误

* MT：
   * 回滚 `Topological sort bones, 33% perf increase (https://github.com/OpenXRay/xray-16/commit/797ba9236bb4b2e6f4f914f71bf684f0888a9479)`

**2026.05.23**

* 主分支与 MT：
  * DEFAULT_SAMPLE_COUNT 为 32，修复 https://github.com/themrdemonized/xray-monolith/issues/534
  * 修复不正确的 Modded Exes 选项字符串
  * 移除 `CSE_ALifeOnlineOfflineGroup::synchronize_location` 和 `CSE_ALifeOnlineOfflineGroup::update` 中的空检查，改用 begin 内部的 `actualize` 调用
  * trans_outfit.transparent_gg 改进（与 GAMMA 无关）
    * 当 NPC 被任何东西击中时，将视野提升到 200 米持续 2 分钟，除非角色穿着潜行服
    * 将失明检查从 120 米降低到 100 米
  * 在 `CGamePersistent::WeathersUpdate` 中为天气的 `ambient_particles` 应用随机偏移
  * Skyki15: AI 配置与修复（https://github.com/themrdemonized/xray-monolith/pull/523）
  * Leyths: PDA 地图体验优化功能（https://github.com/themrdemonized/xray-monolith/pull/535）
  * Verdatim25: 修复因 UI 菜单打开导致三态装填武器装填被打断的问题（https://github.com/themrdemonized/xray-monolith/pull/536）
  * Orleonn: CTexture: GIF 动画支持（https://github.com/themrdemonized/xray-monolith/pull/537）
  * damiansirbu: 向 Lua 提供逐角色关卡的按 clsid 过滤的对象遍历（https://github.com/themrdemonized/xray-monolith/pull/538）

* MT：
  * 不稳定的 MT 优化被移到其他项下方并带有警告标签
  * 修复粒子重复渲染（https://github.com/ixray-team/ixray-1.6-stcop/commit/30832c945e18f08a4f2279682b06e1b3c149b2b9）
  * 骨骼拓扑排序，性能提升 33%（https://github.com/OpenXRay/xray-16/commit/797ba9236bb4b2e6f4f914f71bf684f0888a9479）
  * 修复 NPC 瞄准附近玩家时的抖动（https://github.com/ixray-team/ixray-1.6-stcop/commit/7e2fbfdaa8c26aa87aac9abf97cd805ff4db5463）
  * CModelPool 的线程安全
  * 简化 `light_vis`，修复某些地方灯光闪烁
  * 修复 `r__clear_resource_on_unload 0` 时加载贴图可能导致的栈溢出
  * 修改 `phase_ssfx_sss_ext` 以在 `r2_shadow_omnipart_vischeck 1` 下正确工作

**2026.05.15**

* 主分支与 MT：
  * 第一人称死亡改进：
    * 死亡时增大 `bip01_head` 的碰撞体积，防止摄像机穿入地面
    * `first_person_death_head_scale` cvar 用于调整
  * `BusyHandsDebug`：当在主菜单中触发 busy hands 时不要弹出 UI
  * `CSE_ALifeOnlineOfflineGroup::update` 置于 try catch 块中
  * damiansirbu: 在 CSE_ALifeOnlineOfflineGroup::update 和 synchronize_location 中防范空的 m_members（https://github.com/themrdemonized/xray-monolith/pull/527）
  * Lucy: 修复使用 set_shader 时模型未编译正确 shader 的问题（https://github.com/themrdemonized/xray-monolith/pull/529）
  * Orleon: 添加 'actor_on_item_sell' 和 'actor_on_item_buy' 回调
  * GhenTuong: 回滚 xr_combat_ignore.script，修复 CCar 无人机，导出 physics_shell 和 CUIProgressBar 的函数（https://github.com/themrdemonized/xray-monolith/pull/532）

* MT：
  * 灯光：
    * `r2_shadow_lod_min` cvar 用于独立于 `r2_slight_fade` 硬性限制灯光的渲染距离，值越大渲染距离限制得越早，默认 0.02
    * `r2_shadow_omnipart_vischeck` 用于为每个点光源额外创建的 6 个灯启用可见性检查，通过不渲染不可见灯光提升性能，默认启用
    * 移除灯光可见性测试中的 while 循环，可见性结果将在下一帧获取。未决结果视为可见。
  * 自发光对象将不应用 SSA 检查，以便在不产生明显弹出感的情况下渲染得更远
  * `CPHSimpleCharacter::UpdateDynamicDamage` 加入 try catch 块，希望能避免崩溃
  * 修复 `TTestDepthCallback` 错误导致的崩溃
  * `ph_ref_object` 安全检查
  * 修复某些系统上草地闪烁，确保 MT_CALC 在渲染前完成。
  * 修复 `r2_mt 0` 在草地方面的行为
  * 恢复多线程 HOM 并增加额外的线程安全
  * 通过略微预留 UI 元素容量降低 UI 崩溃的可能性

**2026.05.08**

* 主分支与 MT：
  * 持久化天气在切换到地下关卡时不加载/保存状态
  * 当 `level.set_cam_custom_position_direction` 启用且 `r__actor_shadow_in_demo_record` 为 1 时禁用角色阴影
  * 修复与 Poltergeist（幽灵）相关时 `poltergeist_telekinesis.cpp (310): SCollisionHitCallback::call` 的崩溃

* MT：
  * `r__clear_resources_on_unload` cvar 用于在切换关卡或退出时强制从池中卸载贴图和模型。会降低显存占用但增加加载时间
  * 修复 `r__no_ram_textures` 的行为以匹配原版
  * 对即将被销毁的对象调用 `setVisible(false)`，可能修复了类似 Mags Redux mod 中视觉模型闪烁一帧的问题
  * `CSoundMemoryManager::update` 的 `m_sounds` 空指针检查

**2026.05.05**
* 主分支与 MT：
  * 在 `CDamageManager::load_section` 和 `CWeaponMagazined::LoadScopeKoeffs` 中提供更有意义的错误消息
  * `level.set_cam_custom_position_direction` 在自定义平滑为 0 时不应用 FPCam 平滑
  * 应用 `level.set_cam_custom_position_direction` 时禁用腿部渲染
  * 装填后自动开火，使用 `Level().IR_OnKeyboardPress` 而非 Actor 的输入接收器，修复 https://github.com/themrdemonized/xray-monolith/issues/521
  * 禁用 `utils_item.script` 中的缓存，修复数据陈旧问题
  * `luabind::detail::class_rep::function_dispatcher` 拥有自己的 try catch 块，会将错误重定向到 BusyHandsDebug，可能覆盖更多脚本问题
  * 武器过热烟雾脚本重构：
    * 正确使用 HUD 几何体
    * 使用 `stop_deffered` 而非 `stop` 以正确停止烟雾粒子
    * 每把武器有独立的烟雾数据，武器被丢弃时粒子仍会工作
    * 与帧率无关的累积和冷却
    * 可以为 NPC 武器启用，目前禁用，效果不够好
    * 基线调校为 PKM 连续射击 80-85 发后开始过热
  * 使用引擎天气插值的持久化天气实现
    * 保存上一个天气文件、当前天气文件以及引擎在它们之间的插值
    * 加载时先强制应用上一个天气，然后非强制地应用新天气，再应用插值
    * 可在 `Video / Weather` 选项中切换
  * 用于操控天气的新引擎导出
  * 更安全的 `pda.calculate_rankings` 补丁
  * leyten: 在高 FOV 下限制角色摄像机碰撞盒以修复超宽屏门框卡住，`g_clamp_actor_camera_collision 1` 可启用超宽屏修复（https://github.com/themrdemonized/xray-monolith/pull/520）
  * erepb: 通过 simulation_board 路由 assign_smart 以修复 SIMBOARD.smarts 孤儿（https://github.com/themrdemonized/xray-monolith/pull/522）
  * SaloEater: 动作存在性引擎调用（https://github.com/themrdemonized/xray-monolith/pull/524）

* MT：
  * 将 NPC 的 `process_sound_callbacks` Lua 回调移到 `shedule_update`，在 `mt_scheduler 1` 下它们将位于独立线程，在 NPC 众多时略微提升性能
  * 优化 `CSector::traverse` 以解决视锥中有大量传送门时（如普里皮亚季郊区）的帧率下降
  * `mt_ui` cvar 用于将 `pUIGame->OnFrame` 移到独立线程，默认禁用
  * `CPHMovementControl::Calculate` 安全检查
  * `CParticlesObject::renderable_Render` 空指针检查
  * `ISpatial::OwnerSectorPoint` 略更安全
  * `CMapLocation::UpdateSpot` 的 `m_owner_se_object` 空指针检查
  * `CAI_Stalker::process_enemies()` 的 `memory().visual().objectsPtr()` 空指针检查
  * 从 `ModelPool` 中移除遗留代码
  * 更安全的 `ModelsToDeleteDefer` 模型延迟删除流程
  * 可能修复了 `Physics.cpp (245): CollideIntoGroup` 崩溃
  * 在调用 `PSI_Destroy` 时从空间数据库中注销粒子
  * 用 `std::min` 和 `std::max` 替换 `_min` `_max`
  * 雨：
    * 修复物品池不缩减的问题，该问题会导致从雨天天气过渡时密度缩减失效
    * `r__rain_exp` 和 `r__rain_k` 命令用于控制雨的累积和最大密度

**2026.04.26**

* 主分支与 MT：
  * BusyHandsDebug：移除不必要的位置
  * `CWeaponMagazined::LoadScopeKoeffs` 在武器配置无效时打印错误消息
  * maks7231: 通过从 `level_sounds` 中移除 `occluder_volume` 修复其被重复应用的问题，该问题导致某些地方环境音非常安静
  * GhenTuong: 添加 callback.net_spawn_after（https://github.com/themrdemonized/xray-monolith/pull/516）
  * erepb: 显示器选择（https://github.com/themrdemonized/xray-monolith/pull/517, https://github.com/themrdemonized/xray-monolith/pull/518）
  * Verdatim25: 修复有运动标记的轻机枪装填、排障，并为 motion_marked tri_state_reload 武器添加能力（https://github.com/themrdemonized/xray-monolith/pull/519）

* MT：
  * 通过 `r_wallmarks_static` 和 `r_wallmarks_dynamic` cvar 禁用静态和动态墙痕的选项
  * 修复 `CObjectList::Unload` 中可能的崩溃
  * 更安全的 `stat_memory_async`，在关键位置恢复为 `stat_memory` 调用

**2026.04.21**

* 主分支与 MT：
  * `player_hud::StopScriptAnim()` 在 `print_bone_warnings` 标志下隐藏警告
  * 头灯更新优化（CTorch）：
    * 通过自定义 epsilon 限制 `set_position` 和 `set_rotation` 调用进行优化
    * 更远的对象使用更大的位置 epsilon 但相同的旋转 epsilon
    * `r__optimize_torch` cvar 用于切换优化
  * Controller 攻击修复：
    * 不再依赖 `actor_psy_immunity`，在 Anomaly 中表现正确
    * 若模型有 `left_eye/right_eye/bip01_head` 骨骼，摄像机将聚焦其上，否则回退到对象位置
    * 角色离 Controller 太近时更好的摄像机行为
  * 修复 `randI` 在范围内 `min == max` 时获取随机值可能导致的崩溃
  * 不设置线程描述，修复 https://github.com/themrdemonized/xray-monolith/issues/511
  * 腿部：为 DX8 和 DX9 禁用阴影
  * `r__actor_shadow_in_demo_record` cvar 用于在 `demo_record 1` 时禁用角色阴影
  * `r2_sun_lumscale_color` cvar 用于调节太阳颜色
  * 禁用 `alife_object that uses server_objects_registry, less calls to engine unless necessary`，因为我疑心重，无法确认它不会导致错误
  * erepb:
    * 修复 online 切换时小队传送（https://github.com/themrdemonized/xray-monolith/pull/512）
    * 修复 actualize 失败（https://github.com/themrdemonized/xray-monolith/pull/513）

* MT：
  * 如果 `CObjectList` 销毁时 `destroy_queue` 因某种原因不为空，则清理它
  * 将 `PreRenderThread` 分为变换前和变换后。雨和粒子的更新在游戏循环中更早开始
  * 可能修复了 `CDialogHolder` 中与 UI 相关的崩溃
  * 恢复头灯和手电筒的阴影，修复 https://github.com/themrdemonized/xray-monolith/issues/510
  * 腿部：修复 DX9 上头灯位置闪烁
  * `CAI_Stalker::net_Relcase` 在匹配时使 `m_best_item_to_kill` 失效
  * `stat_memory_async` 命令用于在独立线程获取内存统计
  * 将所有 `stat_memory` 调用替换为 `stat_memory_async`，以减少卡顿和加载时间
  * 基于 SSA 的渲染对象剔除逻辑优化
    * `CalcSSA` 对静态对象使用平方半径，较小的对象将被更积极地剔除
    * 基于 SSA 和位置哈希对静态对象和草地进行渐变剔除：
      * 未通过 SSA 测试的较小对象仍会渲染，具体取决于它们比剔除上限小多少。
      * 实际上它把 "渲染半径" 的硬性截断变成了距离越远对象密度越低，使弹出感更不明显
    * `r__ssa_discard` cvar 用于调节 SSA 剔除，默认 SSA 剔除从 3.5 提高到 7
    * `r_ssa_discard_exp` cvar 用于微调静态对象的剔除逻辑。小于 1 会增加较近对象的密度，大于 1 会减少，默认 0.5
    * `r_ssa_discard_fade_k` cvar 用于微调远处静态对象的剔除逻辑，值越大剔除越严格，默认 4
    * HUD 几何体将跳过 SSA 检查
  * 重做 `r_wallmarks_ssa_k` cvar，使用不同的取值范围，默认 0.5

**2026.04.13**

* 主分支与 MT：
  * 若 `m_ammoTypes[m_ammoType]` 无效则打印警告并将 m_ammoType 设为 0
  * 修复因 `m_attached_items` 索引无效导致游戏加载时可能的 "heavy busy hands"
  * CMissile，修复 https://github.com/themrdemonized/xray-monolith/issues/507：
    * 缓存进度条 xml
    * 在按下 kWPN_ZOOM 时预先创建进度条对象，这样就不会在渲染阶段创建，从而可能干扰 Lua GC
  * DLTX:
    * 使用 `xr_vector<Sect>` 而非 `xr_vector<Sect*>` 存储数据，
    * 缓存实际存储预处理后的数据，缓存读取更快且缓存体积略有减小
  * 体验优化：装填动画结束时，若按住武器开火键，武器将自动开始射击
  * GhenTuong: CRayPick: 实现 get_normal（https://github.com/themrdemonized/xray-monolith/pull/508）

* MT：
  * 在进入主菜单和存档时禁用开销大的 `stat_memory` 调用，修复严重卡顿
  * 墙痕创建优化（https://github.com/ixray-team/ixray-1.6-stcop/commit/c731e386173f284502e71c3201a9c1f68b22c1c5）
  * 回滚可能导致崩溃的任务管理器改动
  * 修复使用 https://github.com/DoktorDauerfeuer/Anomaly-hf-Gadgets-GAMMA-For-Hideout-Furniture 时可能的崩溃
  * 尝试修复 `CEntityAlive::get_new_local_point_on_mesh` 中可能的崩溃
  * 尝试修复 agent_manager_properties.cpp (48): CAgentManagerPropertyEvaluatorEnemy::evaluate 空指针解引用

**2026.04.06**

* MT：
  * 墙痕
    * 恢复 `! Failed to render dynamic wallmark` try-catch 块，修复与某些脚本 mod 的崩溃
    * 禁用 `g_wallmark_range_static` 和 `g_wallmark_range_skeleton` 命令，它们未被使用
  * 修复因在独立线程计算骨骼导致引擎完全卡死的替代方案，修复 `HudItem.cpp (551): CHudItem::UpdateCL` 崩溃
  * `CVisualMemoryManager::visible_object` 中 `m_objects` 的空指针检查

**2026.04.05（预发布）**

* 主分支与 MT：
  * 腿部：修复多个光源下挂件阴影的渲染
  * BusyHandsDebug：若 `db.actor` 为 nil 则不介入，此时无关紧要
  * DXML: 更安全的 Lua 回调，修复诸如用鼠标右键投掷手雷时可能的崩溃
  * `duplicate_story_id_crash` 控制台命令用于在 `"Specified story object is already in the Story registry!"` 错误时禁用崩溃
  * GhenTuong: CWeaponStatMgun: 引入字段 "on_range_fov" 以更改机枪手可见距离。导出 lua 游戏对象函数（https://github.com/themrdemonized/xray-monolith/pull/498）

* MT：
  * 回滚 "Wallmarks: Increase MAX_TRIS from 16384 to 32768"
  * 回滚视觉记忆管理器的改动并移除不必要的临界区保护
  * 拆分 `CObjectList::destroy_queue` 的处理：
    * 当 `mt_scheduler 1` 时，delegate 将被推入下一帧的 `seqParallelBeforRender`
    * 子弹管理器的 `net_RelCase`
    * 对限制器进行 `empty()` 检查
    * 可能修复了在 mt 调度器处理对象期间对象被销毁时的随机崩溃
  * 墙痕重构
    * 静态墙痕按扇区分组。仅可见扇区会渲染墙痕
    * 在渲染前更新墙痕生命周期，简化渲染循环
    * 在对象的 `net_destroy` 时移除骨骼墙痕，修复空中漂浮的墙痕或拉伸墙痕瑕疵
    * 移除将墙痕加入渲染队列时的距离检查，修复在 50 米以外被击杀对象上缺失墙痕的问题
    * `r_wallmarks_ssa_k` 控制台命令用于限制墙痕的渲染距离，默认 40。值越大渲染距离越短
  * 修复使用带运动矢量的 SSS 时过度拖影
  * 当对象更改其视觉模型时更新全局 Feel::Vision 数据，可能修复与 `get_new_local_point_on_mesh` 相关的崩溃
  * 可能修复了因在独立线程计算骨骼导致引擎完全卡死的问题

**2026.03.29**

* 主分支与 MT：
  * `CEnemyManager::useful` 优化：
    * 添加 Lua 调用结果的短生命周期缓存，避免每帧进行开销大的 lua 调用，在 GAMMA 中尤为明显
    * 基于 `entity_alive->ID` 抖动缓存时间，使更新分散到各帧之间
    * 与 NPC 交火时性能大幅提升，在 GAMMA 中最高可达 100%
    * `g_enemy_manager_useful_cache_time` 用于控制缓存过期时间。默认 250ms。-1 将禁用缓存

    ![image](http://puu.sh/KKJxE/a4770b7660.jpg)

  * 腿部：
    * 修复挂件（如头灯）阴影的渲染
    * `g_legs_render_attachments_shadow` 用于切换挂件阴影的渲染，默认启用
  * 修复因网络包中下挂弹药数量异常导致的 Out Of Memory 错误
  * `alife():object_count()` 函数用于返回当前 alife 数量
  * Lua 变更：
    * `_g_patches`:
      * 修补 `pairs` 和 `ipairs`，若定义了元表方法则使用它们（Lua 5.2 功能）
      * 更简单的 `empty_table` 和 `iempty_table`
      * `get_object_by_id` 使用 `gameobjects_registry`，比 `db.storage` 更可靠
      * `alife_object` 使用 `server_objects_registry`，除非必要否则减少引擎调用
      * `fis_zero` 和 `fsimilar` 函数
      * `MinHeap` 尽可能返回自身引用
      * 简单的 `OrderedTable` 类
      * 修改 `_G` 元表的 newindex 以防止遮蔽 const 表
    * `callbacks_gameobject`:
      * `server_objects_registry` 表包含所有 alife 服务器对象，无需引擎调用即可快速查找
    * `item_weapon`:
      * 略微更优化的弹药聚合算法
  * erepb: 向 lua 导出 CALifeSimulator::update_scheduled 为 force_update（https://github.com/themrdemonized/xray-monolith/pull/493）
  * Verdatim25: 为 CWeapon 添加新方法，允许强制更改缩放类型（https://github.com/themrdemonized/xray-monolith/pull/495）

MT：
  * 修复使用 `log_timestamps` 时的崩溃，修复 https://github.com/themrdemonized/xray-monolith/issues/485
  * 修复 `feel_vision.h` 中 `feel_vision_get` 方法的 "Out of memory" 错误
  * 禁用多线程 HOM，修复屏幕边缘附近的瑕疵
  * 修复 `CMemoryManager::make_object_visible_somewhen` 中可能的空指针崩溃
  * 修复因 IK 计算坐标无效导致的游戏可能卡死
  * 墙痕：将 MAX_TRIS 从 16384 提高到 32768

**2026.03.22**

主分支与 MT：
  * `play_cycle CA->PlayCycle` 返回值的空指针检查
  * 腿部改进：
    * 在阴影贴图阶段渲染完整身体模型而不隐藏骨骼，而非移动玩家的骨骼，修复了诸如关卡切换对话框重新出现之类的 bug
    * 调整玩家手电和螺栓的偏移，使其不会漂浮在空中
    * 模型挂接到摄像机，修复与物体或潜行者碰撞时身体位移的 bug
    * `g_legs_in_low_crounch` 命令用于在低蹲时禁用腿部渲染，默认启用
  * `lua_busy_hands_debug` 命令用于调试常见的 "Busy Hands" 错误，默认启用：
    * 目前它调试两组最常见的可能错误：
      * 在已销毁的 `CScriptGameObject` 对象上调用方法
      * 调用方法时参数不匹配
    * 当发生会导致 "Busy Hands" 的严重错误时：
      * Lua 回调将进行一次临时存档并弹出 "Lua Critical Error" 窗口
      * 在窗口中你可以选择立即返回主菜单、重新加载临时存档，或忽略并继续
      ![image](http://puu.sh/KKpnk/2353c0f344.jpg)
  * 用基于索引最小堆的时间事件替换时间事件系统
    * 仅查看最近的事件
    * 快速插入和移除
    * 创建时间事件但函数为 nil 时的安全检查
    * 当 `sleep_active` 标志激活时推迟事件，这样标志解除时它们不会一次性全部触发
    * 我的 `optimized_time_events` 脚本已被拉黑不再加载，以使新系统始终生效
  * `_g.script` 中的小优化
    * 简化 `is_empty`
    * 简化 `shuffle_table`
    * 简化 `size_table`
    * 基于蓄水池抽样的 `random_key_table`
    * `random_choice` 不再分配表
  * 修订了 `DynamicNewsManager` 的修复，使其更稳健
  * 修复 script_fixes_mp:727 `attempt to index local tm (nil value)`
  * 修复 `state_mgr_animation.delayed_attach` 不再刷屏时间事件
  * 修复 `ui_debug_main.delayed_attach` 不再刷屏时间事件
  * 修复 `ui_enemy_health.cs_remove` 在时间事件中刷屏的问题，因为时间事件的包装函数没有返回 true
  * 当未提供排序函数时，`spairs` 采用更快的算法  
  * leer-h：更新 poltergeist.cpp (https://github.com/themrdemonized/xray-monolith/pull/475)
  * erepb：
    * 修复移动目标的寻路 (https://github.com/themrdemonized/xray-monolith/pull/474)
    * 使用 m_location_level 对地图标记点排序 (https://github.com/themrdemonized/xray-monolith/pull/476)
    * 修复当系统无声音设备时的无限循环 (https://github.com/themrdemonized/xray-monolith/pull/479)

MT：
  * .peak 灯光的强度略高，使其在视觉上比 SSS 更易察觉

**2026.03.16**

主分支与 MT：
  * LuaJIT 内存分配提高到 512MB
  * 修复在遍历过程中从表中删除元素时 `spairs` 可能发生的崩溃
  * `spairs` 排序时对 nil 的检查更严格
  * 仅替换 `_g` 元表中的 `__index`，以始终返回 `VEC_ZERO`、`VEC_X`、`VEC_Y`、`VEC_Z` 的副本

MT：
  * 回退 "Merge pull request #462 from knallpsi/detail-cache-optimization"，需要进一步测试
  * 回退 `Alife registry uses sparse_map data structure for faster insertion, removal and iteration`，可能导致随机 bug

**2026.03.15h1**

主分支与 MT：
  * 修复启用腿部后 Interaction Dot Marks 的问题
  * 修复阴影阶段腿部阴影的抖动，修复当前物品位置错误

**2026.03.15**

主分支：
  * 从 MT 移植：SSS phase_ssfx_sss_ext 增加更多安全措施

主分支与 MT：
  * 腿部渲染改进
    * 代码清理
    * `bip01_spine` 附加到骨盆，可选 y 偏移 `g_legs_spine_offset_y`，默认为 0.1
    * 隐藏颈部而非头部
    * 启用腿部时玩家阴影位置正确
  * 基于最小堆的 `spairs` 迭代器
    * 支持提前中断而无需排序整个表
    * 更快地获取第一项
  * 在 `pda.calculate_rankings` 中禁用 `alife():object(id)` 无效 id 的刷屏
  * 当 `on_loading_screen_key_prompt` 发生时，执行 Lua GC 并调用 `jit.flush`
  * leer-h：用于禁用角色身体/腿部模型旋转延迟的命令。可在不编辑现有 stalker_animation.omf 或将新 omf 添加到模型动作引用的情况下，为 Actor\NPC 加载新动画的功能 (https://github.com/themrdemonized/xray-monolith/pull/457)
  * knallpsi：UI 优化，crc32 替换 (https://github.com/themrdemonized/xray-monolith/pull/463)
  * GhenTuong：
    * CWeaponStatMgun：添加射击时的相机效果以及手部模型/动画 (https://github.com/themrdemonized/xray-monolith/pull/464)
    * 针对 WeaponStatMgun 和 Projector 修改 ltx_help_ex.script (https://github.com/themrdemonized/xray-monolith/pull/471)

MT：
  * 在 `CAgentManagerPropertyEvaluatorEnemy::_value_type CAgentManagerPropertyEvaluatorEnemy::evaluate()` 中添加 nullptr 检查
  * Alife registry 使用 `sparse_map` 数据结构以加快插入、删除和遍历
  * knallpsi：细节缓存更新优化 (https://github.com/themrdemonized/xray-monolith/pull/462)

**2026.03.10**

MT：
  * 视觉内存管理器中的额外安全检查
  * `CPHSimpleCharacter::UpdateDynamicDamage`，nullptr 检查应能减少与尸体交互时的崩溃
  * `cNameVisual_set`，更新视觉时将等待对象在视觉感知例程中处理完成
  * `CHudItem::renderable_Render`，对 owner 进行 nullptr 检查

**2026.03.07**

主分支与 MT：
  * Spawn Antifreeze：如果对象有服务器端对应物，则在预取后检查服务器对象是否仍在 alife 中
  * 修复因 `game_tutorials` 缓存导致 Interaction Dot Marks 模组的问题
  * 修复当 dest_level_vertex_id 无效时 `CMovementManager::process_game_path` 可能发生的崩溃
  * 在脚本中始终返回 `VEC_ZERO`、`VEC_X`、`VEC_Y`、`VEC_Z` 的副本，以防止对原始对象的不必要修改，可能修复了大量与此类变量相关的原版 bug。感谢 PrivatePirate97 处理该问题
  * 可能修复 `aaaa_script_fixes_mp.script:652: 'for' initial value must be a number`
  * 为所有服务器对象提供 `server_object_on_(un)register` 回调
  * 在创建全新服务器对象时移除 `bind_item` 和 `item_parts` 中的陈旧数据，修复了武器或服装部件出现在无关对象上，或新制作物品的 uses 和 condition 错误的问题
  * 将基于回调的脚本修复从 `callbacks_gameobject` 移动到 `aaaa_script_fixes_mp`
  * Kutez：空间音频重做 —— 能够覆盖 EFX 的混响，实现可控混响 (https://github.com/themrdemonized/xray-monolith/pull/451)
  * leer-h：添加 "show_actor_body" (https://github.com/themrdemonized/xray-monolith/pull/454)
  * tabudz：克隆代码中的潜在漏洞 (https://github.com/themrdemonized/xray-monolith/pull/453)
  * knallpsi：引擎侧第一人称身体实现 (https://github.com/themrdemonized/xray-monolith/pull/437)
    * 在控制台输入 `g_legs 1` 以启用腿部
    * 如果配置中未指定腿部位移，可用 `g_legs_fwd_offset` 调整腿部位置，见下文
    * `g_legs_in_demo_record` 用于在 `demo_record` 激活时渲染腿部
    * 默认情况下，将使用基于当前服装的现有玩家模型，并隐藏头部和手臂骨骼
    * 可为腿部指定自定义模型。在定义了 `actor_visual` 的服装段落中，或默认腿部的 `[actor]` 段落中，可使用 DLTX 或直接调整 LTX：
      1. 添加 `legs_visual` 字段，包含要使用的模型路径（`legs_visual = sm\actor_legs\jacket_loner.ogf`）
      2. 可选地添加 `legs_fwd_offset` 字段以调整腿部位置（`legs_fwd_offset = -0.55`）
      
MT：
  * 修复使用 `GT - Emplacement` 模组时可能发生的崩溃
  * `g_sv_Spawn` 安全功能
  * `CUIWindow` 延迟删除 `AutoDelete` 项
  * `CWeapon::GetFireDispersion` 添加 `pOwner` 检查以确保安全
  * `CObject::net_Destroy()` 将等待对象在视觉感知例程中处理完成

**2026.03.01**

主分支与 MT：
  * DLTX：将 Malformed Line 和 Invalid Section Parent 警告隐藏到 `print_dltx_warnings` 之后
  * 修复出现点燃或熄灭篝火提示，或任何使用 `game.start_tutotial` 函数的提示时的卡顿
  * 并行 GC 仅在关卡完全加载后才工作，以防止加载时的一些 bug
  * 为 `openal` 和 `optick` 添加 `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` 宏，修复某些 PC 配置下的崩溃
  * damoldavskiy：HUD 状态切换回调 (https://github.com/themrdemonized/xray-monolith/pull/448)
  * LVutner：更快的 CBuffer 更新 (https://github.com/themrdemonized/xray-monolith/pull/449)
  * SaloEater：
    * 修复交易管理员补货同步 (https://github.com/themrdemonized/xray-monolith/pull/442)
    * Imgui luadebug 按钮 (https://github.com/themrdemonized/xray-monolith/pull/443)

MT：
  * 来自 IXRay 仓库的修复，修复 Pripyat Outskirts 中可能的栈溢出 (https://github.com/ixray-team/ixray-1.6-stcop/commit/d9f32e486a27f61ec3cc88a7a0cb87863def2284)
  * 修复当 `mt_level_call` 为 0 时 Molotov 模组爆炸粒子缺失的问题
  * 修复当灯光不允许投射阴影时聚光灯锥体缺失的问题
  * `combat_members()` 访问的线程安全
  * 修复因空间组件中的 NaN 坐标导致 Zaton 可能出现的卡死 

**2026.02.22**

主分支与 MT：
  * Spawn Antifreeze 更多安全功能
  * 通过在 `pda.calculate_rankings()` 函数中遍历实际的 se_stalker 和 se_monster 对象而非整个 alife，消除举起 PDA 时的卡顿
  * damoldavskiy：.anms 的缓动与正确缩放，自定义轴心点 (https://github.com/themrdemonized/xray-monolith/pull/436)

MT：
  * 修复 DX8 下对象光照异常的问题
  * 视觉内存管理器的更多安全功能
  * 对 `dsgraph` 项的构建与渲染进行轻量重构
  * `mt_task_manager` 控制台命令用于在主线程和第二线程之间切换任务管理器，默认为 0，即主线程
  * `phase_ssfx_sss_ext` 中更多安全措施，应减少崩溃
  * 并行 GC 与物理和声音处理并行运行，而非在其之后运行，为 GC 提供更多工作时间以提高效率
  * knallpsi：对 LVutner 的 .peak 体积光的支持 (https://github.com/themrdemonized/xray-monolith/pull/430)
    * 如何启用 .peak？
      0. 建议先安装 SSS
      1. 从 www.moddb.com/mods/stalker-anomaly/addons/peak-volumetrics-1-1 下载并安装 shader
      2. 在控制台输入 `pfx_volumetric_mode 1`，或在 Modded Exes 选项 -> Visual -> Graphics 中启用 .peak 体积光
      3. 可选地调整 SSS MCM 选项中的体积光强度

**2026.02.15**

MT：
  * 修复交换纹理的 bug

**2026.02.14**

主分支：
  * 从 MT 分支移植新的 GC 流程
  * 从 MT 分支移植资源创建和删除的临界区锁定

主分支与 MT：
  * Spawn Antifreeze：修复可能的 `stack overflow` 崩溃
  * 移除引擎内所有动态线程亲和性和进程优先级更改，游戏以正常优先级启动
  * 移除启动画面上的 1 秒暂停
  * `_mm_pause` 自旋次数为 `4096`
  * 简化使用独立按键操作下挂榴弹发射器的代码，应减少 bug
  * 通过优化获取 `R_constant` 指针小幅提升性能
  * erepb：
    * 各种窗口相关修复，一致的窗口创建，居中启动画面，多显示器支持，正确的光标限制 (https://github.com/themrdemonized/xray-monolith/pull/418)
    * 修复引用计数中的竞态 (https://github.com/themrdemonized/xray-monolith/pull/426) (https://github.com/themrdemonized/xray-monolith/pull/429)
  * Tosox：基于安装的实例互斥锁，允许多个游戏实例 (https://github.com/themrdemonized/xray-monolith/pull/424)

MT：
  * 修复体积光渲染
  * 修复使用 SSS 时全向光阴影闪烁的问题
  * 修复实体销毁时与视觉管理器相关的可能崩溃
  * 雨水和粒子管理器使用更宽松的原子操作
  * knallpsi：控制台命令历史，用鼠标滚轮滚动控制台，修复用箭头浏览命令历史 (https://github.com/themrdemonized/xray-monolith/pull/423)

**2026.02.08**

主分支与 MT：
  * 抢先的交易员更新检查在独立计时器上运行，从这方面修复了 Gamma 中的卡顿 
  * DLTX 重构：
    * 重复段落检查现在真正生效
    * DLTX 缓存：已解析的文件和段落永久存储在 RAM 中。下次通过 `CInifile` 构造访问文件时将快速返回数据，无需访问磁盘和进行 DLTX 解析
    * `dltx_use_cache` 变量用于切换缓存
    * 原版中缓存使用约 50MB，在 GAMMA 上最高达 150MB
  * 更好的 Debug Inputs 脚本（按 F7 时）。影响物品、对象生成器和执行器
    * 当你从列表或通过在输入框输入来生成对象时，输入将被保存
    * 你可以按上下箭头在已保存的输入之间循环，就像浏览控制台命令一样。输入框不应处于聚焦状态（看不到光标）
    * 最多可保存 20 条输入
    * 执行器中函数的执行会自动包装为受保护调用，因此不会因某些错误使游戏崩溃。如果发生错误，你会在底部或控制台中看到消息
  * Spawn Antifreeze：修复生成时偶发的崩溃，重新组织数据以尽量减少多线程锁定时间
  * erepb：极早期 luajit 初始化 (https://github.com/themrdemonized/xray-monolith/pull/419)
  * erepb：真正修复声音设备选择和自动切换 (https://github.com/themrdemonized/xray-monolith/pull/416)
  * PrivatePirate97：解耦水平后坐力，非线性惯性移动，lua_help_ex.script 的补充 (https://github.com/themrdemonized/xray-monolith/pull/417)
  * GhenTuong：针对 level_graph、CExplosive、CGrenade、CWeaponStatMgun、CWeapon、脚本变异体移动的开发工作。导出 CGrenade 函数 (https://github.com/themrdemonized/xray-monolith/pull/420)
  * Tosox：键盘布局感知的文本输入与 Caps Lock 处理 (https://github.com/themrdemonized/xray-monolith/pull/421)

MT：
  * 修复使用 Glowsticks 模组时的崩溃
  * 修复使用 SWM Visible Legs 模组时偶发的崩溃
  * 修复因渲染器在 shader 数据之前被销毁而导致退出游戏时的偶发崩溃
  * 3DSS，修复通过瞄准镜观察时草地渲染在潜行者上方的问题
  * 减少 `shared_str` 和 `str_container` 的内存占用
  * 防止在 `ModelPool` 中调用 `Instance_Register` 时产生重复 
  * 当 `feel_vision_update` 正在运行时更安全地销毁对象
  * 在视觉感知中添加忽略自身模型 (https://github.com/ixray-team/ixray-1.6-stcop/commit/c0a2540937708d029e4f7247e6cbcb929204eb5b)
  * 默认禁用射击时对象上的血迹贴花，可通过 `r__blood_decals_on_objects` 启用
  * `mt_calc_bones` 选项用于禁用多线程骨骼计算
  * 多线程骨骼计算略微更安全
  * 调度器：
    * 当 MT 调度器启用时，在主线程上拆分实时对象，非实时对象放入任务组。修复 Interaction Dot Marks 模组
    * `scheduler_batch_size` 控制每帧 `shedule_update` 的最大对象数
    * `scheduler_log` 用于记录日志信息
  * 空闲时间并行 Lua GC
    * 当渲染器准备帧时，以小步长反复调用 Lua GC 使其持续工作并减少后续清理工作，减少卡顿
    * 帧渲染完成后，检查 Lua 内存使用情况是否良好，若不佳则用常规 GC 步长执行一次大 GC
    * `lua_parallel_gc` 用于启用该功能
    * `lua_parallel_gcstep` 控制 GC 步长，默认为 75
    * `lua_parallel_gc_call_amount` 设置 GC 被调用的次数，默认为 25
    * `lua_parallel_gc_debug` 用于打印内存信息

**2026.01.31**

主分支与 MT：
  * DLTX 重构
    * 恢复 KV 对使用旧的 `xr_vector` 结构，提高加载稳定性
    * 显著优化了与模组合并段落的加载，缩短了加载时间，尤其在 DLTX 密集的整合包中
    * 更具信息量的日志记录，当文件中遇到格式错误的行时打印警告，当段落继承自不存在的父级或使用 `!` 覆盖定义的父级时也打印警告
  * PrivatePirate97：修复使用倾斜瞄准时的惯性偏移移动 (https://github.com/themrdemonized/xray-monolith/pull/410)

MT：
  * 修复因代码中运动矢量计算位置不正确导致的持续动态模糊
  * 修复尝试使用 `demo_record` 并穿过几何体飞行，或出现大量渲染扇区和传送门的任何情况下可能发生的崩溃
  * 通过 `mt_level_call` 命令并行执行 `CreateTimeEvent` 和 `AddUniqueCall` 命令（默认禁用）。禁用后可部分修复 Interaction Dot Marks 模组
  * `stat_memory` 打印更多关于 `shared_str` 容器的信息 —— 当前使用的池块、负载因子、最大哈希冲突
  * `stat_memory` 打印 Lua 内存使用情况

**2026.01.25（预发布）**
* MT 分支测试版首次发布 
* 在 Modded Exes 设置中扩展了草地阴影设置
* 重构 DLTX 代码，使其对引擎模组作者更高效、更易读。
* 通过将键值存储从 `xr_vector` 改为 `xr_set`，显著减少了 DLTX 密集整合包的加载时间
* 移除 `FPU` 模块中的过时代码调用
* VodoXleb：修复 level.set_music_volume() (https://github.com/themrdemonized/xray-monolith/pull/403)
* erepb：修复声音设备自动切换 (https://github.com/themrdemonized/xray-monolith/pull/405)

**2025.12.30**
* 为 `VerifiedDX11` 配置启用热重载配置
* 移除 `Actor.cpp` 中对 `calculateBones` 的重复调用
* tlnguyen-smu：修复 LuaJIT 中克隆代码的潜在漏洞 (https://github.com/themrdemonized/xray-monolith/pull/399)

**2025.12.23**
* 移除对过时 `Loki` 库的依赖，减小 exe 和 pdb 文件大小 (https://github.com/ixray-team/ixray-1.6-stcop/commit/4dc52310a94829c254ea03019399afac12f77450, https://github.com/ixray-team/ixray-1.6-stcop/commit/8e916f28fbc293628b61d8b3270e33b91294f31a)
* 乌鸦会飞向最近被击杀的 NPC (https://github.com/ixray-team/ixray-1.6-stcop/commit/0392ea1ae4e7454dd8a7d38e1868e03558c42093)
* 修复从声源到玩家的音量计算 (https://github.com/ixray-team/ixray-1.6-stcop/commit/dddeb09cb90190d151b23bcba15a389175ed686f)
* 如果提供了无效的 fire_bone、fire_bone2 或 shell_bone，则在控制台中打印警告
* 修复 player_hud 函数的编译警告
* 修复 xrSound 中 `g_target_temp_data_16` 的分配大小
* GhenTuong：CProjector、CWeaponStatMgun、bind_monster.script 的开发改动 (https://github.com/themrdemonized/xray-monolith/pull/396)

**2025.12.02**
* Antglobes：导出截图函数 + 可变分辨率与编码 (https://github.com/themrdemonized/xray-monolith/pull/394)

**2025.11.17**
* 修复 `g_recon_maxdist`、minspeed 和 maxspeed 命令以设置正确的变量
* GhenTuong：针对 CCar、CExplosive、CWeaponStatMgun、SPATIAL_CHANGE 的小调整与 bug 修复。

**2025.10.28**
* 临时回退 https://github.com/themrdemonized/xray-monolith/pull/326 以修复 https://github.com/themrdemonized/xray-monolith/issues/387#issuecomment-3453531244

**2025.10.23**
* 将 `bolt` 和 `bolt_bullet` 添加到生成防冻结忽略列表
* SaloEater：带深度的 World2UI lua 方法 (https://github.com/themrdemonized/xray-monolith/pull/386)

**2025.09.28**
* 将 ubgl 键移动到 custom21，修复 https://github.com/themrdemonized/xray-monolith/issues/380，你需要在设置中重新绑定 UBGL 键
* RavenAscendant：额外 100 个条目，允许 bind 控制台命令将动作绑定到不会由键盘输入触发但可由 `level.press_action` 函数触发的值

**2025.09.26**
* 在 `set_scope_ui` Lua 导出中禁用 `ZoomTexture` 检查
* Xottab-DUTY：修复快速（阴影）几何体的加载 (https://github.com/themrdemonized/xray-monolith/pull/379)

**2025.09.19**
* 可能修复 https://github.com/themrdemonized/xray-monolith/issues/375

**2025.09.15**
* 各种 Luabind 修复与改进
* 简化 `script_callback_ex` 模板
* 修复 `PHItemList` 编译警告
* SaloEater：更新指向新 wiki 的链接 (https://github.com/themrdemonized/xray-monolith/pull/372)

**2025.09.12**
* OXR：将日志和 fs 的 shared_str 替换为 xr_string，修复了 https://github.com/themrdemonized/xray-monolith/issues/366
* 修复 https://github.com/themrdemonized/xray-monolith/issues/369

**2025.09.10**
* ProfLander：修复 CALifeMonsterBrain::process_task 段错误 (https://github.com/themrdemonized/xray-monolith/pull/364)

**2025.09.06**
* 将 luabind 替换为非 Boost 版本 (https://github.com/ixray-team/ixray-1.6-stcop/commit/2f61f5f781130468c945720b76d23ce4bbea95b1)
* 在 luabind 中禁用 `std::terminate` (https://github.com/ixray-team/ixray-1.6-stcop/commit/723fb65a8b9ebb89dda8f03cbb4b1bebceabacdc)
* 移除 Boost 库
* 在 `CMonsterCorpseMemory::add_corpse` 中添加 nullptr 检查
* LVutner：
  * ADD：[Render] 添加缺失的 s_position 采样器 (https://github.com/themrdemonized/xray-monolith/pull/358)
  * UPD：[Render] r_ComputePass 修复 (https://github.com/themrdemonized/xray-monolith/pull/359)

**2025.08.30**
* MFB：智能掩体现在具有固定的进入最小/最大距离 (https://github.com/themrdemonized/xray-monolith/pull/352)
* Lucy：关卡脚本挂载 (https://github.com/themrdemonized/xray-monolith/pull/353)
* ProfLander：ImGui：实现分组 API (https://github.com/themrdemonized/xray-monolith/pull/354)

**2025.08.27**
* 修复 https://github.com/themrdemonized/xray-monolith/issues/346
* NLTP_Ashes：
  * 向 Lua 导出多个 CWeaponKnife 相关函数 (https://github.com/themrdemonized/xray-monolith/pull/347)
  * 向 Lua 导出 HUD 元素 (https://github.com/themrdemonized/xray-monolith/pull/351)
* Ncenka：修复主菜单随机音乐 (https://github.com/themrdemonized/xray-monolith/pull/345)

**2025.08.23**
* Kutez：回调优先级系统 (https://github.com/themrdemonized/xray-monolith/pull/339)
* Ncenka：PDA UI XML 设置器 (https://github.com/themrdemonized/xray-monolith/pull/343)

**2025.08.21**
* GhenTuong：
  * CCar CWeaponStatMgun 改动 (https://github.com/themrdemonized/xray-monolith/pull/310)
  * 导出 API 函数及小幅改进 (https://github.com/themrdemonized/xray-monolith/pull/340)
* Antglobes：太阳数值 (https://github.com/themrdemonized/xray-monolith/pull/341)

**2025.08.19**
* 移除 HOM 中的 `parallel_for` 和 `particle_actions_collection`，改用单线程循环以减少线程创建开销
* 将 `unordered_map` 实现替换为 `unordered_node_map`，set 同理
* 移除体积光代码中的双重循环
* 禁用驾驶汽车时角色耐力的更新 (https://github.com/ixray-team/ixray-1.6-stcop/commit/6c1ad01adffba180df8f47a58f33e66e69def949)
* 修复 NPC 尝试使用已销毁对象时的崩溃 (https://github.com/ixray-team/ixray-1.6-stcop/commit/d34966c3e255568f60df7bd0e33d61bebfe98afa)
* ProfLander：向 stat_memory 和 OOM 处理器添加字符串计数 (https://github.com/themrdemonized/xray-monolith/pull/337)
* Kutez：为 HUD 声音回调添加新的 "volume_mult" 属性。移除所有室内框架相关的引擎侧代码 (https://github.com/themrdemonized/xray-monolith/pull/338)

**2025.08.12u1**
* 修复 https://github.com/themrdemonized/xray-monolith/issues/333
* v2v3v4：修复用探测器瞄准镜放大即将被销毁的对象时的 ctd

**2025.08.12**
* 修复 https://github.com/themrdemonized/xray-monolith/issues/332

**2025.08.11**
* 第三方文件的小幅整理
* `g_interrupt_fire_on_aim_toggle` cvar 用于设置按下瞄准时停止开火，默认启用，解决 https://github.com/themrdemonized/xray-monolith/issues/327
* v2v3v4：一次性更新所有声音位置
* ProfLander：发射器：弹壳弹药 + 弹道与装填选项 (https://github.com/themrdemonized/xray-monolith/pull/322)
* VodoXleb：为新望远镜变焦添加 `binoculars_dynamic_zoom_check` cvar，默认禁用 (https://github.com/themrdemonized/xray-monolith/pull/328)，解决 https://github.com/themrdemonized/xray-monolith/issues/325
* Lucy：修复用于获取/设置模型 shader 和纹理的 lua 函数 (https://github.com/themrdemonized/xray-monolith/pull/329)
* LVutner：
  * 移除无用的 DSV... (https://github.com/themrdemonized/xray-monolith/pull/330)
  * 可能修复损坏的 CBuffers [r_ComputePass] (https://github.com/themrdemonized/xray-monolith/pull/331)

**2025.08.09**
* 用 fast_dynamic_cast 库替换 `smart_cast` (https://github.com/ixray-team/ixray-1.6-stcop/commit/2197a168bbd700f64df0fbcb5f0139a289a39102)
* 将 LuaJIT NMake 转换为 VS2022 项目 (https://github.com/themrdemonized/xray-monolith/pull/323)
* 减小 .pdb 大小

**2025.08.07**
* VodoXleb：为望远镜添加 scope_dynamic_zoom = off
* Ncenka：动态设备开启/关闭 (https://github.com/themrdemonized/xray-monolith/pull/318)

**2025.08.04**
* 调试渲染器支持将字符串作为图元的 id
* 将新的弹药聚合脚本移入 item_weapon，直接替换旧实现

**2025.08.01**
* 修复 Modded Exes 菜单中缺失的 `ik_calc_ssa` 设置
* 修复与某些误用弹药聚合函数的模组一起使用时的崩溃

**2025.07.31**
* 使用 x64 工具链进行编译
* Orleonn：Lua 导出：CUIDialogWnd::AllowWorkInPause 和 render_device:pause_ex (https://github.com/themrdemonized/xray-monolith/pull/308)
* NLTP_ASHES：在 CGameObject::net_Spawn 中添加故障保护，尝试更新缺失的模型 (https://github.com/themrdemonized/xray-monolith/pull/311)

**2025.07.29**
* 骨骼计算优化
  * 使用屏幕空间面积（SSA）代替距离检查，对假巨人等较大对象效果更好
  * `ik_calc_ssa` cvar 用于控制优化强度，默认为 0.006
* 将交易员更新半径从 100 减少到 30
* `bullet_on...` 回调收到 `bullet.element` 字段。当子弹命中 alife 对象时，element 将是骨骼 id。如果是静态几何体，那么它将是几何三角形编号。如果未命中则为 -1。
* 新的弹药聚合函数，会等待游戏对象上线，且采用不同算法。可能修复 https://github.com/themrdemonized/xray-monolith/issues/118

**2025.07.27**
* Lucy：模型 Visbox 更新与脚本挂载修复 (https://github.com/themrdemonized/xray-monolith/pull/306)

**2025.07.26**
* 附近交易员的库存将提前更新
* 声音：
  * 将 OpenAL 版本更新到 1.23.1
  * 集成 OpenAL EFX 扩展，`snd_efx` 现在可用 (https://github.com/ixray-team/ixray-1.6-stcop/commit/e429c13023261623b5260c8e85b588d6d8535e44)
  * 在适当的地方添加了一些声音环境，来自 (https://www.moddb.com/mods/doctorx-call-of-the-zone/addons/dead-air-spatial-sound-and-reverb)。
  * 并未从该模组添加所有地图。如果你想要全部，请单独下载。拥有声音环境的地图：
    * 地下地图
    * 沼泽
    * 黑暗山谷
    * 罗斯托克
    * 焦脑器
    * 红森林
    * 医院
    * 普里皮亚季郊区
  * `snd_efx_environment_change_time` 用于更改地图上声音环境之间的插值时间，默认为 1.66 秒

**2025.07.23**
* 各种 bug 修复与崩溃修复
* ZoulKrystal：防毒面具性能调整与 ltx 瞄准 fov (https://github.com/themrdemonized/xray-monolith/pull/304)

**2025.07.20**
* 修复使用 `mt_update_weapon_sounds` 时骨骼闪烁的问题
* VodoXleb：GAME 路径构建失败的回调 (https://github.com/themrdemonized/xray-monolith/pull/301)

**2025.07.19**
* 优化
  * 武器声音位置更新移至单独帧，可通过 `mt_update_weapon_sounds` cvar 切换，默认开启。根据武器游戏对象数量可获得小幅性能提升
  * 骨骼计算优化仅在关卡完全加载后启用
* 玩法
  * 可选渐进式耐力消耗，耐力使用量与当前重量线性相关而非硬性截断点，cvar `g_progressive_stamina_cost`，默认关闭
  * 修复乌鸦 AI 飞行目标位置 (https://github.com/ixray-team/ixray-1.6-stcop/commit/4e7de9844c1906749bb519f9c6ce350f42f02dea)
  * NPC 在近距离时会转头看向主角 (https://github.com/ixray-team/ixray-1.6-stcop/commit/e55a85f0d5b719e3cd9ce23ca7976b0ca2124b08)
    * cvar `g_npcs_look_at_actor` 用于启用该功能，默认开启
    * cvar `g_npcs_look_at_actor_min_distance` 用于控制他们开始注视的最小距离，默认为 3.5
    * 回调 `npc_on_before_look_at_actor` 用于控制 NPC 的行为
* 修复 https://github.com/themrdemonized/xray-monolith/issues/296
* VodoXleb：修复 `_G.get_object_squad` 错误 (https://github.com/themrdemonized/xray-monolith/pull/299)

**2025.07.16**
* 修复因在骨骼计算中未清除指向已删除 IRenderable 的指针而导致的崩溃

**2025.07.15**
* 减少骨骼计算更新而非禁用它们，修复尸体 T-pose 的问题

**2025.07.14**
* DLTX：允许 DLTX 的 `>` 在属性不存在时创建它 (https://github.com/themrdemonized/xray-monolith/issues/289)
* 优化：
  * 视野视锥体之外的骨骼模型将不进行骨骼计算，减少 CPU 负载
  * 此外 `r__optimize_calculate_bones` cvar 允许禁用远处对象的计算（默认启用）
  * `ik_calc_dist` 表示一个距离，超过该距离则停止计算（默认 100）
  * `ik_always_calc_dist` 表示一个距离，低于该距离时模型即使不在视锥体中也会进行计算（默认 20）
  * 在实体众多、人口密集的地图上，如果你是 CPU 受限，预计帧时间减少约 2ms
* 声音：
  * 根据正常的 343m/s 音速添加了基于距离的延迟。可调整的控制台变量：
    * `snd_distance_based_delay_power` 控制延迟强度。0 将禁用延迟。默认 1
    * `snd_distance_based_delay_min_distance` 控制开始察觉延迟的最小距离（米）。默认 50
  * 为声音添加了可选的音高变化。每次播放声音时音高都会略有不同。`snd_pitch_variation_power` 控制变化强度。默认 0

**2025.07.12**
* lulnope：向 lua 脚本暴露 `memory_remove_links`

**2025.07.08**
* Spawn antifreeze：修复与螺栓相关的问题，由上一版本引入：https://github.com/themrdemonized/xray-monolith/issues/287
* SaloEater：使用 luapanda 调试脚本 (https://github.com/themrdemonized/xray-monolith/pull/251)

**2025.07.06**
* Spawn antifreeze：
  * 添加 `mod_system_spawn_antifreeze_ignore.ltx` 文件，该文件中的行是不会被 antifreeze 处理的段落。部分解决 https://github.com/themrdemonized/xray-monolith/issues/283
  * 修复当父对象已被销毁且不存在于 ALife 中时尝试生成子物品这一可能情况下的 CTD，修复 https://github.com/themrdemonized/xray-monolith/issues/284
* 将所有 `luabind` 替换为 `::luabind`
* 将项目迁移到 C++17 标准

**2025.07.05**
* Ascii1457：SSS 23.2 更新

**2025.07.03**
* Ascii1457：SSS 23.1 更新

**2025.07.02**
* Spawn Antifreeze：不预取直升机，可能修复 https://github.com/themrdemonized/xray-monolith/issues/278

**2025.07.01**
* `hanging_lamp_ignore_match_configuration` cvar 用于规避 SSS23 + HF 崩溃，与 https://github.com/themrdemonized/xray-monolith/issues/273 相关
* 修复 https://github.com/themrdemonized/xray-monolith/issues/276

**2025.06.30**
* 默认禁用打印 DLTX `!` 警告，用 `print_dltx_warnings 1` cvar 切换
* Lua GC 步长增加到 300
* OneMorePseudoCoder：
  * 不对汽车和直升机应用渲染优化 (https://github.com/themrdemonized/xray-monolith/pull/271)
  * 修复 NPC 脚步声 (https://github.com/themrdemonized/xray-monolith/pull/274)

**2025.06.28**
* 修复 updateDiscordPresence 函数中因竞态条件导致的崩溃
* Spawn antifreeze：不预取 G_RPG7 和 G_FAKE 对象，修复 "incorrect destroy sequence for object" 错误
* Ascii1457：SSS 23 更新
* LVutner：R11G11B10_FLOAT 支持

**2025.06.27**
* 禁用初始脚本预取，与 SSS 冲突

**2025.06.26**
* Spawn antifreeze：修复因 shader 创建中的竞态条件导致的若干随机崩溃
* LVutner：[Render] PIXEVENTs。感谢 forserx 和 frowik

**2025.06.24**
* 热修复：
  * Spawn antifreeze：修复当未找到用于预取的模型时崩溃
  * 动态新闻管理器：检查战利品表中的内容类型是否为实际对象，修复与现有 moddb 修复一起使用时导致的崩溃
* 修复构建时 "overriding /ob2 with /ob3" 警告
* 修复 https://github.com/themrdemonized/xray-monolith/issues/267

**2025.06.23**
* 优化梳理：
  * Spawn Antifreeze：在生成实体前将模型和纹理资源的卸载放入单独的线程。默认启用。如果你发现问题，请报告并将其在控制台用 `spawn_antifreeze 0` 关闭 (https://github.com/themrdemonized/xray-monolith/pull/257)
  * 将 Discord 更新移至单独线程
  * 修复 `trans_outfit.transparent_gg()` 函数存在整个 alife 循环的问题，现在仅使用游戏对象
  * 修复动态新闻管理器战利品表包含可能已被销毁的对象，导致物品占用的问题
  * 将 Lua 垃圾回收步长从 400 改为 160
* 将项目文件移出 vs2022 文件夹，重构解决方案文件。应修复损坏的预编译头
* ProfLander：集成 optick 分析器 (https://github.com/themrdemonized/xray-monolith/pull/262)

**2025.06.20**
* 更新 Github Action 以使用 `softprops/action-gh-release`
* [功能请求] 添加了切换武器时反转鼠标滚轮的选项 (https://github.com/themrdemonized/xray-monolith/issues/261)
* ProfLander：3D 弹道修复 (https://github.com/themrdemonized/xray-monolith/pull/259)
* Kutez：更新 v3 室内枪声框架 (https://github.com/themrdemonized/xray-monolith/pull/260)

**2025.06.18**
* Lucy：脚本挂载 3D UI 缩放/原点 (https://github.com/themrdemonized/xray-monolith/pull/258)

**2025.06.15**
* Lucy：更多改动 (https://github.com/themrdemonized/xray-monolith/pull/254)

**2025.06.11**
* Lucy：带脚本支持的 ImGui 实现 (https://github.com/themrdemonized/xray-monolith/pull/250)

**2025.06.09**
* NLTP_ASHES：向 Lua 导出了 `_keyboard` 的 `key_name` 和 `key_local_name` (https://github.com/themrdemonized/xray-monolith/pull/248)

**2025.06.08**
* NLTP_ASHES：添加 `actor_on_death` 回调 (https://github.com/themrdemonized/xray-monolith/pull/244)

**2025.06.05**
* 如果覆盖声音在 `_G.COnBeforePlayHudSound` 回调中初始化失败，引擎将崩溃
* ProfLander：修复默认准星为黑色的问题 (https://github.com/themrdemonized/xray-monolith/pull/242)
* NLTP_ASHES：在 CScriptParticles 和 CParticlesObject 上实现 SetHudMode (https://github.com/themrdemonized/xray-monolith/pull/243/files)

**2025.06.03**
* NLTP_ASHES：导出颜色动画函数并实现 RemoveColorAnimation (https://github.com/themrdemonized/xray-monolith/pull/241)

**2025.05.29**
* lulnope：修复 PDA 任务/地图标签页中高弹窗的弹窗定位 (https://github.com/themrdemonized/xray-monolith/pull/237)

**2025.05.26**
* 修复并清理 Modded Exes 选项
* Lucy：脚本挂载改进 (https://github.com/themrdemonized/xray-monolith/pull/234)

**2025.05.24**
* NLTP_ASHES：向 Lua 导出 CurrentGameUI()->m_pMessagesWnd (https://github.com/themrdemonized/xray-monolith/pull/233)

**2025.05.23**
* damoldavskiy：程序化移动动画 (https://github.com/themrdemonized/xray-monolith/pull/230)

**2025.05.19**
* damoldavskiy：MAS：最小变焦处理 (https://github.com/themrdemonized/xray-monolith/pull/228)

**2025.05.16**
* 修复脚本挂载的 `get_ui_position` 函数
* ProfLander：
  * 多个准星 (https://github.com/themrdemonized/xray-monolith/pull/224)
  * 实现 `g_aimpos_zoom` (https://github.com/themrdemonized/xray-monolith/pull/226)
  * 对 `g_get_target_position origin` 与方向使用正确的 trace (https://github.com/themrdemonized/xray-monolith/pull/227)

**2025.05.12**
* NLTP_ASHES：将 CUIProgressBar 的 m_UIProgressItem 向 LUA 导出 (https://github.com/themrdemonized/xray-monolith/pull/225)

**2025.05.10**
* 在 `on_news_received` 回调中添加 ScrollView 实例
* Lucy:
  * 脚本附件现在使用字符串而非 ID (https://github.com/themrdemonized/xray-monolith/pull/221)
  * [ReShade] 修复全屏模式下的崩溃 (https://github.com/themrdemonized/xray-monolith/pull/223)
* Lander：通过选项菜单导出 wallmark 范围 (https://github.com/themrdemonized/xray-monolith/pull/222)

**2025.05.07**
* 修复 MAS 的升级 bug (https://github.com/themrdemonized/xray-monolith/pull/219)

**2025.05.06**
* `get_console():get_variable_bounds(cvar)` 函数，用于获取 cvar 的取值范围
* damoldavskiy:
  * 修复 MAS 瞄具生成 (https://github.com/themrdemonized/xray-monolith/pull/217)
* ProfLander:
  * 修复可脚本化的贴墙抖动 (https://github.com/themrdemonized/xray-monolith/pull/213)
  * 实现 g_aimpos 以控制子弹方向 (https://github.com/themrdemonized/xray-monolith/pull/214)
  * 改进设置菜单的分类与定义机制 (https://github.com/themrdemonized/xray-monolith/pull/215)

**2025.05.03**
* `ray_pick:query()` 将清除先前的查询结果
* GhenTuong：合并 force-body-state、axr_beh 改进、vignette_control (https://github.com/themrdemonized/xray-monolith/pull/209)
* ProfLander:
  * 改进准星插值 (https://github.com/themrdemonized/xray-monolith/pull/206)
  * 再次修复准星尺寸 (https://github.com/themrdemonized/xray-monolith/pull/210)

**2025.05.01**
* `g_draw_pickup_item_names` cvar，用于在按住按键时禁用物品名称显示 (https://github.com/themrdemonized/xray-monolith/issues/191)
* ProfLander: 
  * 修复 CWeapon::GetNearWallOffset 中的日志刷屏 (https://github.com/themrdemonized/xray-monolith/pull/192)
  * 修复 firepos 设置的选项默认值 (https://github.com/themrdemonized/xray-monolith/pull/193)
  * 修复准星读数抖动 (https://github.com/themrdemonized/xray-monolith/pull/194)
  * 准星：远距离尺寸选项 (https://github.com/themrdemonized/xray-monolith/pull/197)
  * 改进 Lua trace API (https://github.com/themrdemonized/xray-monolith/pull/198)
  * 修复动画 HUD FOV (https://github.com/themrdemonized/xray-monolith/pull/199)
  * 可脚本化的近墙 (Nearwall) (https://github.com/themrdemonized/xray-monolith/pull/201)
  * 添加「始终显示准星」选项 (https://github.com/themrdemonized/xray-monolith/pull/203)
* VodoXleb:  用于强制设置 NPC body_state 的回调 (https://github.com/themrdemonized/xray-monolith/pull/202)

**2025.04.28**
* 可通过多次 `set_ignore_object` 调用为 `ray_pick` 设置多个要忽略的对象：
  ```lua
    ray:set_ignore_object(obj_1)
    ray:set_ignore_object(obj_2)
    ...
  ```
* ProfLander：实现 g_firepos (https://github.com/themrdemonized/xray-monolith/pull/190)

**2025.04.27**
* 移除了 Lua 相关 CTD 上的重复堆栈跟踪
* `obj:bounding_box(bool bHud)` 方法，用于获取对象的包围盒
* ProfLander:
  * 基于 shader 的准星 (https://github.com/themrdemonized/xray-monolith/pull/183)
  * 基于位置的贴墙偏移 (https://github.com/themrdemonized/xray-monolith/pull/186)

**2025.04.25**
* `game.ui2world_offscreen(pos)` 函数，用于从屏幕外的 UI 坐标反投影
* 新增 vector 导出：
  * 静态函数：
    * `vector.generate_orthonormal_basis(Fvector dir, Fvector up, Fvector right)`
    * `vector.generate_orthonormal_basis_normalized(Fvector dir, Fvector up, Fvector right)`
  * `function project(Fvector u, Fvector v)`
  * `function project(Fvector v)`
* 新增 matrix 导出：
  * `function transform(Fvector, Fvector)`
  * `function transform(Fvector)`
  * `function transform_tiny(Fvector, Fvector)`
  * `function transform_tiny(Fvector)`
  * `function transform_dir(Fvector, Fvector)`
  * `function transform_dir(Fvector)`
  * `function hud_to_world()`
  * `function world_to_hud()`
* Kutez：更新 v0.3 Indoor Gunsound Framework (https://github.com/themrdemonized/xray-monolith/pull/176)
* ProfLander:
  * HUD <-> 世界变换 (https://github.com/themrdemonized/xray-monolith/pull/179)
  * 修复第三人称下的武器粒子投影 (https://github.com/themrdemonized/xray-monolith/pull/180)
  * 减少 bone_position / bone_direction 中的代码重复 (https://github.com/themrdemonized/xray-monolith/pull/182)

**2025.04.22**
* `on_before_play_hud_sound` 回调，可用另一个音效覆盖 hud 音效
* `obj:bone_transform` 函数，用于获取骨骼的变换矩阵
* `obj:xform` 函数，用于获取对象的变换矩阵
* 新增 vector 函数
  * `hud_to_world()` 用于将向量从 hud 空间转换到世界空间
  * `world_to_hud()` 反之亦然
  * `hud_to_world_dir()` 用于将方向向量从 hud 空间转换到世界空间
  * `world_to_hud_dir()` 反之亦然

**2025.04.21**
* NLTP_Ashes：为 HUD 几何体添加扭曲（distortion）支持 (https://github.com/themrdemonized/xray-monolith/pull/174)
* Lucy：更新 ReShade 支持 (https://github.com/themrdemonized/xray-monolith/pull/175)。**如果你从早于 2025.04.21 的版本更新并且使用 Reshade，你也需要更新 Reshade**

**2025.04.19**
* Lucy: (https://github.com/themrdemonized/xray-monolith/pull/171)
  * 移除 Model Pool 导致大量不可预测的崩溃，所以现在它回来了，但做了一个小改动，以便与 set_shader 的 lua 方法协同工作 :)
  * 添加 `get_default_shaders` 和 `reset_shader`，可轻松将模型还原为原版 shader，无需把默认值存到表中
  * 添加对获取/设置/重置脚本附件模型 shader/贴图的支持

**2025.04.18**
* VodoXleb：角色相机 y 轴偏移 (https://github.com/themrdemonized/xray-monolith/pull/170)

**2025.04.15**
* 修复 (https://github.com/themrdemonized/xray-monolith/issues/168)
* Lucy：修复 (https://github.com/themrdemonized/xray-monolith/pull/167)

**2025.04.14**
* Lucy：修复 (https://github.com/themrdemonized/xray-monolith/pull/165)

**2025.04.13**
* Kutez：Indoor Gunsound Framework (https://github.com/themrdemonized/xray-monolith/pull/161)

**2025.04.11**
* Lucy：通过脚本更改 game_object 的 shader/贴图 (https://github.com/themrdemonized/xray-monolith/pull/157)

**2025.04.05**
* 移除了附件的 `FreeRoom_inBelt` 检查

**2025.04.04**
* 脚本修复
  * 修复了 `utils_item.has_scope` 在有些情况下返回错误附加物品的问题

**2025.04.03**
* 脚本修复
  * 修复了 `axr_trade_manager.npc_trade_buy_sell` 和 `axr_trade_manager.npc_tech_upgrade_sell` 函数
  * 修复了 `item_weapon.detach_scope` 在有些情况下给出错误附加物品的问题

**2025.03.29**
* Bence7661：修复问题 (https://github.com/themrdemonized/xray-monolith/issues/153)

**2025.03.22**
* 修复问题 (https://github.com/themrdemonized/xray-monolith/issues/149)

**2025.03.21**
* `--dxgi-old` 命令行参数，用于修复 Linux 问题 (https://github.com/themrdemonized/xray-monolith/issues/95)

**2025.03.18**
* Lucy：移除 Flashlight 类中硬编码的 hud_mode (https://github.com/themrdemonized/xray-monolith/pull/148)，修复 (https://github.com/themrdemonized/xray-monolith/issues/147)

**2025.03.17**
* 修复 (https://github.com/themrdemonized/xray-monolith/issues/146)

**2025.03.16**
* Lucy：修复头灯位置 (https://github.com/themrdemonized/xray-monolith/pull/145)

**2025.03.15**
* `obj:list_bones(bHud = false)` 以 `[bone_id] = bone_name` 的形式返回对象所有骨骼的表。如果 `bHud` 为 true，则使用 hud 模型
* damoldavskiy：修复了脚本附件的 MAS (https://github.com/themrdemonized/xray-monolith/pull/144)

**2025.03.14**
* `use_separate_ubgl_keybind` 用于启用独立的 UBGL 按键绑定，或恢复为原版系统
* damoldavskiy：Modular Attachment System：备用位置、变焦档位数量 (https://github.com/themrdemonized/xray-monolith/pull/143)
* Lucy：脚本附件 (https://github.com/themrdemonized/xray-monolith/pull/140)

**2025.03.13**
* Bence7661：允许在换弹时使用自由视角 (https://github.com/themrdemonized/xray-monolith/pull/139)

**2025.03.12**
* Bence7661：UBGL/备用瞄准分离功能的俄语翻译 (https://github.com/themrdemonized/xray-monolith/pull/138)

**2025.03.09**
* damoldavskiy：Modular Attachment System 的修复 (https://github.com/themrdemonized/xray-monolith/pull/136)
* ZoulKrystal：预生成的防毒面具雨声 (https://github.com/themrdemonized/xray-monolith/pull/137)

**2025.03.07**
* damoldavskiy：Modular Attachment System (https://github.com/themrdemonized/xray-monolith/pull/135)

**2025.03.06**
* `actor_on_update_pickup` 回调，用于检查是否有可拾取的物品

**2025.03.05**
* Bence7661：可分别为 UBGL（下挂榴弹发射器）和备用瞄准切换（倾斜、激光等）设置按键绑定 (https://github.com/themrdemonized/xray-monolith/pull/134)

**2025.02.28**
* DXML:
  * 为 `on_xml_read` 回调添加 `flags` 参数。目前支持的 flags：
    * cache - 如果 `flags.cache = true`，则 DXML 的结果会被缓存，下次引擎处理该 XML 文件时不会再触发该回调
  * 修复了 `dxml_core.openXMLFile` 工具函数中的拼写错误

**2025.02.25**
* 新增引擎导出：
  * db.actor:get_actor_object_looking_at() - 获取角色正在注视且处于交互半径内的对象
  * db.actor:get_actor_person_looking_at() - 获取角色正在注视且处于交互半径内的物品栏持有者
  * db.actor:get_actor_default_action_for_object() - 获取角色所注视对象的交互动作

**2025.02.22**
* Bence7661：弹匣供弹榴弹发射器类 -> SSRS（允许通过 mod 加入弹匣供弹榴弹发射器）(https://github.com/themrdemonized/xray-monolith/pull/130)

**2025.02.19**
* VodoXleb：`g_firepos_zoom` 用于在瞄准时让子弹从枪管发射 (https://github.com/themrdemonized/xray-monolith/pull/129)

**2025.02.07**
* GAMEMTL_SUBITEM_COUNT 从 10 提高到 20

**2025.01.26**
* privatepirate：重新设计 Modded Exes 选项菜单

**2025.01.25**
* `db.actor:get_actor_ui_luminosity()` 用于获取 HUD 上显示的光照度，范围从 0 到 1

**2025.01.22**
* `allow_silencer_hide_tracer` cvar 默认为 0

**2025.01.19**
* TIHan：改善卡顿，禁止在脚本上运行 'net_Relcase' (https://github.com/themrdemonized/xray-monolith/pull/122)

**2025.01.17**
* 可为武器设置 `fire_point_silencer`。在安装消音器时调整开火点，默认值为 fire_point 的值。同时支持 HUD 和世界模型

**2025.01.13**
* privatepirate：修复 HDR 设置 UI

**2024.12.11**
* 修复「无法从某些武器上拆下瞄具」(https://github.com/themrdemonized/xray-monolith/issues/114)

**2024.12.10**
* momopate (https://github.com/themrdemonized/xray-monolith/pull/112):
  * 添加控制台命令：
    * `fix_avelocity_spread` -> Bool；修复移动相机不影响精度的问题。
    * `apply_pdm_to_ads` -> Bool；使武器上的 PDM 值在开镜瞄准时被正确应用。
    * `smooth_ads_transition` -> Bool；使进入和退出 ADS 时平滑地改变精度加成，而不是仅在完全进入 ADS 时才改变。
    * `allow_silencer_hide_tracer` -> Bool；可选择禁用消音器完全隐藏曳光弹的能力。（默认为 true）
  * 为武器 section 添加 `silenced_tracers`（bool）字段。如果该弹药类型在使用消音武器时可被隐藏，将此字段设为 true 会重新显示曳光弹。
  * 添加 `GetZoomRotateTime()` 和 `SetZoomRotateTime(float val)`。
  * 为 `level.add_bullet()` 所用的表添加 `senderweapon`，以改善与 mod 的兼容性。默认为 sender。
  * 为 `SetActorPosition()` 添加 `bkeep_speed`。
  * 添加 `SetMovementSpeed()`。作为 `set_movement_speed(vector vel)` 导出以供脚本使用。

**2024.12.09**
* SSS 22 更新

**2024.11.30**
* 将 `_g.script` 补丁移到单独的脚本中，并将其加入 `script.ltx` 的加载
* RavenAscendant：alife_clone_weapon 修复

**2024.11.27**
* 可通过在控制台或 Modded Exes 设置中输入 `mouse_sens_vertical` 来更改垂直鼠标灵敏度系数。该数值是对整体鼠标灵敏度的乘数，默认为 1

**2024.11.10**
* 新的缩放增量算法：改变瞄具的缩放档位，使各档位之间具有相同的视觉缩放变化，要启用它在控制台输入 `new_zoom_delta_algorithm 1` (https://github.com/themrdemonized/xray-monolith/issues/98)

**2024.11.09**
* 临时禁用 cform_cache (https://github.com/themrdemonized/xray-monolith/issues/100)

**2024.11.08**
* Lucy、RavenAscendant、NLTP_ASHES：探测器的透明 shader 修复 (https://github.com/nltp-ashes/Western-Goods/releases/tag/fix-vanilla-shader)

**2024.11.04**
* `level.map_get_object_spots_by_id(id)` 用于获取与 `id` 关联的所有地图对象标记

**2024.11.02**
* 可在武器升级中定义 `scope_texture` 和 `scope_texture_alt`

**2024.11.01**
* 幽灵（Poltergeist）死亡时将始终生成可见尸体。此前火焰型幽灵和伪幽灵只会生成碰撞网格而没有可见网格。要恢复旧行为，请在控制台输入 `poltergeist_spawn_corpse_on_death 0`  

**2024.10.28**
* VodoXleb：添加 explosive_item_on_explode 回调 (https://github.com/themrdemonized/xray-monolith/pull/97/files)

**2024.10.26**
* VodoXleb：添加可在 Lua 中使用的 torch 强制更新函数 (https://github.com/themrdemonized/xray-monolith/pull/96)

**2024.10.22**
* `aaa_sound_object_patch.script` 文件：重做 `sound_object` 类以缓存正在播放的音效，修复因 LuaJIT GC 导致音效被突然截断的问题

**2024.10.19**
* Ishmael：允许 addon 隐藏加载提示 (https://github.com/themrdemonized/xray-monolith/pull/91)
* 添加控制台命令 `print_bone_warnings`，用于切换在使用 bone_position 和 bone_direction 函数并遇到无效骨骼时是否打印警告

**2024.10.18**
* 修复了动态缩放瞄具的初始缩放档位错误
* `level.add_bullet` 支持以表作为参数来定义子弹属性。可用字段请参考 `lua_help_ex.script` 文件

**2024.10.14**
* deggua：修复因 PRNG 种子不佳导致的草地图案问题 (https://github.com/themrdemonized/xray-monolith/pull/86)

**2024.10.10**
* damoldavskiy：添加 hud_fov_aim_factor 控制台参数，可在 ADS 时降低 hud_fov。对 3D 瞄具很有用。默认为 0 (https://github.com/themrdemonized/xray-monolith/pull/83)

**2024.10.07**
* damoldavskiy：最小/最大瞄具缩放 fov 参数 (https://github.com/themrdemonized/xray-monolith/pull/81)
* 由于 3DSS 兼容性问题，回退了「修复 HUD 抖动」(https://github.com/themrdemonized/xray-monolith/pull/75/commits/98022a2a2cd17204c0ffac215d541425940042ba)
* 修复加载存档时榴弹发射器弹药问题 (https://github.com/themrdemonized/xray-monolith/issues/79)

**2024.10.06**
* Hozar2002：大型关卡支持，并修复 HUD 模型在远离地图原点处的渲染 (https://github.com/themrdemonized/xray-monolith/pull/75)
* damoldavskiy：缩放档位数量调整及少量重构 (https://github.com/themrdemonized/xray-monolith/pull/80)
* 新增 `ltx_help_ex.script` 文件，用于描述引擎支持的额外 LTX 参数

**2024.10.05**
* deggua：HDR10 泛光、镜头光晕、杂项更新 (https://github.com/themrdemonized/xray-monolith/pull/78)
* 若启用 HDR，则禁用截图功能以防止崩溃
* 在 Modded Exes 设置中添加 HDR 实时编辑

**2024.10.01**
* 为 ActorMenu_on_item_after_move 修复添加游戏版本检查
* DXML：添加对空 XML 的检查，修复了对其使用 `query` 时的崩溃
* 默认 `r__3Dfakescope 0`

**2024.09.29（[需要全新安装](https://github.com/themrdemonized/xray-monolith?tab=readme-ov-file#troubleshooting)）**
* Anomaly 1.5.3 更新

**2024.09.28**
* 加载游戏时重置鼠标状态

**2024.09.25（需要全新安装）**
* 在 Github Action 期间将 Gamedata 文件打包为 `00_modded_exes_gamedata.db0` 归档

**2024.09.23**
* Anomaly 1.5.3 Test3 更新

**2024.09.20**
* Anomaly 1.5.3 Test2 更新

**2024.09.19**
* Anomaly 1.5.3 Test1 更新

**2024.09.15**
* deggua：针对窗口模式的 DX11 DWM 垂直同步修复 + 修复损坏的模糊通道 (https://github.com/themrdemonized/xray-monolith/pull/72)

**2024.09.14（安装前请删除 Anomaly 文件夹内的 gamedata/shaders/r3 文件夹）**
* deggua：修复 HDR10 渲染 (https://github.com/themrdemonized/xray-monolith/pull/71)

**2024.09.12**
* GhenTuong: 
  * 通过使用单线程更新修复粒子闪烁。
  * 对象可在 section 配置中使用 "ignore_collision" 来忽略与其他对象的碰撞。可以是地图几何体、其他物理对象和生物。
  `ignore_collision = map,obj,npc`
  * 允许在 section 配置中使用 "on_physic_contact"，在发生碰撞时运行脚本。
  * 允许在 section 配置中使用 "on_explode"，在爆炸物爆炸时运行脚本。

  * 修复了一个 bug：所有类型的榴弹发射器都会生成与弹药数量相等的榴弹对象，并且在卸载时不会释放旧的榴弹对象。
  * 允许在榴弹弹药的 section 配置中使用 "ammo_grenade_vel" 来使用不同的速度
* damoldavskiy：可选择让僵尸在热成像中不可见 (https://github.com/themrdemonized/xray-monolith/pull/69)

**2024.09.09**
* Ascii1457：修复了没有 SSS 的武器导致的 FPS 下降

**2024.09.08**
* Redotix：3DSS SSS21 兼容性及 Modded exes 菜单开关 (https://github.com/themrdemonized/xray-monolith/pull/67)

**2024.09.07**
* Ascii1457：修复了没有 SSS 时的水面渲染

**2024.09.06**
* deggua：修复 dx10HW.cpp 中的 D3D 管理问题 (https://github.com/themrdemonized/xray-monolith/pull/66)

**2024.09.04**
* damoldavskiy：支持 Shader 3D Scopes 即将推出的更新 (https://github.com/themrdemonized/xray-monolith/pull/64)
* deggua：修复 HUD 采样器与 MSAA 设置相关的 HDR10 问题 (https://github.com/themrdemonized/xray-monolith/pull/65)

**2024.09.02**
* deggua：为 DX11 渲染器添加 HDR10 输出支持 (https://github.com/themrdemonized/xray-monolith/pull/63)
  * 向控制台变量添加 HDR 参数。
  * `r4_hdr_on` 命令用于启用 HDR。需要重启，默认为禁用。
  * `r4_hdr_whitepoint_nits` 用于设置显示器的 HDR 白点（最大亮度）。
  * `r4_hdr_ui_nits` 用于以 nits 为单位设置 UI 亮度，应设置为低于白点、且让用户感到舒适的数值。
  * `r4_hdr_colorspace` 用于设置 HDR 色彩空间
  * 所有选项均可在 Modded Exes 选项菜单中找到
  * 随附的 shader 可与原版 Anomaly 配合使用。为兼容 SSS 和 GAMMA，请从此处下载 GAMMA shader https://github.com/deggua/xray-hdr10-shaders/releases/tag/v3
  
**2024.08.25**
* Redotix：3D Shader scopes (3DSS) 渲染调整 (https://github.com/themrdemonized/xray-monolith/pull/62)：
  * 为 3DSS 镜片提供自定义 shader flags 及自定义渲染顺序
  * 用于渲染瞄具准星的渲染阶段
  * 一个渲染目标，在 hud 渲染开始前对场景的 z 缓冲进行采样

**2024.08.20**
* SSS 21 更新

**2024.08.18**
* NLTP_ASHES (https://github.com/themrdemonized/xray-monolith/pull/61):
  * 基于 LTX 的巡逻路径定义
  * 支持在 `CWeaponMagazined` 中自定义首发枪声音效

    示例：
    ```![wpn_pkm]
    snd_shoot_actor_first             = path\to\my_sfx
    snd_silncer_shoot_actor_first     = path\to\my_sln_sfx
    ```
  * 在 `game_object:object("section")` 函数中支持物品栏容器
  * 修复 `ActorMenu_on_item_after_move` 回调在 obj 参数中发送 nil 值的问题
  * 修复当物品从槽位中移出时 `actor_on_item_take_from_box` 回调不触发的问题

**2024.08.10**
* 由于图形问题，将 `VIEWPORT_NEAR` 值恢复为原版

**2024.08.04**
* VodoXleb：添加 viewport_near 控制台命令以更改相机近裁剪值

**2024.07.21**
* VodoXleb：添加用于获取服装的 artefact_count 以及物品栏中 belt_size 的回调。

**2024.07.06**
* 修复了对敌人使用探测器瞄具时可能出现的 0xffff 地址违例崩溃

**2024.07.05**
* 新增 Lua 函数：
  * obj:get_scope_ui() 返回包含以下数据的表
    * `name` - 武器当前使用的 scope_texture 名称
    * `uiWindow` - 瞄具 UI 的 CUIWindow 实例
    * `statics` - CUIWindow 瞄具 UI 实例所使用的 CUIStatic 数组
  * obj:set_scope_ui(string) 用于按贴图名称设置瞄具 UI（例如 `wpn_crosshair_mosin`）

**2024.06.12**
* GhenTuong：修复与 PhraseDialog.cpp 相关的崩溃

**2024.05.31**
* 回退了「vegeta1k95：将物品的 `max_uses` 和 `remaining_uses` 改为最大值 65535（u16）」，该功能与现有存档不兼容，会破坏使用次数和物品栏重量计算

**2024.05.30**
* NLTP_ASHES：`obj:character_dialogs()` 用于获取角色可用对话的列表

**2024.05.27**
* etapomom:
  * 添加控制台命令：
    * `allow_outfit_control_inertion_factor` *
    * `allow_weapon_control_inertion_factor` *
    * `render_short_tracers` **
  ```
      (*) 武器和服装的 control_inertion_factor 可以影响鼠标灵敏度，可通过各自的命令开关。
      (**) 启用此命令后，曳光弹将被限制在其最小长度，而不是不渲染。
  ```

  * 爆炸破片（frags）自定义：
    * `frags_ap`
    * `frags_air_resistance`
    * `frags_tracer`
    * `frags_4to1_tracer`
    * `frags_magnetic_beam_shot`
    * `frags_tracer_color_ID`

  * 服装的 `control_inertion_factor` ltx 字段现在由引擎读取
  * 消音器可以隐藏子弹曳光，可通过 `tracer_silenced` ltx 字段按弹药分别开关
  * 在 bullet_manager section 中添加曳光长度修正系数 `tracer_length_k`
* vegeta1k95：将物品的 `max_uses` 和 `remaining_uses` 改为最大值 65535（u16）

**2024.05.23**
* 修复了 `complex_mode=1` 与彩色文本一起使用时 CUIListBox 宽度不正确的问题

**2024.05.21**
* 支持使用 `rpm_mode_2` 的任意发数点射，且 `cycle_down` 参数可用
* 武器升级支持 `cycle_down` 和 `rpm_mode_2`
* `db.actor:get_talking_npc()` 函数，返回玩家正在交谈的对象

**2024.05.20**
* NLTP_ASHES, GhenTuong:
  * 修复了 script_text 节点在参数化对话中无法正常工作的问题（来自 GhenTuong 的修复）；
  * 重写（更多是重新格式化）了 PhraseDialog.cpp 的部分内容，以使其更具可读性。

**2024.05.15**
* NLTP_ASHES：hotfix(wpn-zoom-type-changed-with-gl)：修复使用榴弹发射器时武器缩放类型变更的问题

**2024.05.11**
* 修复了有问题的 xrs_facer.hit_callback 函数
* VodoXleb：可通过 `explode_effector_sect_name = effector_sect` ltx 参数为每个榴弹 section 自定义 `explode_effector`
* NLTP_ASHES：`actor_on_weapon_zoom_type_changed` Lua 回调

**2024.05.07**
* 修复了 CMovementManager 并行寻路可能导致怪物卡在地图上的问题
* 将寻路计算之间的延迟从 1.5-2.5 秒减少到 1-1.5 秒
* Lua 函数 `get_string_table()`，返回所有已翻译字符串文本及对应字符串 ID 的表

**2024.05.06**
* 修复了 `item_device.on_anomaly_touch` 和 `itms_manager.actor_on_item_before_use` 回调不遵循输入的 `flags.ret_value` 的问题
* strangerism：新增 `freeze_time` 控制台命令，可冻结时间但音效仍可播放

**2024.05.03**
* vegeta1k95：`get_hud():GetWindow()` 方法

**2024.05.01**
* 修复了某些 modded exes 选项缺失翻译字符串的问题
* 新增 Lua 函数
  * `db.actor:update_weight()` - 强制更新角色的物品栏重量
  * `db.actor:get_total_weight_force_update()` - 强制更新角色的物品栏重量并返回更新后的重量

**2024.04.22**
* 为 `<list>` UI 节点添加 `complex_mode` 属性（默认禁用），它允许列表项使用彩色文本及其他特性。示例：
  ```xml
    <properties_box>
      <texture>ui_inGame2_demo_player_info_window</texture>
      <list x="0" y="0" width="10" height="10" item_height="18" always_show_scroll="0" can_select="1" bottom_indent="10" right_ident="10" left_ident="20" complex_mode="1">
          <font_s r="220" g="220" b="220" />
          <font r="150" g="150" b="150" font="letterica16" complex_mode="1" />
      </list>
  </properties_box>
  ```
* vegeta1k95:
  * 为消音器在连续射击后添加了可选且可配置的贴图发光效果，视觉上看起来像过热。

**2024.04.17**
* Tosox:
  * 幽灵（Poltergeist）和布鲁尔（Burer）可以投掷尸体，要启用它请查看 modded exes 选项菜单或在控制台输入 `telekinetic_objects_include_corpses 1`

**2024.04.12**
* 脚本命中 `.bone` 字段可以被赋值 `s_hit.bone = "bip01_spine"`，也可以被调用 `s_hit.bone("bip01_spine")`。这可能会将某些原版脚本和 mod 的行为改变为作者原本意图的方式。   
* vegeta1k95:
  * 为重力异常的行为添加新的可选念力类型：CTeleTrampolin
  * 可供在异常配置 section 中设置 `tele_type 1` 的 mod 使用。
  * CTeleTrampolin 会将陷入异常的物体弹射到极高的天空。现在你可以尝试沉浸式且符合设定的 "Springboard"/"Trampolin" 异常 :)


**2024.04.07**
* strangerism:
  * 修复了启动 demo_record 时鼠标指针仍可见的问题
  * 添加 F12 按键截图事件的传播（仅在命令 `demo_record_return_ctrl_inputs` 下可用）
  * 当 demo_record 控制权切换到 invoker 脚本并切回时切换鼠标指针可见性（仅在命令 `demo_record_return_ctrl_inputs` 下可用）
  * 添加新 API 以启用相机边界检查（限制相机与角色的距离）
  * 添加相机边界检查，确保相机不会低于角色所在地面（并非真正的地面碰撞检查）
  * 更新控制台命令 `demo_record_return_ctrl_inputs` 以启用相机边界检查
* DaimeneX：base_hud_offset_... 参数可与 PDA 一起使用

**2024.04.06**
* 安全读取 base_hud_offset_... 参数
* `on_loading_screen_key_prompt` 回调可与 `keypress_on_start 0` cvar 配合使用

**2024.04.05**
* DaimeneX：可编辑变量，允许调整武器位置而无需调整瞄准
  * base_hud_offset_pos
  * base_hud_offset_pos_16x9
  * base_hud_offset_rot
  * base_hud_offset_rot_16x9

**2024.04.03**
* vegeta1k95：修复了武器枪口闪光与烟雾粒子相互之间对齐错误的问题。

**2024.04.01**
* `npc:set_enable_movement_collision(true|false)` 用于启用或禁用 NPC 的移动碰撞
* strangerism:
  * 为 DemoRecord 添加额外的按键绑定，以支持没有小键盘的 pc/笔记本
  * 添加新的控制台命令（demo_record_return_ctrl_inputs），以在启动 DemoRecord 时让客户端脚本能够在 DemoRecord 仍在运行时接收某些按键并对其作出反应

**2024.03.30**
* vegeta1k95：修复了重力异常并不总是播放身体撕裂的粒子/音效的问题

**2024.03.26**
* vegeta1k95：为异常添加可选的 "blowout_disable_idle" 选项

**2024.03.24**
* 当调用 `level.object_by_id(nil)` 时打印 Lua 调用栈

**2024.03.21**
* vegeta1k95:
  * 修复 DX10 热成像的镜头光晕以及太阳/月亮（在上次请求中遗漏了）；
  * 通过新的控制台参数使尸体热量衰减可配置；
* Tosox:
  * 新增 npc_on_item_before_pickup 回调

**2024.03.18**
* vegeta1k95:
  * 修复了镜头光晕——来自太阳等——被渲染在红外图像之上的问题。
  * 将太阳/月亮渲染到热成像 RT 中——使它们在红外中能够显示为热源。

**2024.03.12**
* SSS 20.2 更新
* 修复 DX10 下的水面渲染

**2024.03.11**
* damoldavskiy：标记 Switch shader 参数

**2024.03.08**
* Lucy, VodoXleb：新增 `on_phrase_callback`

**2024.03.05**
* SSS 20 更新

**2024.02.28**
* 崩溃存档在加载画面之后启用，并在玩家 net_destroy 时禁用。这应能修复虚假的 `db.actor is nil` Lua 报错
* MOUSEBUFFERSIZE 从 64 增加到 1024
* KEYBOARDBUFFERSIZE 从 64 增加到 128
* 可通过控制台命令 `mouse_buffer_size` 和 `keyboard_buffer_size` 调整鼠标与键盘缓冲区大小

**2024.02.05**
* Xr_ini.cpp 修复
* 修复了 `bullet_on_init` 回调中子弹速度不正确的问题

**2024.02.01**
* 将 `mouse_sens_aim` 控制台命令的上限提高到 0.01-5.0

**2024.01.31**
* LVutner：compute shader 修复

**2024.01.30**
* Hrust：修复体积雾

**2024.01.27**
* 修复：`[error]Expression    : left_eye_bone_id != u16(-1) && right_eye_bone_id != u16(-1), CBaseMonster::update_eyes_visibility`
* 重新启用 robinhood 哈希

**2024.01.26**
* 临时禁用 robinhood 哈希，原因：`[error]Expression    : left_eye_bone_id != u16(-1) && right_eye_bone_id != u16(-1), CBaseMonster::update_eyes_visibility`
* 在 `you are trying to use a destroyed object` 报错时打印 Lua 堆栈
* 崩溃时在日志中打印 CAI_Stalker::net_Export 的错误信息
* MagielBruntink：仅在调试模式下打印缺失的 `.ogg` 注释

**2024.01.18**
* MagielBruntink：将 lua_gcstep 的默认值提高到 400，并允许通过 `lua_gcstep` 命令在控制台修改 

**2024.01.13**
* 修复 GameMtlLib.cpp 中的拼写错误
* OneMorePseudoCoder：内存泄漏修复

**2024.01.11**
* 新增通过 `materials.ltx` 和 `material_pairs.ltx` 文件覆盖 gamemtl.xr 材质并定义新材质的功能。请阅读 `gamedata/materials` 文件夹中这些文件里的指南
* 移除了 `pSettings->line_exist(sect_name,"fire_point")==pSettings->line_exist(sect_name,"fire_bone")` 游戏崩溃检查
* 将材质音效与粒子所用的 `GAMEMTL_SUBITEM_COUNT` 常量从 4 提高到 10
* 修复了罕见的 unlocalizer 崩溃以及由此 bug 导致的 Linux 崩溃

**2024.01.04**
* 新增 `db.actor` 导出
  * `db.actor:get_actor_crouch_coef()`
  * `db.actor:set_actor_crouch_coef(float)`
  * `db.actor:get_actor_climb_coef()`
  * `db.actor:set_actor_climb_coef(float)`
  * `db.actor:get_actor_walk_strafe_coef()`
  * `db.actor:set_actor_walk_strafe_coef(float)`
  * `db.actor:get_actor_run_strafe_coef()`
  * `db.actor:set_actor_run_strafe_coef(float)`
  * `db.actor:get_actor_sprint_strafe_coef()`
  * `db.actor:set_actor_sprint_strafe_coef(float)`
* 为头盔升级新增 `hit_fraction_actor` 字段
* 为 section 条目新增 filename 字段
* 为 DLTX 新增 ini 脚本方法
  * dltx_print(string sec = nil, string line = nil)：打印 section 的信息。如果 sec 为 nil，则打印整个文件。如果提供了 section 且 line 为 nil，则打印整个 section。如果两者都提供，则只打印所提供的那一行
  * dltx_get_filename_of_line(string sec, string line)：返回该 section 中某一行所使用的文件名
  * dltx_get_section(string sec)：返回包含 section 信息的表，结构如下：
  ```
    {
      [<section_1>] = {
        name = <section_1>
        value = <value_1>
        filename = <filename_1>
      }
      ...
    }
  ```
  * dltx_is_override(string sec, string line)：如果 section 中的某一行被 DLTX mod 覆盖，则返回 true

**2023.12.30**
* 为脚本 hit 结构体新增 `bullet_id` 字段
* 恢复 `r_bool_ex` 和 `r_value` 函数的行为，使其与原版代码一致
* 如果不存在 `pdb` 文件，则过滤堆栈跟踪中的无用行

**2023.12.24**
* SSS 更新
* 小地图形状修复已改为选项。在 `zone_map_*.xml` 文件的 `level_frame` 节点中添加 `ratio_mode = "1"` 以启用新代码

**2023.12.23**
* DLTX：简化了删除整个 section 的方式，现在只需写入 `!![section]` 即可将其彻底删除
* Longreed：新增让小地图边框使用自定义矩形形状的功能 

**2023.12.17**
* 修复了因 robin_hood 哈希导致 ini 缓存获取到错误值的问题，已重新启用该库

**2023.12.16u1**
* 临时禁用 robin_hood 哈希以修复崩溃

**2023.12.16**
* `xr_unordered_map` 类型已替换为来自 https://github.com/martinus/robin-hood-hashing 的 robin_hood 哈希。此外还引入了基于该库的 `xr_unordered_set` 和 `xr_pair` 类型
* 在 CIniFile 类中新增基于引擎的 ini 值缓存
* 新增 Lua 函数 `ini_file:get_filename()`，用于返回文件的名字
* 将 `ini_file_ex` 类替换为派生自 `ini_file` 的实现，与现有 `ini_file_ex` 完全向后兼容
* `level.iterate_nearest` 现在会在执行 Lua 回调之前按距离升序对对象排序
* 新增 `game.update_pda_news_from_uiwindow(CUIWindow*)` 函数，用于通过 HUD 上的新闻窗口更新 PDA 中的新闻
* 通过 StackWalker 库 https://github.com/JochenKalmbach/StackWalker 新增在日志中打印引擎堆栈跟踪
  * 要使其生效，你需要下载与你 DX/AVX 版本对应的 `pdb` 文件，并将其放到与 `exe` 文件相同的位置。PDB 文件在这里：https://github.com/themrdemonized/xray-monolith/releases/latest

**2023.12.09u1**
* 修复了使用 `SYS_GetParam` 时 `tonumber` 转换失败导致的崩溃

**2023.12.09**
* 禁用了针对 bool 变量的 get_console 警告
* Lua unlocalizer 支持 `_g.script`
* Lua unlocalizer 的文件名检查不区分大小写，即 `[_G]` 与 `[_g]` 相同
* 在使用 `SYS_GetParam` 以及读取 `r_string_ex` 之类的变量时，禁用了 ini 文件的 Lua 缓存，改用引擎函数，以减少内存占用和可能的 GC 调用
* 将未文档化的 Lua 扩展导出加入 `lua_help_ex.script`
* 新增回调 `on_news_received`，用于在游戏新闻显示时对其进行操作
* 新增 `game.change_game_news_show_time` 函数，用于推迟游戏新闻的淡出
* 新增 `throttle(func, time)` 函数，用于将 `func` 的调用按毫秒为单位的 `time` 进行节流


**2023.12.06**
* 为 `actor_on_hud_animation_mark` 和 `
actor_on_hud_animation_play` 回调新增枪械对象与枪械持有者参数
* 当脚本因类型不正确而无法获取控制台变量时给出警告

**2023.12.02**
* 新增通过控制台命令 `freelook_cam_limit` 编辑自由视角角度限制的功能
* 使用 /Ob3 编译选项，理论上应能略微提升性能

**2023.11.28**
* Lucy：为手电筒的取出/收起动画添加一个运动标记，在灯光开启或关闭时触发

**2023.11.25**
* 新增用于操作地图标记图形的函数
  * `level.map_get_object_spot_static(u16 id, LPCSTR spot_type)` 将返回地图上该标记的 CUIStatic 对象
  * `level.map_get_object_minimap_spot_static(u16 id, LPCSTR spot_type)` 将返回小地图上该标记的 CUIStatic 对象
* 如果 DXML 查询无效，游戏将带着错误信息和调用栈崩溃

**2023.11.22**
* GhenTuong：对话中的参数化函数（precondition、action、script_text 标签）
  * 可以使用 '=' 和 '!'，其作用与 condlist 相同。`=` 表示条件为真，`!` 表示条件为假
  * 可以使用 () 来输入参数。参数之间用 ':' 分隔
  * 示例：`<precondition>!file.func(dolg:1:false)</precondition>`
  ```lua
    function func(a,b,dialog_id,phrase_id,next_id,p)
      printf("%s %s %s %s %s %s", a, b, dialog_id, phrase_id, next_id, table.concat(p, " "))
    end
  ```
* 新增在控制台和日志文件中写入时间戳的选项。在控制台输入 `log_timestamps 1` 即可开启该功能
* DXML 查询函数即使在查询无效时也始终返回表

**2023.11.15**
* 修复了在类似莫辛的瞄具上尝试使用替代瞄准时的崩溃

**2023.11.14**
* 修复了挂载榴弹发射器会触发 shader scopes 的问题

**2023.11.12**
* 修复了使用 obj:force_set_position(pos, true) 时的物理初始化
* 修复了替代瞄具为光学瞄具时 shader scopes 不工作的问题
* 从主菜单界面的版本标题中移除了 (DLTX, DXML, ...) 字符串

**2023.10.27**
* 撤销了对鼠标滚轮的改动，现在它不再反转
* 通过控制台命令 `mouse_wheel_invert_zoom 1` 切换反转鼠标滚轮 

**2023.10.20**
* 由于会造成大幅掉帧，移除了怪物卡住的修复。你可以通过控制台命令 `monster_stuck_fix 1` 将其重新开启

**2023.10.17**
* SSS 18 更新
* Lucy：可通过 `detector_ui_bone` 属性在探测器的 hud section 中为 Svarog/Veles 指定自定义 UI 骨骼
* `string_table_error_msg` 控制台命令，用于打印缺失的翻译字符串 

**2023.10.05**
* 修复了在 PDA 地图上右键时偶发的崩溃
* `db.actor:set_actor_direction` 支持将 roll 作为第三个参数

**2023.09.30**
* 新增禁用鼠标滚轮切换武器的功能，使用控制台命令 `mouse_wheel_change_weapon 0`
* 修复了在使用不可见的调试 gizmo 时控制台不可见的 bug
* `db.actor:set_actor_direction` 支持将 pitch 作为第二个参数
* 对右键 PDA 回调的小幅修复
* 当为骨骼函数提供了不正确的 bone id 时，新增堆栈打印
* 将 LuaJIT 迁移到 VS2022 工具链，并为构建应用了优化选项 

**2023.09.22**
* 新增 `on_map_right_click` 回调，它允许在地图上任意位置右键，并在投影后的真实世界坐标处触发用户函数。请参阅 `callbacks_gameobject.script`
* 新增在地图上相对于光标而非始终以中心进行缩放的功能
* 修复了使用脚本函数获取骨骼属性时可能发生的崩溃
* 新增临时修复，解决使用 2D PDA 时查看 NPC PDA 的功能无法正常工作的问题
* 在 `lua_help_ex.script` 中新增 `player_hud` 函数的文档
* 新增全局函数
  * `nextTick(f, n)` 将在接下来的第 n 个游戏 tick 执行函数 f。如果你希望它只触发一次，你的函数应返回 true，就像时间事件一样
  * `print_tip(s)` 会在新闻推送以及控制台中打印信息 

**2023.09.06**
* 将构建流程迁移到 https://github.com/themrdemonized/xray-monolith 上的 Github Actions
* 除了 exe 本身之外，如果你想自己调试代码，https://github.com/themrdemonized/xray-monolith 仓库还将包含 PDB 文件
* 由 GhenTuong 为游戏对象新增 `angle` 和 `force_set_angle` 函数
* 新增 `get_modded_exes_version` 函数

**2023.09.02**
* 提升了调用 level.object_by_id 函数时的性能，感谢 Lucy 提供的线索

**2023.08.29**
* 为 `get_physics_shell()` Lua 对象新增 `apply_torque` 函数，用于对模型施加旋转力
* 由 ![clayne](https://github.com/clayne) 提交的修复
  * https://github.com/themrdemonized/xray-monolith/pull/14

**2023.08.19**
* 移除了 diff 文件，请 fork XRay-Monolith 仓库以参与引擎开发，并通过 pull request 提交改动：https://github.com/themrdemonized/xray-monolith
* 为 `on_before_hit_after_calcs` 回调新增 boneId 参数
* 如果崩溃是由 Lua 引起的，则给出更具描述性的错误信息
* 由 ![clayne](https://github.com/clayne) 提交的修复
  * https://github.com/themrdemonized/xray-monolith/pull/1
  * https://github.com/themrdemonized/xray-monolith/pull/2
  * https://github.com/themrdemonized/xray-monolith/pull/3
  * https://github.com/themrdemonized/xray-monolith/pull/4
  * https://github.com/themrdemonized/xray-monolith/pull/5
  * https://github.com/themrdemonized/xray-monolith/pull/6
  * https://github.com/themrdemonized/xray-monolith/pull/7
  * https://github.com/themrdemonized/xray-monolith/pull/8
  * https://github.com/themrdemonized/xray-monolith/pull/9
  * https://github.com/themrdemonized/xray-monolith/pull/10
  * https://github.com/themrdemonized/xray-monolith/pull/11
  * https://github.com/themrdemonized/xray-monolith/pull/12
  * https://github.com/themrdemonized/xray-monolith/pull/13
  
**2023.08.09**
* 减小了 exe 的体积
* 修改构建流程，以解决在某些 CPU 配置下游戏无法启动的问题
* 恢复 `bone_position` 和 `bone_direction` 函数的原始行为，以解决某些 mod 的问题。如果为它们指定了不正确的 bone_id，将在控制台打印警告

**2023.08.08**
* SSS 更新
* 尝试解决在某些 CPU 配置下游戏无法启动的问题。非常感谢 ![clayne](https://github.com/clayne) 投入时间和精力来解决该问题

**2023.08.07**
* Lucy：修复损坏的左手动画

**2023.08.06**
* SSS 更新

**2023.08.05**
* 通过在首次更新时强制 `r__fakescope 1`，修复了 shader scopes 可能出现的故障
* 新回调 `on_before_hit_after_calcs`，将在对实体施加命中之前触发，请参阅 `callbacks_gameobject.script`
* 在使用 `level.set_cam_custom_position_direction` 函数时可以保持 hud 被绘制并受影响，请参阅 `lua_help_ex.script`
* 由 Lucy 提供的功能与修复：
  * 如果设备/探测器的 hud section 中 lh_lead_gun = true，其动画现在可以使用 lead_gun 骨骼（脚本动画也可以在其 section 中使用此配置开关）
  * 即使你右手拿着东西，设备/探测器的相机动画现在也会播放
  * 清理了 player_hud 动画代码

**2023.07.28**
* 由 LVutner 提供的功能：
  * 新增对 DX9 上 Shader Scopes 的支持
  * 新增对 DX10 上 Heatvision 的支持
* 新增文件 `aaaa_script_fixes_mp.script`，其中将包含用于修复某些原版脚本的 monkeypatch
  * 修复了 `actor_on_item_before_pickup` 回调的不正确行为
* 修复了输入文本时无法切换到俄语的问题
* 在 readme 文件中新增故障排查章节

**2023.07.24**
* exe 现在使用 Visual Studio 2022 构建。如果你遇到问题，请确保安装了最新的 Visual C++ 可再发行组件包。可在此处找到：https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/
* 由 Lucy 提供的修复与功能
  * Reshade shader 将不会影响 UI，需要完整支持 addon 的 Reshade 版本
  * 修复从 blender 导出的手部（你不再需要重新指定动作引用）
  * 修复在脚本中获取玩家精准度时的静默错误/脚本卡死
  * 动画修复（霰弹枪弹壳与装填弹壳和关闭动画不同步）
  * 游戏在加载存档时不再因缺少物品 section 而崩溃（现在应该可以在游戏中途卸载大多数 mod 了）
  * 修复变种怪物卡在原地跑动的问题（非常感谢 Arszi 发现该问题）
  * 修复双手握持的探测器/设备动画（摇摆现在应用于双臂，而不再只应用于左臂）
  * 现在可以在 hud_mode 下使用 :play(true) / :play_at_pos(pos, true) 播放脚本粒子效果
  * 当因空翻译字符串而崩溃时，游戏现在会显示崩溃信息
  * 脚本化调试渲染函数，可绘制调试用的方框、球体和线条 
  * 调试渲染器可在 DX10/11 上工作
    * 非常感谢 OpenXRay 和 OGSR 的作者：
    * https://github.com/OpenXRay/xray-16/commit/752cfddc09989b1f6545f420a5c76a3baf3004d7
    * https://github.com/OpenXRay/xray-16/commit/a1e581285c21f2d5fd59ffe8afb089fb7b2da154
    * https://github.com/OpenXRay/xray-16/commit/c17a38abf6318f1a8b8c09e9e68c188fe7b821c1
    * https://github.com/OGSR/OGSR-Engine/commit/d359bde2f1e5548a053faf0c5361520a55b0552c
  * 向 lua 导出了更多向量和矩阵函数
  * 为 world2ui 新增可选第三个参数，使位置即使在屏幕外也返回，而不是 -9999
  * 统一了骨骼 lua 函数并使其更安全
  * 现在可以在 lua 中获取玩家第一人称手部的骨骼数据
* 所有新函数都添加在 `lua_help_ex.script` 中

**2023.07.21**
* 新增 `level.get_target_pos()` 和 `level.get_target_result()` 函数，请参阅 lua_help_ex.script
* 新增 `actor_on_changed_slot` 回调，请参阅 `callbacks_gameobject.script`
* 由 Lucy 提供的热修复：游戏窗口可能失去鼠标焦点的问题

**2023.07.09**
* 在 callbacks_gameobject.script 中为 NPC 新增 `anomaly_on_before_activate` 事件，使它们在启用寻路时更不易受到异常点影响
* 新增设置弹痕自定义旋转角度的功能，请参阅 lua_help_ex.script 中的 wallmarks_manager 类
* 由 Lucy 修复：3D UI 不再与天空发生裁剪，以及黑暗中的可见性问题

**2023.07.07**
* 新增 `bullet_on_update` 回调，有关该新回调的可用信息请参阅 callbacks_gameobject.script 文件
* 为子弹表新增 `life_time` 字段

**2023.07.04**
* `ai_die_in_anomalies` 命令现在可实时生效
* 修订了描述 NPC 和怪物在异常点附近行为的代码
  * 在引擎层面，异常点对 AI 始终可见，此前这由控制台变量 `ai_die_in_anomalies` 定义
  * 怪物将始终尝试避开异常点
  * NPC 的行为方式如下
    * 如果 `ai_die_in_anomalies` 为 1，它们将尝试避开异常点，如果无法避开则会受到伤害
    * 否则这由每个 NPC 单独定义（默认是原版行为——不寻路也不受伤），可通过脚本修改
* 为 NPC 新增的游戏对象函数：
  * npc:get_enable_anomalies_pathfinding() - 获取异常点寻路的状态
  * npc:set_enable_anomalies_pathfinding(bool) - 启用或禁用异常点寻路
  * npc:get_enable_anomalies_damage() -  获取异常点伤害的状态
  * npc:set_enable_anomalies_damage(bool) -  启用或禁用异常点伤害

**2023.07.03**
* 新增 level.map_remove_all_object_spots(id) 函数

**2023.07.01**
* 鼠标滚轮回调 `on_mouse_wheel` 在 flags.ret_value 为 false 时可以吞掉输入，有关该新回调的可用信息请参阅 callbacks_gameobject.script 文件
* 新增全局变量 MODDED_EXES_VERSION

**2023.06.30**
* 新增鼠标滚轮回调 `on_mouse_wheel`，有关该新回调的可用信息请参阅 callbacks_gameobject.script 文件
* 新增 `db.actor:get_actor_lookout_coef()` 和 `db.actor:set_actor_lookout_coef(float)` 函数，用于调整最大倾身角度
* 修复在第一人称和第三人称视角之间切换时枪械消失的问题

**2023.06.17**
* 新增 `bullet_on_init` 回调，有关该新回调的可用信息请参阅 callbacks_gameobject.script 文件
* 对 dxml_core.script 的小幅清理

**2023.06.04**
  * SSS 更新

**2023.05.29**
  * 如果某个字符串缺少翻译，引擎将回退使用该字符串的英文文本。要禁用该行为，请使用控制台命令 `use_english_text_for_missing_translations 0`

**2023.05.27**
  * `local res, obj_id = game.ui2world(pos)` 用于从 ui 坐标（即鼠标光标）反投影到世界

**2023.05.22**
  * `alife():iterate_object(function(se_obj))` 函数，用于遍历游戏中存在的所有对象
  * `level.set_cam_custom_position_direction(Fvector position, Fvector direction)` 和 `level.remove_cam_custom_position_direction()`，用于在世界坐标中操作相机
  * CWeapon 附加方法：潜行者上的世界模型调整
    * function Set_mOffset(Fvector position, Fvector orientation)
    * function Set_mStrapOffset(Fvector position, Fvector orientation)
    * function Set_mFirePoint(Fvector position)
    * function Set_mFirePoint2(Fvector position)
    * function Set_mShellPoint(Fvector position) 

**2023.04.27**
* SSS 更新

**2023.04.15**
* 为神器新增 `get_artefact_additional_inventory_weight()` 和 `set_artefact_additional_inventory_weight(float)`

**2023.04.09**:
* 将额外的 shader 文件放入 000_shader_placeholder.db0，必须清理 SHADER 缓存

**2023.04.08**:

* SSS 更新
* 由 vegeta1k95 提供的可配置更新速率的平滑粒子
  * 要更改更新速率，请使用控制台命令 `particle_update_mod`，其取值范围为 0.04 到 10.0（默认 1.0）。1.0 对应 30hz，0.5 对应 60hz，依此类推。该设置也可在选项菜单的 "Modded Exes" 分组中找到
  * 可以在 .pe 文件中以毫秒为单位设置粒子更新增量，通过 `update_step` 字段进行微调

**2023.03.31**:

* SSS 更新
* 移除了引擎中腰带上最多 5 个神器的限制

**2023.03.25**:

* 来自 OGSR Engine 仓库、针对怪物卡住的潜在修复，位于 `control_animation_base_accel.cpp`
* 新增 `bullet_on_impact` 和 `bullet_on_remove` 回调，有关这些新回调的可用信息请参阅 callbacks_gameobject.script 文件

**2023.03.19**:

* 新增真正的第一人称死亡镜头（默认启用），它会在玩家死亡时停留在玩家身上，并根据玩家头部的变换做出相应反应。附加控制台命令
  * `first_person_death` // 启用第一人称死亡镜头
  * `first_person_death_direction_offset` // FPD 镜头方向偏移（以度为单位）
  * `first_person_death_position_offset` // FPD 镜头位置偏移（x, y, z）
  * `first_person_death_position_smoothing` // FPD 镜头位置变化平滑
  * `first_person_death_direction_smoothing` // FPD 镜头方向变化平滑
  * `first_person_death_near_plane_offset` // FPD 镜头近裁剪面偏移

[![Watch the video](https://img.youtube.com/vi/Jm-DRNqnak0/default.jpg)](https://youtu.be/Jm-DRNqnak0)

* 修复热成像效果未应用于玩家的手的问题
* 新增 modded exes 设置的选项菜单
  * 在选项菜单中你可以调整所有新增的 modded exes 参数，例如 Shader Scopes、Sound Doppler、FPD Camera 等

![image](http://puu.sh/JC40Y/9315119150.jpg)

* 新增 `level.get_music_volume()` 和 `level.set_music_volume()` Lua 函数，用于在运行时调整音乐音量，而无需折腾控制台命令

**2023.03.15**:

* 对热成像来源的稳定性更新
* DXML 3.0 更新：
  * DXML 现在使用自己的存储来保存回调，以确保它们按照注册顺序触发
  * 新增 `insertFromXMLFile` 函数，用于读取 xml 文件的内容并插入到 xml_obj 中
  * 为 `insertFromXMLString` 和 `insertFromXMLFile` 函数新增可选参数 useRootNode，它将提示 DXML 插入所解析 XML 的根节点内容，而不是整个文件（默认：false） 

**2023.03.11**:

* 由 vegeta1k95 新增热成像支持（https://www.moddb.com/mods/stalker-anomaly/addons/heatvision-v02-extension-for-beefs-nvg-dx11engine-mod/）
* 修复了在启用 `new_zoom_enable` 命令时使用 shader scopes 导致 FOV 过大的问题

**2023.03.09**:

* 新增在加载前对脚本中的 Lua 变量解除局部化的功能，使其成为脚本命名空间的全局变量
  * 关于在脚本中对变量解除局部化，请参阅 `gamedata/configs/unlocalizers` 文件夹中测试文件里的文档
* 修复了 `scope_factor` 设置被应用于已禁用的 shader scopes 或未定义 shader 效果半径的瞄具的 bug
* 修复了武器可调瞄具升级不工作的问题

**2023.03.05**:

* 修复了同时挂载可调瞄具和榴弹发射器的武器会允许使用榴弹发射器放大的 bug。如果出于某种原因想要显式启用在榴弹发射器激活时放大，可在其 .ltx 文件中的武器 section 里添加 `scope_dynamic_zoom_gl = true`
* 可以为替代瞄具添加 shader scopes
  * `scope_dynamic_zoom_alt = true` 将为替代瞄具启用可调瞄具
  * `scope_texture_alt = <path to texture>` 将允许指定替代瞄具使用哪种准星
* 修正了启用 shader scopes 时可调瞄具的 zoom_factor 计算，你不会在引擎 FOV 之上从 shader 获得任何额外放大
* 通过 shader scopes 改变放大的 `scope_factor` 控制台命令现在可实时生效
* 降低了色差和瞄具模糊效果，提升了画面质量  

**2023.02.20**:

* 新的 SSS 更新
* 包含这些改动的新 demo-record.diff

  * 新增控制台命令：
    * `demo_record_blocked_input 1` 会启动 demo_record，但你无法移动或停止它，它旨在通过脚本执行下面的控制台命令来进行操作。控制台和 Esc 键仍可用
    * `demo_record_stop` 会停止所有已启动的 `demo_record` 命令，包括输入被屏蔽的那些 
    * `demo_set_cam_direction <head, pitch, roll>` 会设置相机朝向的方向及其 roll。注意，参数以弧度为单位。与 `demo_set_cam_position <x, y, z>` 一起使用可通过脚本操作相机

**2023.02.18**:

* 新的 SSS 更新

**2023.02.16**:

* 在 `callbacks_gameobject.script` 中新增 `gameobjects_registry` 表，它包含所有在线游戏对象并会随之更新。此外还新增了一个全局迭代器 `game_objects_iter`，它会遍历所有在线游戏对象

  ```lua
  for obj in game_objects_iter() do
    printf(obj:section())
  end
  ```

* 伪巨人的跺脚现在可以杀死并伤害任何对象、潜行者或变种怪物，而不再仅限玩家。在 callbacks_gameobject.script 中为伪巨人的攻击提供了新回调

  * 要禁用该新功能，请在控制台输入 `pseudogiant_can_damage_objects_on_stomp 0`

**2023.01.28**:

* DLTX 新增了使用同一符号 `@` 在 section 不存在时创建它、存在时覆盖它的功能。
下面是一个 `newsection` 未定义时的示例。首先用参数 `override = false` 创建它，然后用 `override = true` 覆盖它

```
@[newsection]
override = false

@[newsection]
override = true

```
* 为游戏对象新增 `bone_direction()` 函数
* 更新 `lua_help_ex.script`，加入可用的新函数

**2023.01.23**:

* MAX_TRIS 常量从 1024 增加到 16384

**2023.01.13**:

* 修复重复 section 打印损坏的问题

**2023.01.06**:

* 对于典型的第一人称模型/动画错误，游戏将打印定义了模型的 section

**2023.01.03**:

* 向 Lua 新增 CGameObject::NetSpawn 和 NetDestroy 回调（文件 callbacks_gameobject.script），要注册回调请使用

  ```lua
  RegisterScriptCallback("game_object_on_net_spawn", function(obj))
  RegisterScriptCallback("game_object_on_net_destroy", function(obj))
  ```

* DXML 将不再处理非 eng/rus 语言的翻译字符串，目前尚不支持它们
* 新增 lua_help_ex.script 文件，用于描述新的引擎导出
* 导出了额外的 CWeapon 函数，涉及武器的 RPM、操控性和后坐力
* 导出了获取和设置玩家行走加速度与后退系数的函数

  ```lua
  db.actor:get_actor_walk_accel()
  db.actor:set_actor_walk_accel(float)
  db.actor:get_actor_walk_back_coef()
  db.actor:set_actor_walk_back_coef(float)
  ```
  * DLTX 新增了向参数列表添加条目的功能，前提是该参数的结构类似 
  
  ```name = item1, item2, item3```
  
    * `>name = item4, item5` 会将 item4 和 item5 添加到列表，结果为 `name = item1, item2, item3, item4, item5`
    * `<name = item3` 会从列表中移除 item3，结果为 `name = item1, item2`
    * mod_system_...ltx 的示例： 
    
    ```
      ![info_portions]
      >files                                    = ah_info, info_hidden_threat

      ![dialogs]
      >files                                    = AH_dialogs, dialogs_hidden_threat
      
      ![profiles]
      >files                                    = npc_profile_ah, npc_profile_hidden_threat
      >specific_characters_files                = character_desc_ah, character_desc_hidden_threat
    ```

* 导出了 Fvector 的 distance_to_xz_sqr() 函数
* 重新设计了重复 section 错误，除包含重复项的文件外，它还会额外打印最初是哪个文件添加了该 section
