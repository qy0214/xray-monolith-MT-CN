MT-CN 部署 / 撤回 说明（2026-10-28 起：gamedata 单一来源在 MO2）
================================================================

一、两个位置现在各管什么
1. C:\game\ANTHOLOGY\Modpack-1.5.3-Anthology 2.1\mods\MT-CN\gamedata\
   —— 唯一的脚本 / 文本 / 着色器来源，由 MO2 挂载（profile「Anthology 2.1 Standart Стандарт」
   modlist 第 2 行）。以后改 Lua、改 ui 文本、改着色器都只改这一份，改完确认 MO2 里 MT-CN 是勾上的。
   现有 30 个文件：10 scripts + 2 configs\text + 18 shaders\r3。
2. C:\game\ANTHOLOGY\Anomaly-1.5.3-Anthology 2.1\MODS\MT-CN\
   —— 只留必须物理存在的东西：bin\AnomalyDX11AVX.exe、bin\nvngx_dlss.dll、
   db\mods\00_modded_exes_gamedata.db0，加本说明目录和 清空着色器缓存.bat。
   exe 是进程映像、db0 是归档注册文件，这两类注入不进去，必须真实拷到游戏目录。

二、部署（换机 / 重装）
1. MO2 profile 切到 Standart Стандарт（MT-CN 只存在于这个 profile 的 modlist，HARD / 本机里没有）。
2. 把上面第 2 条的三个文件拷到游戏目录的同名位置（JSGME 或直接复制都行；
   MODS\MT-CN 里已经没有 gamedata，所以不可能再覆盖脚本/配置）。
3. 双击 游戏目录\清空着色器缓存.bat（换 exe 或换着色器都必须清）。
4. 启动游戏。日志第一屏应出现：
   [MT-CN] engine build <日期> pipBuild=10 ...        ← exe 是 MT 版
   [MT-CN] d_promin_health_ui override active ...      ← MO2 挂载生效

三、撤回
- 只撤脚本/文本/着色器：MO2 里取消 MT-CN 勾选即可（动过着色器就再清一次缓存）。
- 撤引擎：还原原版 bin\AnomalyDX11AVX.exe 与原版 bin\nvngx_dlss.dll、删掉
  db\mods\00_modded_exes_gamedata.db0，然后清缓存。

四、2026-10-28 从 mod 里移出的两个文件（原因）
- gamedata\configs\axr_options.ltx：mod 里那份是 10-26 11:02 的旧快照。挂载层读取优先于根目录
  （证据：只在 mod 里存在的 MT-CN 脚本能加载），留着会把你在游戏里改过的选项在下次启动退回旧值
  ——根目录那份是游戏自己写的活文件（现 10-28 18:25），里面 MT 相关值与旧快照逐条一致，移出不丢设置。
- appdata\user.ltx：同理，10-26 11:32 的旧快照，一旦物理覆盖就吃掉当前配置。
  现值在 游戏目录\appdata\user.ltx：scheduler_batch_size=512（生效）、
  lua_use_functor_cache=0、mt_task_manager=0（这两项目前是关的）。
- 备份：C:\game\xray-monolith-mt\_mtcn_offline_20261028\
  （A_gamedata\ 整份、axr_options.MOD261026.ltx、user.MOD261132.ltx、A_appdata_user.ltx）

五、注意
- 游戏根目录 gamedata\shaders\r3\ 里现有 18 个着色器与 mod 里那 18 个字节完全相同（早先物理拷入的）。
  重复但同内容，不影响结果；要彻底单一来源可以删根目录那 18 个，删前请先实机确认一次。
- 校验清单 MODS\MT-CN.sha256.txt 还是旧结构生成的，需要重新生成才对得上（未经同意不动）。

六、必须经 MO2 启动（2026-10-28 22:14 之后成为硬要求）
----------------------------------------------------
游戏根 gamedata\ 里那批历史遗留的散落覆盖（18 个着色器等）已在一次 JSGME「停用 → 启用」循环中被清除，
MODS\MT-CN 自 19:40 起也不再携带 gamedata，因此根目录已经没有兜底副本：绕过 MO2 直接双击 exe 启动，
DLSS/FSR3 的着色器契约、SSFX 覆盖、穿戴设备与快旅等脚本覆盖全部不会生效。
启动请用 MO2（profile「Anthology 2.1 Standart Стандарт」，MT-CN 在 modlist 第 2 行、已勾选）。
JSGME 只负责 bin\、db\mods\00_modded_exes_gamedata.db0、本说明目录与清缓存脚本；
每次在 JSGME 里重新启用，会把 MODS\MT-CN\MT-CN_说明 复制一份到游戏根（那份是副本，改这里）。
