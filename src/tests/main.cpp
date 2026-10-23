// Test entry point. Add one run_* declaration per test translation unit.
#include "test_harness.h"

void run_svp_scope_state_tests();
void run_spawn_budget_tests();
void run_console_registry_tests();
void run_upscaler_alias_tests();

int main()
{
	run_svp_scope_state_tests();
	run_spawn_budget_tests();
	run_console_registry_tests();
	run_upscaler_alias_tests();

	std::printf("[tests] %d checks, %d failures\n", ::xrtest::checks(), ::xrtest::failures());
	return ::xrtest::failures() == 0 ? 0 : 1;
}
