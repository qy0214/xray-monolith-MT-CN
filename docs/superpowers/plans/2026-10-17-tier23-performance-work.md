# Tier 2/3 performance work — implementation plan

State when this was written: branch `all-in-one-vs2022-wpo-mt`, 26 local commits
(unpushed), deployed payload `MODS\MT-CN` exe `9182928B04AF`.
Engine is at parity with the pack's reference build on the same scene
(FPS 79-82 vs 83.6, render submit 4.2-5.3 ms vs 9.44 ms, identical geometry);
the remaining items below are secondary or startup-side work.

Verified facts to build on (do not re-derive):
- Device is created with `D3D11_CREATE_DEVICE_BGRA_SUPPORT` only — no
  `D3D11_CREATE_DEVICE_SINGLETHREADED`, so creating resources from worker
  threads is legal (`Layers/xrRenderDX10/dx10HW.cpp:557`).
- The engine already has MT task helpers: `secondary_tasks.wait()` /
  `thread_spawn` (`xrEngine/device.cpp:661`, `xrEngine/EngineThreading.cpp`).
- `SPAWN_ANTIFREEZE` exists in our source only as a race guard
  (`Layers/xrRender/Shader.cpp:51`), the throttle itself is not ported.
- The reference build logs `[STARTUP/RENDER TARGET] mode=parallel initial=1584
  overlap=137 shader-wait=0 total=1721 ms`; ours creates them serially.
- `mt_calc_bones 0` breaks the first-person weapon pose. Never test it off.
- optick is compiled in (`xrCore/profiler.h`), `Possibility`/capture trigger
  still missing.

## T3 #9a — parallel render-target creation

Goal: cut startup by ~1-1.5 s, no behaviour change.

Approach (low risk, no ctor restructure):
1. In `r4_rendertarget.cpp`, the ctor's first big block (lines ~606-760, the
   render-sized scene RTs) is pure creation with no render-state interleaving.
   Wrap exactly those `create(...)` calls into lambdas, push them into a small
   vector and run them through the existing MT task API with 2-4 workers.
2. Do NOT defer anything outside that block: later creations are interleaved
   with state calls (`u_setrt`, `ClearRenderTargetView` on `t_LUM_pool`, shader
   and geometry creation) and reordering them changes behaviour.
3. Keep a serial fallback: if task submission fails, run the lambdas inline.

Verification: startup line in our own log (add a `[STARTUP/RENDER TARGET]` line
mirroring the reference), no visual change, all three DLSS quality tiers still
activate, `compat::SyncUpscalerAliases()` still runs before the targets exist.

## T3 #8① — `load_defer_full_lua_gc`

Goal: make the registered-but-inert cvar functional; defer the full Lua GC that
runs at the end of level loading.

Site to find: the post-load `collectgarbage("collect")`. `Layers/xrRender` has
none; it is either in `xrGame` (`script_engine*`, `ui/UIWindow.cpp:209/291` are
window-scoped, not the loader) or driven from the modpack's Lua. Gate the
engine-side call with `compat::load_defer_full_lua_gc` (default 1 = current
behaviour), and if the call turns out to be script-side, expose the value to Lua
instead of guessing.

Verification: load time and post-load memory in the log, no leaked script state
(the deferred GC must still run, just later).

## T3 #8② — `spawn_antifreeze_max_per_frame`

Goal: port the spawn throttle the reference build has (the cvar is registered but
inert here). The concept: cap the amount of spawn/object-activation work per
frame so the load burst is spread over frames instead of one long hitch.

Work: find the spawn/activation path (xrGame level spawn / `alife` object
activation; `SPAWN_ANTIFREEZE` guards already show where the renderer races with
it), add the per-frame counter and the cap, default 8, and keep the current
behaviour when the value is 0.

Verification: the load/spawn hitch profile (frame-time spikes), no objects lost
(spawned count must match a run with the cap disabled), and the modpack's own
spawn-throttle modules still behave.

## T3 #6 — same-frame PiP (second render-target set)

Goal: lens content rendered in the same frame instead of a frame-delayed copy.
This removes the 15-20 fps cost of the scope resolution override (Tier 1, Plan A)
and the root cause of the lens ghosting, replacing the "no history while aiming"
mitigation.

