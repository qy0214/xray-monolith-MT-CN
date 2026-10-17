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
  (exe `8E9FA2238E19` — that is the first 12 hex chars of the **deployed exe's
  SHA256**, so it changes on every rebuild and is the quickest way to confirm
  what is actually deployed; nvngx_dlss.dll 310.9.1, 16 shader overrides,
  `清空着色器缓存.bat`, `MT-CN_说明.txt`, `MT-CN_说明\README_部署与撤回.txt`.
  There is **no** `MT-CN.sha256.txt` in the payload — that line was wrong).
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
- Optick is now usable, not just compiled in:
  - `PROF_FRAME("Main")` is the first statement of `CRenderDevice::FrameMove()`
    — frame markers did not exist before, so a capture had no frames at all.
  - console: `optick_start`, `optick_stop [file]` (default `optick_capture.opt`
    in the working directory), `optick_autostart 0|1` (starts the capture at the
    top of `CRenderDevice::Run()`, so it covers startup + level load; the file is
    written on exit). State/helpers live in `xrEngine/device.cpp`, the console
    entries in `xrEngine/xr_ioc_cmd.cpp`.
  - Viewer: `src\3rd party\optick-git\gui\OptickApp_vs2022.sln` (or the Optick
    release build); there is no GUI in the payload.

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

1. **T3 #6 same-frame PiP (second target set)** — **implemented end to end
   (stages 0-5), awaiting the player's in-game test.** See the T3 #6 section of
   `docs/superpowers/plans/2026-10-17-tier23-performance-work.md` for the design,
   the evidence, the new `r__svp_same_frame` switch (0 = full legacy fallback) and
   the verification checklist. In short: the lens and the main view are drawn in one
   frame from two camera snapshots, the lens renders into a small second target set
   (`$svp`-suffixed names, remapped onto the stock `$user$*` textures by
   `SetActive`), the swapchain copy and the skipped Present are gone, and the
   scope-resolution override no longer fires while that set handles the lens.
2. ~~Optick capture trigger (console command) so the profiler is usable.~~ **Done**
   (see the diagnostics section; needs one in-game run to be exercised).
3. Tier-1 A/B data from the player (FPS + `uSheduler_L` + `queue:`) → then fix
   whatever the data points at instead of guessing.
5. Verify the FDDA exo screen: health tint + clock/hands; adjust the encoding in
   `xrGame/Actor.cpp` if the digits are wrong.
6. `lua_parallel_gc_*` / load accelerators: only worth implementing if the user
   needs the pack's own perf modules to actually work (registered-but-inert today).

## Outstanding user actions

- Enable `[汉化] MT-CN 选项汉化` in JSGME/MO2 (DLSS options become Chinese).
- Confirm the aim pose is fixed (`mt_calc_bones 1`), the plain log survives, and
  `spawn_antifreeze_max_per_frame 8` vs `0` only changes smoothness, not content.
- Verify the deployed exe is the new one: the payload `bin\AnomalyDX11AVX.exe`
  must hash to `8E9FA2238E19…` (SHA256), then play a minute — the `optick_*`
  commands must appear in the console (Tab-completion) and
  `optick_start` → `optick_stop` must log
  `* [OPTICK] capture started` / `capture saved to 'optick_capture.opt'`.
- **Test the same-frame lens** (T3 #6, the headline change): aim through a magnifying
  optic, pan hard and confirm the lens image no longer trails the main view; `rs_stats
  on` must show `RENDER:` not doubling and the FPS back at the no-optic level. If the
  lens misbehaves, `r__svp_same_frame 0` restores the previous behaviour exactly.
  Full checklist: the "Verification checklist for the in-game test" section of the
  tier 2/3 plan.
- Decide when to push the local commits (38 as of this checkpoint).
