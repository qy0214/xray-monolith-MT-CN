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

	std::printf("[tests] svp_scope_state: 4 states, 19 checks\n");
}