Reference implementation on this machine:
`C:\game\xray-monolith-pipdlss\src\Layers\xrRenderPC_R4\` —
`svp_target.{h,cpp}`, `svp_camera.{h,cpp}`, `svp_dsgraph_cull.cpp`,
`svp_hud_latch.cpp`, `SvpDlss.h`, plus `TargetSVP` usage in `r4_R_render.cpp`.
Also `xrEngine/device.{h,cpp}` for `CSecondVPParams`.

Stages (each one committable, none required to land the next):
0. **RESOLVED (2026-10-17): the second set must register under distinct names,
   and switching re-points the shared `$user$*` textures.** Evidence is from our
   own code, not from the reference:
   - `CResourceManager::_CreateRT()` (`Layers/xrRender/ResourceManager_Resources.cpp:363`)
     and `_CreateTexture()` (`:458`) are **name-keyed dedup factories**: they look
     the name up in `m_rtargets` / `m_textures` and *return the existing object*.
     So a second `CRenderTarget` built with the same literal names does not even
     create new surfaces — its `ref_rt` members silently alias the main set's CRTs.
     (`CRT::create()` also early-outs on `if (pSurface) return;`, `SH_RT.cpp:27`.)
     Prefixing is therefore **mandatory, not a style choice**.
   - Apply the prefix in the **lens-set creation code**, never in a global hook:
     `_CreateRT`/`_CreateTexture` are shared with every runtime texture load, so a
     "current prefix" global would be a trap. Convention: the main name stays the
     key and the lens name is derived from it, `$user$X` ↔ `$user$svp_X`, with one
     helper doing both directions — no hand-written pair table.
   - Switching is then exactly what stage 2 describes, and there is a working
     precedent in this codebase: `bus_texture_slot` / `bus_textures_update()`
     (`Layers/xrRender/dxRenderDeviceRender.cpp:8-73`) already aliases one texture
     name onto another texture's surface via `surface_set` whenever the target
     moves. The blender-bound name (`$user$position`, …) never changes, only the
     surface behind it, so every name-binding shader follows the active set.
   - Remember `HW.pBaseZB` / `u_SceneDepth()` are *not* part of the `$user$*`
     family (see the `u_setrt()` hazard in the session checkpoint).
   - Lens-set size = display extent (or optic extent); `rt_secondVP`/`rt_ui_pda`
     are already display-sized, so they are the natural first members.
1. `CRenderTarget(bool mainSet)` — second instance sized to the display (or the
   optic extent), created lazily on the first magnifying ADS, kept for the
   session, **using the stage-0 `$user$svp*` naming helper**. Nothing switches yet;
   verify the main path is untouched (compare `rs_stats`, the PDA/scope behaviour
   and the shader cache hit rate).

## Stage 1 — DONE (2026-10-17): target-set infrastructure, nothing switches yet

Ported from the reference's shape, adapted to this engine:
- `CRenderTarget(LPCSTR set_name, u32 width, u32 height)` (set_name `nullptr` = the
  main viewport; the old default ctor forwards to it) with `Width`/`Height`,
  `m_is_main_target`, `baseRT`/`baseZB`, `rt_baseRT`/`rt_baseZB`.
- **All 87 RT creations in the ctor now go through a `createUnique` lambda**: the
  main set keeps the stock name verbatim, a second set registers
  `"<stock name>$<set_name>"`, and every call records (stock-named `ref_texture` ->
  this set's `ref_rt`) in `RenderTargetRemaps`.
- `SetActive(bool force)`: main restores `HW.secret_pBaseRT/ZB`; sets
  `RImplementation.Target`, `hud_params.w`, `m_render_pass_is_svp`,
  `Device.dw*`/`fASPECT`, `RCache.Invalidate()`, then `surface_set`s every remap
  entry, `set_viewport_size`, `set_RT(baseRT)`/`set_ZB(baseZB)`.
- `HW.secret_pBaseRT`/`secret_pBaseZB` are recorded where the swapchain views are
  created (and nulled where they are released).
- The upscaler context stays main-only: the second set does **not** call
  `dlss::InitializeTargets()`/`ReleaseTargets()` (it is created/destroyed
  mid-frame), and `rt_upscaler_camera_mv` is main-only so it is never remapped.
- Nothing calls `SetActive` on the second set yet, and no second set is created
  yet, so behaviour is unchanged by construction.

## Why the lens lags today (drives stages 3-4)

This engine already renders the second view **in-frame**: `r4_R_render.cpp:514`
targets `Target->rt_secondVP` inside `Render()`. The one-frame delay (the ghosting
root cause) comes from `dxRenderDeviceRender::End()`:

```cpp
if (!Device.m_SecondViewport.IsSVPFrame() && !Device.m_SecondViewport.isCamReady)
    HW.m_pSwapChain->Present(...);   // the SVP frame is rendered but NOT presented
