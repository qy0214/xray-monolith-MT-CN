// Boundary tests for the PiP scope state table (xrCore/svp_scope_state.h).
//
// These lock the rules that this session kept getting wrong:
//  - with the module disabled nothing PiP side happens (plain 3DSS is untouched)
//  - a magnifying optic without a ready second target set must never widen the main
//    view or change the mouse ratio (that exact state produced the "whole screen
//    magnified, mouse flying fast" regression)
//  - the wide view and the lens sensitivity are only valid together with a rendered
//    lens
#include "svp_scope_state.h"
#include "test_harness.h"

void run_svp_scope_state_tests()
{
	SvpScopeInputs in;

	// module off: everything inert (this is the "player disabled the PiP mod" case)
	SvpScopeState s = svp_evaluate_scope(in);
	CHECK(!s.lens_rendered);
	CHECK(!s.main_view_wide);
	CHECK(!s.aim_sensitivity_lens);

	// module installed but no magnifying optic
	in.pack_pip_module = true;
	s = svp_evaluate_scope(in);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// magnifying optic, policy allows, but the renderer has no second set yet
	in.optic_magnifies = true;
	in.optic_policy_allows = true;
	s = svp_evaluate_scope(in);
	CHECK(!s.lens_rendered);
	CHECK(!s.main_view_wide);
	CHECK(!s.aim_sensitivity_lens);

	// every input present: the lens owns the frame
	in.target_set_ready = true;
	s = svp_evaluate_scope(in);
	CHECK(s.lens_rendered);
	CHECK(s.main_view_wide);
	CHECK(s.aim_sensitivity_lens);

	// losing the target set must drop both derived flags again
	SvpScopeInputs no_set = in;
	no_set.target_set_ready = false;
	s = svp_evaluate_scope(no_set);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// an optic outside the module policy: no lens, so no wide view either
	SvpScopeInputs no_policy = in;
	no_policy.optic_policy_allows = false;
	s = svp_evaluate_scope(no_policy);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// a non magnifying optic can never take over, even with everything else ready
	SvpScopeInputs no_zoom = in;
	no_zoom.optic_magnifies = false;
	s = svp_evaluate_scope(no_zoom);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// a thermal optic must never take the second viewport: the pack's own shader has the whole
	// thermal model on the 3DSS path, and the engine's only thermal pass is a masked device
	// effect that writes black without a worn device (see svp_scope_state.h). Everything else
	// being ready must not change that.
	SvpScopeInputs thermal = in;
	thermal.optic_thermal_image = true;
	s = svp_evaluate_scope(thermal);
	CHECK(!s.lens_rendered);
	CHECK(!s.main_view_wide);
	CHECK(!s.aim_sensitivity_lens);

	// the input builder carries the thermal fact through like any other
	SvpScopeInputs th = svp_make_inputs(true, true, true, true, true, true);
	CHECK(th.optic_thermal_image);
	s = svp_evaluate_scope(th);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// and an optic that is not thermal is unaffected by the new input's presence
	SvpScopeInputs not_thermal = svp_make_inputs(true, true, true, true, true, false);
	CHECK(!not_thermal.optic_thermal_image);
	s = svp_evaluate_scope(not_thermal);
	CHECK(s.lens_rendered && s.main_view_wide && s.aim_sensitivity_lens);

	// the kill switch (r__svp_same_frame 0) must disable the whole chain
	SvpScopeInputs sw = svp_make_inputs(true, false, true, true, true);
	CHECK(!sw.pack_pip_module);
	s = svp_evaluate_scope(sw);
	CHECK(!s.lens_rendered && !s.main_view_wide && !s.aim_sensitivity_lens);

	// the input builder passes the optic facts through unchanged
	SvpScopeInputs ok = svp_make_inputs(true, true, true, true, true);
	CHECK(ok.pack_pip_module && ok.optic_magnifies && ok.optic_policy_allows && ok.target_set_ready);
	s = svp_evaluate_scope(ok);
	CHECK(s.lens_rendered && s.main_view_wide && s.aim_sensitivity_lens);

	std::printf("[tests] svp_scope_state: lens ownership states, including the thermal optic case\n");
}
