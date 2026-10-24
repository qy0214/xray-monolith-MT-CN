# C:\game\ANTHOLOGY 游戏+MOD 全量审查（对照 MT-CN 引擎）— 2026-10-24

审查范围：游戏安装 `Anomaly-1.5.3-Anthology 2.1`（48G）、`Modpack-1.5.3-Anthology 2.1`（67G，MO2 管理）、translation_audit（273M）、备份与崩溃史。方法：只读扫描 + 与引擎注册表（src 全部 CMD/xr_console_register 提取，1180 个命令）比对。

## 结论一览

| 项 | 状态 |
|---|---|
| MOD↔引擎 cvar 契约 | ✅ **零缺口**（见下） |
| MT-CN 载荷完整性 | ✅ 25/25（manifest 逐条 sha256 通过；1 行格式警告=BOM 头行，无害） |
| 实际运行版本 | ⚠️ **挂载的是 HDR10 修复版（5ACDE694），zwork 批次（8A0171A9）尚未挂载** |
| PiP 3DSS 安装形态 | ⚠️ Standart profile 两个 3DSS 变体同时启用（详见下）；HARD profile 只有普通 3DSS（无 PiP） |
| 引擎备份 | ⚠️ checkpoint 记录的三个 `_engine_backup_*` 目录已不存在；现行回退路径=JSGME 停用 MT-CN（!BACKUP 含原版引擎文件，已确认在位）+ 仓库 git + MODS\MT-CN.rar |
| 活动配置 | ✅ scheduler_batch_size=512；r4_upscaler/r_upscaler 均为 dlss（P1-1 修复后镜像一致） |
| 崩溃史 | ✅ 无新增（4 个 mdmp 全为 10-15~17 旧物） |

## 1. cvar 契约全量扫描（MCM 崩溃风险面）

- 扫描 `Modpack/mods` + 游戏 `gamedata` + `MT-TestKit` 全部 **169 个 *_mcm.script**，提取 `cmd="..."` 引用 **19 个**引擎命令：r4_upscaler 族 4 + r4_snow 族 13 + scope_lense_live_effects/temporal_mode 2。
- 与引擎注册集（1180 命令，src 全量提取）比对：**全部已注册，零缺口**。r4_snow 族在 mod 侧另有功能守护（port 计划 Track D 结论维持）。
- 提示：比对初轮的"19 条全缺失"是工具链假象（CRLF `\r` 混入 + comm 的 locale 排序），已用 `tr -d '\r'` + `LC_ALL=C` 消除——复跑时勿再踩。

## 2. 实际运行版本（重要待办）

JSGME 当前唯一启用 mod = MT-CN；挂载发生在启用时刻，之后更新 `MODS\MT-CN\bin\` **不会**自动刷新 `bin\`。实测：`game\bin\AnomalyDX11AVX.exe` = `5ACDE694…`（HDR10 修复版），而载荷已是 `8A0171A9…`（zwork 批次一）。
**待用户操作：JSGME 里停用→重新启用 MT-CN**（换引擎版本后按部署说明清一次着色器缓存），zwork 批次才能上机验证。

## 3. PiP 3DSS 变体形态（MO2）

- Modpack 由 **ModOrganizer** 管理，激活 profile = `Anthology 2.1 Standart Стандарт`。
- Standart 的 modlist：`+PiP 3DSS`（行 202）与 `+普通 3DSS`（行 203）**同时启用**——两者 `models_scope_reticle.ps` 逐字节相同、仅 .s 绑定差一行（s_second_vp）。真机日志证明 PiP 语义生效（第二视口建集、lens=1、倍率正确），即 PiP 变体在合并中胜出；但双启用使结果依赖 MO2 行序语义，属**隐性脆弱点**。
  - 建议（可选）：Standart 里禁用普通 3DSS 变体，消除歧义。
- **HARD profile：只有普通 3DSS（PiP 变体禁用）**——若切 HARD 游玩，引擎按设计优雅降级（检测不到 PiP 模块→3DSS 路径，`scope_svp=0`），不是缺陷但需知情。
- PiP 着色器不在游戏散装 gamedata（经 MO2 虚拟层提供），运行时证据充分。

## 4. 其余核对

- 备份链：`_last_engine_backup.txt` 指向的目录及 checkpoint 所载三个备份目录均已不在磁盘（疑被清理腾空间）。现行可用回退：JSGME 停用（`MODS\!BACKUP` 已确认含原版 bin/appdata/db/gamedata）、git 历史、`MODS\MT-CN.rar`。风险可接受，但"known_good 快照"已无独立副本——如需可从 git 重建。
- translation_audit（273M）：db 解包脚本审计工作区（mod 契约真值来源之一），静态资料，无运行时影响。
- 游戏 `appdata\user.ltx` 与 payload 的 512/上采样键一致；P1-1 修复后的 alias 回写痕迹在案。