```

so the SVP content is only consumed by the *next* (main) frame. Same-frame PiP
therefore means: render the second view into the second target set, restore the
main set, render the main view, and present once — i.e. `IsSVPFrame()` must stop
being a whole-frame property.

## Stage 2 — DONE (2026-10-17): the second set is creatable, still nothing switches

- `CRender::TargetMain` / `TargetSVP` (`r4.h`), `TargetMain = Target` on create/reset,
  `TargetSVP = nullptr` on create/reset_begin, both dropped in `destroy()`.
- `CRender::EnsureTargetSVP()` (in `r4.cpp`): creates `CRenderTarget("svp", side, side)`
  lazily, `side` = `dwHeight/2` rounded up to a 32px step (never below 64), rebuilds
  when the extent changes (falling back to the main set first). Nothing calls it yet,
  so the extra RT chain does not exist during normal play.
- `SetActive` is no longer wrapped in `#ifdef USE_DX11`: only the swapchain-view
  restore and the `HW.secret_*` members are DX11-only, and the `HW.pBaseRT/ZB`
  assignment is now null-guarded (the DX10 build compiles unchanged).

## How the second view is produced today (measured, drives stage 3-4)

- `Weapon.cpp:3368` publishes the state: `SetSVPActive(zoomed && first-eye &&
  IsSecondVPZoomPresent() && rotation > 0.05)` ⇒ this is what `IsSVPActive()` means
  (our Plan A scope-resolution override keys off it as well).
- `CLevel::OnRender()` renders the HUD and, **on an SVP frame**, runs
  `Render->RenderToTarget(Render->rtSVP)` (`Level.cpp:1315-1316`), which is a plain
  **`CopyResource` of swapchain buffer 0** into `$user$viewport2`
  (`r4_R_render.cpp:504-525`). In other words: on those frames the *whole* frame was
  rendered with the zoomed camera, copied out, and `Present` was skipped — the lens
  then samples a one-frame-old image. That is both the ghosting and why the scope
  resolution override has to raise the *whole* frame's internal resolution.
- So same-frame PiP = render the normal view **and** a small second view every frame,
  present the normal one, and let the lens sample the second set. The two cameras are
  not both available in this engine (no `Device.matrices[2]` like pipdlss): the game
  alternates the camera per frame, so stage 3-4 snapshots both cameras
  (`mView`/`mProject`/`mProjectHud`/`hud_fov_params`) instead of re-deriving the
  optics zoom, and keeps `IsSVPFrame()` only as the "the zoomed camera was set this
  frame" trigger (no more `Present` skip, no more `CopyResource`).

## Stages 3, 4a, 5 — DONE (2026-10-17)

**Stage 3 — per-pass branches.** `IsSVPFrame()` is the frame-level alternation
(`IsSVPActive() && dwFrame % frameDelay == 0`, `device.cpp:1001`) and used to decide
whether the *whole frame* draws the scope view. The render/shader-side users now read
`m_render_pass_is_svp` instead (13 sites): `Blender_Recorder_StandartBinding` (the
`$user$` shader constant), `r4_R_render` (ssfx/sss skips), `phase_combine` (6),
`accum_spot`, `accum_point`, and the upscaler's `mainView` test. Behaviour-identical
because the per-frame refresh (now guarded to “lens off”) kept the flag in sync.

**Stage 4a — the scope frame renders into the second set** (`r4_R_render.cpp`):
on a scope frame `EnsureTargetSVP()` + `TargetSVP->SetActive()` are taken *before* the
scene, so the frame lands in the small set instead of the swapchain, and
`RenderScreenWidth()` (`DlssIntegration.cpp:76`, `SvpPassBound()`) follows the set's
extent. Right after `Details->details_clear()` the main set is restored
(`TargetMain->SetActive(true)`) so the HUD, post-process and UI keep the main targets.
`$user$viewport2` is now excluded from the remap restore in `SetActive` (both call
sites), i.e. the lens permanently samples the second set, and
`CRender::RenderToTarget(rtSVP)` is a no-op (no more `CopyResource`) while `rtPDA`
always resolves against `TargetMain`. The per-frame flag refresh no longer clobbers
the per-pass value.

