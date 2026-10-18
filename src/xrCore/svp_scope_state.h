#pragma once
// ---------------------------------------------------------------------------
// Single source of truth for the second-viewport (PiP scope) state.
//
// Before this type the same decision was spelled out as six booleans spread over
// xrGame (Weapon / Actor / ActorInput), xrEngine (CSecondVPParams) and the R4
// renderer (r4_R_render / r4_rendertarget / Dlss). Every new rule had to be
// repeated in all of them, which is how the lens pass, the wide main view and the
// mouse sensitivity drifted apart - and how a frame without a rendered lens could
// still widen the view and drop the zoom from the mouse.
//
// Keep the rules here, pure and hostable in tests. The subsystems only consume the
// resulting SvpScopeState.
// ---------------------------------------------------------------------------

struct SvpScopeInputs
{
	bool pack_pip_module = false;     // the PiP module (policy file + lens shaders) is installed
	bool optic_magnifies = false;     // the active optic actually magnifies
	bool optic_policy_allows = false; // the optic resolves a lens FOV (child/parent scope section)
	bool target_set_ready = false;    // the renderer built and owns the second target set
	// The optic's image is COMPUTED, not optical (the pack declares it with s3ds_image_type
	// >= 2 or scope_lense_thermal). Such an optic must keep the 3DSS path; see svp_evaluate_scope.
	bool optic_thermal_image = false;
};

struct SvpScopeState
{
	bool lens_rendered = false;        // this frame draws the lens into the second target set
	bool main_view_wide = false;       // the main view keeps g_fov (the zoom lives in the lens)
	bool aim_sensitivity_lens = false; // first eye sensitivity follows the lens FOV
};

// The rules, in one place. Everything the subsystems need is derived here.
inline SvpScopeState svp_evaluate_scope(const SvpScopeInputs& in)
{
	SvpScopeState out;
	// A thermal optic never takes the second viewport, and this is measured, not assumed. The
	// pack's own lens shader treats that optic as "the engine will hand me a hot image": in its
	// scope_svp > 0 branch it skips its grading and its infrared() path entirely, and just shows
	// the second view. The only thermal image this engine can put in the second view is
	// phase_heatvision(), which is a DEVICE effect: compute_lens_mask() in the bundled heat
	// vision shader accepts a 1/2/4 tube layout and returns 0 everywhere else, and with no worn
	// device the tube count is 0, so the pass writes a black frame (that is the black lens and
	// the black screen a forced r__heatvision 1 produces). The pack's working thermal model is
	// the one its own shader runs on the 3DSS path - infrared() over the G-buffer plus the heat
	// map LUT and pixelation - and that path is exactly what an optic takes when the engine
	// does NOT provide the second view. So: for these optics the honest answer is no lens, and
	// the player gets the thermal sight the pack authored, with the plain ADS zoom the 3DSS
	// setup already gives them.
	out.lens_rendered = in.pack_pip_module && in.optic_magnifies && in.optic_policy_allows
		&& in.target_set_ready && !in.optic_thermal_image;
	// Never widen the view or change the mouse feel unless the lens is really drawn:
	// without a lens there is nothing to own the zoom, and the player would lose both
	// the magnification and the scoped mouse ratio.
	out.main_view_wide = out.lens_rendered;
	out.aim_sensitivity_lens = out.lens_rendered;
	return out;
}

// Builds the inputs from the facts the subsystems hold. The kill switch is folded in
// here so no consumer has to remember it.
inline SvpScopeInputs svp_make_inputs(bool pack_pip_module, bool same_frame_enabled, bool optic_magnifies,
                                      bool optic_policy_allows, bool target_set_ready,
                                      bool optic_thermal_image = false)
{
	SvpScopeInputs in;
	in.pack_pip_module = pack_pip_module && same_frame_enabled;
	in.optic_magnifies = optic_magnifies;
	in.optic_policy_allows = optic_policy_allows;
	in.target_set_ready = target_set_ready;
	in.optic_thermal_image = optic_thermal_image;
	return in;
}
