#pragma once
// ---------------------------------------------------------------------------
// MOD-name compatibility layer (ANTHOLOGY modpack contract).
//
// The engine keeps its native/pipdlss cvar names (r_upscaler, r_dlss_quality,
// r__svp_*). The modpack's MCM pages and scripts, however, address the engine
// by the names below (see anthology_upscaler_mcm.script and
// rak_pip_quality_mcm.script). MCM resolves every declared "cmd" through the
// console; a missing cvar yields nil and clamp(nil, min, max) aborts the script
// engine ("_g.script:787 attempt to compare nil with number").
//
// Names, types and ranges here MUST match the MCM declarations verbatim.
// Storage is defined once in xrRender_console.cpp (compiled per renderer DLL),
// so only that TU owns the variables; other TUs read them via these externs.
// ---------------------------------------------------------------------------

namespace compat
{
	// --- upscaler (MOD name -> native r_upscaler family, wired in a later task)
	extern u32		r4_upscaler;				// token: off=0, fsr3=1, dlss=2
	extern u32		r4_upscaler_quality;		// token: native=0 .. ultra_performance=4, custom=5
	extern float	r4_upscaler_custom_scale;	// 0.33 .. 1.00
	extern float	r4_upscaler_sharpness;		// 0.0 .. 1.0

	// --- PiP scope lens (MOD name -> engine SecondVP / scope_lense_* contract)
	extern int		scope_lense_live_effects;	// 0/1
	extern int		scope_lense_temporal_mode;	// 0 legacy, 1 sparse, 2 live
	extern int		scope_lense_quality_percent; // 25 .. 100
	extern int		scope_lense_update_interval; // 1 .. 8
	extern float	scope_lense_aim_sensitivity; // 0.25 .. 2.0
	extern int		scope_lense_allow_nvg;		// 0/1
	extern int		scope_lense_allow_thermal;	// 0/1

	extern float	scope_lense_brightness;		// 0.5 .. 1.5
	extern float	scope_lense_contrast;		// 0.5 .. 1.5
	extern float	scope_lense_saturation;		// 0.0 .. 2.0
	extern float	scope_lense_gamma;			// 0.5 .. 1.5
	extern float	scope_lense_sharpness;		// 0.0 .. 2.0
	extern float	scope_lense_grain;			// 0.0 .. 1.0
	extern float	scope_lense_dirt;			// 0.0 .. 2.0
	extern int		scope_lense_tint;			// preset index 0 .. 4
	extern float	scope_lense_tint_intensity;	// 0.0 .. 1.0

	// engine-owned: 1 while the current ADS is a PiP (second viewport) ADS.
	extern int		scope_lense_ads_is_pip;

	// PiP (T3 #6) same-frame lens: 1 (default) renders the second viewport into the
	// second target set inside the main frame; 0 restores the legacy behaviour
	// (scope frame rendered to the swapchain, copied into $user$viewport2, Present
	// skipped). Kept as the switch to fall back to if the lens misbehaves.
	extern int		r__svp_same_frame;

	// --- flora/season style (modpack runtime seasons) and its snow options.
	// anthology_flora_style (bound in Blender_Recorder) is what the pack's terrain
	// and snow wallmark shaders read; the r4_snow_* values drive the pack's MCM
	// page and its snow interaction script.
	extern int		r__flora_style;				// 1..6 season style written by the calendar script
	extern int		r4_snow_enabled;
	extern float	r4_snow_height;
	extern float	r4_snow_distance;
	extern float	r4_snow_density;
	extern float	r4_snow_variation;
	extern float	r4_snow_drift_size;
	extern float	r4_snow_resistance;
	extern int		r4_snow_fall_enabled;
	extern float	r4_snow_flake_size;
	extern float	r4_snow_fall_speed;
	extern float	r4_snow_fall_density;
	extern float	r4_snow_fall_distance;
	extern float	r4_snow_wind;

	// --- engine-side settings that other builds of this engine expose. Registered
	// for MOD compatibility only and deliberately inert here: the modpack MCM and
	// its scripts address them by name, and a missing command makes
	// clamp(nil, min, max) abort the script engine. Ranges are wide so a value
	// already stored in user.ltx is never clamped away.
	// MT profiling page.
	extern int		mt_frame_profile;
	extern int		mt_frame_profile_detail;
	// load/generation accelerators.
	extern int		mt_load_spawn_decode;
	extern int		load_defer_full_lua_gc;
	extern int		load_world_warmup_ms;
	// Lua GC tuning page.
	extern int		lua_parallel_gc_adaptive;
	extern int		lua_parallel_gc_budget_us;
	extern int		lua_parallel_gc_frame_budget_us;
	extern int		lua_parallel_gc_pause;
	extern int		lua_parallel_gc_postload_delay_ms;
	extern float	lua_parallel_gc_stepmul;
	// PiP/optics page extras (the main scope_lense_* block is above).
	extern int		scope_lense_render_quality;
	extern int		scope_lense_quality_preset;
	extern int		scope_lense_frame_delay_add;
	extern int		scope_lense_head_nvg_active;
	extern int		scope_lense_head_thermal_active;
	// render/gameplay extras.
	extern int		r__menu_framelimit;
	extern float	r__npc_torch_dynamic_distance;
	// modpack bookkeeping keys.
	extern int		session_start;
	extern int		tasks_version;

	// Registers every cvar above. Idempotent; called from xrRender_initconsole().
	void RegisterConsoleVars();

	// Pushes the MOD names onto the native upscaler names (and back) when either
	// side changed. Called per frame by Tick() and once before the render targets
	// are created, because the upscaler method/quality is latched at target
	// creation (a vid_restart is needed afterwards to change it).
	void SyncUpscalerAliases();

	// Per-frame refresh of the engine-owned values (scope_lense_ads_is_pip).
	void Tick();
}
