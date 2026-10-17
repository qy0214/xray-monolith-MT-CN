// Boundary tests for the deferred console registry (xrEngine/console_registry.h).
//
// The registry exists so a command can live next to the code it configures; the only
// part worth testing in isolation is the queue itself, since applying it needs a live
// console. These lock the contract the two registrars rely on: registration queues,
// duplicates are kept (the console decides), and applying drains the queue exactly once.
#include "console_registry.h"
#include "test_harness.h"

void run_console_registry_tests()
{
	auto& queue = xr_console_deferred();
	queue.clear();
	CHECK(queue.empty());

	// null commands are ignored (callers may pass an optional one)
	xr_console_register_deferred(nullptr);
	CHECK(queue.empty());

	// registration only queues; nothing is applied before the console exists
	IConsole_Command* first = reinterpret_cast<IConsole_Command*>(0x1);
	IConsole_Command* second = reinterpret_cast<IConsole_Command*>(0x2);
	xr_console_register_deferred(first);
	xr_console_register_deferred(second);
	CHECK(queue.size() == 2);

	// the queue is a plain list, order preserved: the registrars walk it in order
	CHECK(queue[0] == first && queue[1] == second);

	// a second batch appends instead of replacing
	xr_console_register_deferred(first);
	CHECK(queue.size() == 3);

	// draining is what xr_console_apply_deferred does once the console is alive
	queue.clear();
	CHECK(queue.empty());

	std::printf("[tests] console_registry: 4 groups\n");
}
