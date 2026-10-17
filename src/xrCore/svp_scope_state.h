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
	out.lens_rendered = in.pack_pip_module && in.optic_magnifies && in.optic_policy_allows && in.target_set_ready;
	// Never widen the view or change the mouse feel unless the lens is really drawn:
	// without a lens there is nothing to own the zoom, and the player would lose both
	// the magnification and the scoped mouse ratio.
	out.main_view_wide = out.lens_rendered;
	out.aim_sensitivity_lens = out.lens_rendered;
	return out;
}
