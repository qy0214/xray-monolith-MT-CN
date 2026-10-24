# 2026-10-23

- 对 workbuddy/all-in-one-vs2022-wpo-mt-fceabbdc（98efc020，相对 60862562）做 NS7/OGSR 兼容移植的合并前审查：113 文件 +2398/-370，七维全量 diff 通读 + 逐条亲验（git show 新侧全文、调用链、API 签名）。
- 头号阻断：object_factory_register.cpp:389 的 LVL_CHNG 别名 ADD 与 386 行原 ADD 的 script_clsid 同名（"level_changer"），触发 add() 的 script_clsid 替换分支删掉 Anthology 的 LVLCHNGR 注册项（r_clsid=TEXT2CLSID 纯文本解析，Xr_ini.cpp:1796）→ Anthology level_changer 在 release 解析到错误工厂项。
- 次要点：调试 Msg 残留热路径（Weapon/WeaponMagazined/object_item_script/alife_monster_base）；dx10Texture 补丁只覆盖 _DDS 主路径；level.iterate_objects 下标越界隐患；level.block_action 仅接受 int（枚举漂移待验证）。
- 已验证为安全的点：xrDebugNew vsprintf->_vsnprintf（真溢出修复）、UPDATE_Read barrel 边界（SBarrel=u16）、spawn_ini/save_spawn_ini 缓存一致性、traffic_optimization init 早退与 deinit 对账、normalize_string resize(n) 依赖 C++11 operator[](size()) 写 NUL（实际无害但建议 resize(n+1)）。
- 审查结论：不建议按现状合并；修复 B1（+建议 B2）后再评估。

## 全量审查：xray-monolith-mt 基座库自有适配层（用户纠正目标后）
- 范围界定：上游分叉点 9dc5a5b5~1（"feat(render): Add native DLSS/FSR3 upscaling and modpack cvar compat" 之前）→ mainline HEAD 60862562，排除 sdk/（479 文件 vendored FidelityFX/NGX）与 dist/。实际审查面 92 文件 +6714/-341。
- 审查方式：全量 delta diff 通读（9510 行）+ 逐条亲验（新侧 git show 全文、API 签名、调用链、CI env 作用域）。
- 结论：无 P0。工程纪律显著高于 NS7 分支（内联 review 标签 P0-6/P2-8/P4、防御性 VRAM/格式检查、throttled 日志、host tests）。
- 头部问题：P1 SyncUpscalerAliases firstSync 把老用户 user.ltx 里的原生 r_upscaler 设置静默冲掉（xrRender_console.cpp:1544-1563）；P1 SetActive 镜头遍整体改写 Device.dwWidth/Height 全局（r4_rendertarget.cpp:481-560，device.h 注释自认 racy）；P1 scope_lense_live_effects 恒 0 静默降级（Blender_Recorder:1398）。
- 已证伪的疑点（防复发误报）：CI release_mt env 作用域没问题（workflow 级 env）；spawn_antifreeze 家族无双注册（旧 CMD4 已删）；u_setrt 深度替换无害（后处理 blender bZTest=FALSE）；SVP 集删除后共享名回指由 TargetMain->SetActive(true) 保证；HDR10 下同帧 PiP 整体拒建是 README 已声明的有意降级。
