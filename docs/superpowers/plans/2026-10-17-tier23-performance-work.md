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