**Stage 5 — Plan A retired while the set is in charge**: `ScopeResolutionRequested()`
returns false when `TargetSVP` exists and the lens is active (the set already renders
the lens at its own extent), so the 15-20 fps override no longer fires; it remains as
the fallback when the set could not be created (VRAM) and for pure screen-space
magnification. `EnsureTargetSVP()` is called early in `Render()` so the decision sees
the set.

**Still open — stage 4b (same frame)**: both views in one frame so the lens is not
one frame old. Needs the scene body extracted so it can run twice per frame (it must
re-`traverse`/`capture` per view because the frustums differ), the two cameras
snapshotted (`mView`/`mProject`/`mProjectHud`/`hud_fov_params`; this engine has no
`Device.matrices[2]`), and then the removal of the `Present` skip in
`dxRenderDeviceRender::End()` plus the game-side frame alternation
(`Actor.cpp:1237` hides the HUD on SVP frames). The narrow zoom frustum culls the
lens pass on its own (`ViewBase` is built from `Device.mFullTransform`), so the second
pass is cheap.

## Remaining stages (revised, in order)

2. `EnsureTargetSVP()` + `TargetSVP`/`TargetMain` in `CRender` (lazy, on the first
   magnifying ADS; extent = even-rounded `Device.dwHeight/2` to start, no
   supersample/adaptive policy yet). Still nothing switches.
3. Convert the per-frame `Device.m_SecondViewport.IsSVPFrame()` render/shader
   branches (`r4_R_render.cpp` 330/338, `r4_rendertarget_phase_combine.cpp`
   94/110/338/346/540/549, `accum_spot/accum_point`) to the per-pass
   `m_render_pass_is_svp` flag, so the SVP *pass* keeps its branches while the
   frame as a whole is a normal, presentable frame.
4. Render both passes in one `Render()` call: `TargetSVP->SetActive()` → second
   view (reusing the existing SVP camera path) → `TargetMain->SetActive()` → main
   view; drop the `Present` skip. The lens keeps sampling `$user$viewport2`, so
   that one name must stay pointed at the **SVP set's** `rt_secondVP` (exclude it
   from the main set's remap restore) — this also lets the second set be smaller
   than the display, which is where the 15-20 fps comes back.
5. Skip the temporal upscaler while the second set is the active pass (its extent
   is already the final extent); keep `r__upscaler_scope_lock` as the fallback and
   retire Plan A's rebuild path.
2. Re-point the registered `$user$*` names to the other set's surfaces on switch
   (`Resources->_CreateTexture(name)->surface_set(other->pSurface)`), so all
   blenders that bind by name follow the active set.
3. Switch `RImplementation.Target` + `RenderScreenWidth/Height()` +
   `u_SceneDepth()` to the active set, and skip the temporal upscaler while the
   lens set is active (render extent == display extent there).
4. Render the second view same-frame (no `SetSVPFrameDelay`) and draw the lens in
   the main frame. Feed it jitter/MVs like the main view, or accept a spatial
   upscale for the lens.
5. Retire the scope-resolution override's rebuild path (or keep it as fallback
   behind `r__upscaler_scope_lock`).

Risks: registry re-pointing must cover every RT name the shaders use; the DSgraph
must not cache target pointers; the two sets double VRAM while both exist.
Acceptance: with a magnifying optic, the lens shows the current frame (pan hard
and check for ghosting), and carrying the optic no longer costs the 15-20 fps.

## Session handoff checklist

- [ ] Tier 1 A/B results from the player (FPS + `uSheduler_L` + the new
      `queue:` line for `scheduler_batch_size`, `mt_schedulerRT`,
      `lua_use_functor_cache`, `mt_script_gc`, `mt_alife`); then fix whatever the
      data points at instead of guessing.
- [x] Optick capture trigger: `optick_start` / `optick_stop [file]` /
      `optick_autostart 1` + `PROF_FRAME("Main")` in `FrameMove()`.
- [ ] T3 #9a, then #8①, #8②, then #6 in the order above.
- [ ] Push the 26 local commits only when the player explicitly approves.
