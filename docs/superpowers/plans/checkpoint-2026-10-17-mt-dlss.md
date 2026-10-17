# Context checkpoint — MT engine + DLSS/FSR3 + modpack compatibility (2026-10-17)

Read this first in a fresh session; it replaces the previous conversation.

## Objective and rules

- Fork of `themrdemonized/xray-monolith`, MT branch. All work syncs only to the
  user's fork `qy0214/xray-monolith-MT-CN` (branch `all-in-one-vs2022-wpo-mt`).
  Upstream must never be touched. **30 local commits are unpushed; push only when
  the user explicitly approves.**
- Engine must stay drop-in compatible with the ANTHOLOGY 2.1 modpack:
  "compatibility with the mods, not parity with a reference engine". Do not modify
  the modpack. Do not change the display resolution. Everything changed/compiled
  must live in `MODS\MT-CN` so deploy/rollback is one operation.
- After every change: build, deploy, tell the user exactly what to verify.

## Paths

- Engine: `C:\game\xray-monolith-mt` (clean worktree, 30 local commits).
- Payload: `C:\game\ANTHOLOGY\Anomaly-1.5.3-Anthology 2.1\MODS\MT-CN`
  (exe `FD69CB2FE928`, nvngx_dlss.dll 310.9.1, 16 shader overrides,
  `MT-CN.sha256.txt` manifest, `清空着色器缓存.bat`, `MT-CN_说明.txt`).
- Chinese localisation mod (needs enabling in JSGME/MO2):
  `MODS\[汉化] MT-CN 选项汉化` — 24 DLSS/upscaler strings, UTF-8 **no BOM**;
  mirror copy in `Modpack-1.5.3-Anthology 2.1\mods\[汉化] 汉化整合包…`.
- Rollback: `C:\game\ANTHOLOGY\_engine_backup_20261017_1403_pre_scope_res`
  + git branch/tag `backup/pre-scope-res-20261017_1403`.
- Logs: `appdata\logs\xray_admin.log` (always present) + per-session
  `xray_admin_<YYYY-MM-DD_HH-MM-SS>.log` archives.
- Reference engines: `C:\game\xray-monolith-pipdlss` (DLSS+PiP reference, local),
  `C:\game\xray-monolith-all-in-one-vs2022-wpo` (non-MT base).

## Build / deploy / verify

- Build: `msbuild /p:Configuration=DX11-AVX src\engine-vs2022.sln`
  (solution-level configuration names only; `Release-AVX` silently builds DX9).
  Output `_build\_game\bin_dbg\AnomalyDX11AVX.exe`.
