# 草实例化 A/B 实验协议（exp/grass-instancing）

> 目标：同一画质下 GPU 时间下降 ≥15% 且无闪烁/穿帮，才考虑进发布；达不到只留档。
> 本分支 = 主线 HEAD（819902214）+ PR #577 引擎合并 + PR 自带 r3 shader（as-authored）。
> 已知限制：PR shader 丢我方 MT/DLSS 草运动矢量（wave_prev/dir2D_prev/dlss_params），
> DLSS 下草会拖影——这是"PR 原样"基线的固有属性，不是移植完成态。

## 产物

| 构建 | exe | shader |
|---|---|---|
| 基线 | 主仓 `_build\...\AnomalyDX11AVX.exe`（=载荷 `71CB423D…`） | 载荷现役 r3（含 SSFX/MT 草逻辑） |
| 实验 | 本 worktree `_build\...\AnomalyDX11AVX.exe` | 本分支 gamedata\shaders\r3（PR as-authored） |

## 步骤（每轮相同存档、同一天气、同机位）

1. 基线轮：JSGME 挂 MT-CN（现载荷即基线）→ 清着色器缓存 → 读档 → 到选定草密集机位
   （开阔草原，镜头固定朝向）→ `rs_stats on` 记录 FPS / GPU 帧时间（Optick 更佳：
   `optick_start` → 绕机位平移 30s → `optick_stop`）。
2. 实验轮：备份现 exe → 换入本 worktree exe → 把本分支 `gamedata\shaders\r3\` 的
   5+1 个文件覆盖进 `MODS\MT-CN\gamedata\shaders\r3\`（先备份原 4 个：deffer_grass.vs、
   deffer_grass_bb.vs 若存在、deffer_detail_s/w_flat.vs、details_billboard.s）→
   **清着色器缓存（必须）** → 同存档同机位重复记录。
3. 变量档位：`r__detail_radius 50 / 100 / 200` 各一轮；记录
   `r__Detail_density_knee / r__Detail_density_curve`（新 cvar）默认值。

## 判定

- GPU 帧时间（同一机位、同半径）：实验 vs 基线 ≥15% 下降为通过线；
- 视觉：草闪烁/密度跳变/阴影与风动异常/远近期望过渡——任一穿帮即记录截图；
- DLSS 交互：开 DLSS 看草拖影程度（预期会拖影，记录严重度，作为"需 shader 融合移植"的证据）。

## 回退

删除本 worktree 即完全回退；载荷侧只需还原备份的 4 个 shader 文件 + 换回基线 exe + 清缓存。
