// Test entry point. Add one run_* declaration per test translation unit.
#include "test_harness.h"

void run_svp_scope_state_tests();

int main()
{
	run_svp_scope_state_tests();

	std::printf("[tests] %d checks, %d failures\n", ::xrtest::checks(), ::xrtest::failures());
	return ::xrtest::failures() == 0 ? 0 : 1;
}