- Deploy: copy the exe into `MODS\MT-CN\bin\`, regenerate the manifest, tell the
  user to re-enable MT-CN in JSGME/MO2 (shader cache only if shaders changed).
- In game: `rs_stats on` shows FPS/RFPS, `uSheduler`/`uSheduler_L`, the new
  `queue: Q[…] batch[…] done[…] target[…] stop[…]` line, `RENDER:` section
  timings (traverse/gbuffer/lights/combine + hud/pp inside) and `UPSCALER:
  scene WxH -> WxH [SCOPE n%]`.

## Done (all committed)

Rendering / DLSS:
- Full DLSS + FSR3 port (`Layers/xrRenderPC_R4/Dlss/*`, RT family, `u_SceneDepth`,
  jitter/MV/reprojection, `r_upscaler`/`r_dlss_*`/`r_fsr3_*`).
- Screen-space overlays (gas mask, night vision, heat vision, fake scope) moved
  back to `phase_combine`, i.e. **before** the upscaler (fixes the top-left
  squeezed overlay and the magnified pixels).
- Native UI at display resolution: PDA/glass meshes extracted from the scene,
  item + camera UI widgets drawn after reconstruction; the depth reprojection now
  only runs when deferred geometry actually exists.
- `rt_ui_pda` and `rt_secondVP` created at display resolution (they are filled by
  a backbuffer copy; a render-extent size made that copy mismatched — black PDA).
- No-history/no-jitter "stable" state while a device view is up, while a second
  viewport optic is active, or while aiming through a magnifying optic (kills the
  thermal/PiP ghosting; it was DLSS-only by the user's own test).
- Scope resolution override (Plan A): raising the internal resolution while a
  magnifying optic is equipped, default `r__upscaler_scope_scale 75`, with delay,
  rebuild via `reset_begin`/`reset_end`. Costs ~15-20 fps, GPU-side, by design.
- Mip bias now `2*log2(scale)` (matches the reference build's auto -2.0 at 0.5).

Compatibility:
- `compat` layer (`Layers/xrRender/UpscalerCompat.h` + `xrRender_console.cpp`):
  `r4_upscaler*`, all `scope_lense_*`, season/snow (`r__flora_style`, 13
  `r4_snow_*`), plus 21 engine cvars other builds expose
  (`mt_frame_profile*`, `lua_parallel_gc_*`, `mt_load_spawn_decode`, `load_*`,
  `scope_lense_render_quality`/`quality_preset`/`frame_delay_add`/`head_*`,
  `r__menu_framelimit`, `r__npc_torch_dynamic_distance`, `session_start`,
  `tasks_version`) — registered, inert, wide ranges (never clamp a stored value).
- FDDA device-screen contract: `m_actor_params` (= actor condition, red→green
  tint), `m_affects` (= 0, clean screen), `m_digiclock`/`m_timearrow`/
  `m_timearrow2` (in-game clock/hand phases; **encoding inferred, verify on the
  exo screen and adjust in `xrGame/Actor.cpp` if wrong**).
- `r__menu_framelimit` implemented (main-menu branch only; 0 = off, default).
- `spawn_antifreeze_max_per_frame` implemented: `ProcessGameSpawns()` consumes at
  most N per frame, leftovers stay in the engine's own `game_spawn_queue`
  (default 8; 0 = previous behaviour; direct `g_sv_Spawn` paths unaffected).
- Optick compiled in (`xrCore/profiler.h`); capture trigger still missing.

Diagnostics / infra:
- Per-section CPU timings in `rs_stats` (no log noise, no GPU work).
- `[STARTUP/RENDER TARGET]` / `[RESET/RENDER TARGET]` timing lines.
- Log archiving per session while keeping `xray_admin.log`.

## Measurements that drive decisions (do not re-derive)

- Reference build on the same scene: FPS 83.6, render 9.44 ms, VERT 9.43 M,
  POLY 5.87 M, DIP 2863.
- Ours with `scope=0`: FPS 79-82, render 4.2-5.3 ms, VERT 9.02 M, POLY 5.94 M,
  DIP 2878, RFPS 187-232. **Parity reached; the render path is twice as fast.**
- `scope=75` costs ~15-20 fps (GPU-only; CPU submit unchanged).
- Geometry is the real ceiling: ~9 M verts/frame, ~85% flora ⇒ DLSS gives only
  ~1.5x. Further fps needs fewer verts (game setting) or fewer fixed costs.
- Render target creation: ours **serial 1042 ms** vs reference *parallel* 1721 ms
  ⇒ T3 #9a (parallel target creation) is **not worth doing**. RESET path 264-278 ms.
- `mt_task_manager 0/1` makes no difference (it only switches the Level/HUD update
  strategy, it is not the MT master switch).
- The pack's own perf modules (`lua_parallel_gc_*`, `load_*`, `mt_load_spawn_decode`)
  are registered but **inert** here; their MCM pages say "enabled" and do nothing.

## Hazards learned (do not repeat)

- `mt_calc_bones 0` breaks the first-person weapon/ADS pose. Never test it off.
- `mt_ui`, `mt_schedulerRT`, `lua_use_functor_cache` are risky to leave changed;
  all Tier-1 A/B items must be restored to defaults afterwards.
- **Texture name registry**: creating a second `CRenderTarget` with the same
  literal names replaces the main set's surfaces even without switching. Naming
  must be solved *before* any second target set exists (see the T3 #6 plan).
- `u_setrt()` substitutes the upscaler scene depth for `HW.pBaseZB`: bind
  `RCache.set_ZB(HW.pBaseZB)` explicitly when a display-sized depth is required.
- PowerShell treats `[` in a path as a wildcard: use `-LiteralPath` for
  `MODS\[汉化] …`.
- `InitializeTargets()` warns if called mid-frame; call reconfigurations at the
  top of `CRender::Render()` while `inScene == false`.

## Backlog (in order)

1. **T3 #6 same-frame PiP (second target set)** — the only remaining development
   task. Read `docs/superpowers/plans/2026-10-17-tier23-performance-work.md`, its
   T3 #6 section now starts at stage 0 (naming) because of the registry hazard.
   Reference implementation: `C:\game\xray-monolith-pipdlss\...\svp_*`, `SvpDlss.h`,
   `TargetSVP`. Goal: lens rendered in the same frame (no ghosting from the
   2-frame-delayed SecondVP) and no 15-20 fps cost while an optic is equipped.
2. Optick capture trigger (console command) so the profiler is usable.
3. If the user ever wants the pack's stale options page UI in the current engine's
   modelThe modpack build has no source here). (empty line placeholder)
4. Tier-1 A/B data from the player (FPS + `uSheduler_L` + `queue:`) → then fix
   whatever the data points at instead of guessing.
5. Verify the FDDA exo screen: health tint + clock/hands; adjust the encoding in
   `xrGame/Actor.cpp` if the digits are wrong.
6. `lua_parallel_gc_*` / load accelerators: only worth implementing if the user
   needs the pack's own perf modules to actually work (registered-but-inert today).

## Outstanding user actions

- Enable `[汉化] MT-CN 选项汉化` in JSGME/MO2 (DLSS options become Chinese).
- Confirm the aim pose is fixed (`mt_calc_bones 1`), the plain log survives, and
  `spawn_antifreeze_max_per_frame 8` vs `0` only changes smoothness, not content.
- Decide when to push the 30 local commits.
