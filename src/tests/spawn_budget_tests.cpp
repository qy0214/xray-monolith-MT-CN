// Boundary tests for the per-frame spawn budget (xrCore/spawn_budget.h).
//
// The budget is what keeps a level load or a script burst from spawning thousands of
// entities in one frame. These lock the semantics that the game depends on:
//  - 0 means unlimited, i.e. the legacy behaviour is preserved
//  - the cap spreads spawns over frames instead of dropping them
//  - an empty queue never consumes the budget
#include "spawn_budget.h"
#include "test_harness.h"

void run_spawn_budget_tests()
{
	// legacy: no cap, everything drains right away
	{
		SpawnBudget b;
		b.begin(10);
		CHECK(b.take(1000) == 1000);
		CHECK(b.remaining() == 0xFFFFFFFFu);
		CHECK(!b.exhausted());
	}

	// cap: at most 8 per frame, leftovers wait for later frames
	{
		SpawnBudget b;
		b.max_per_frame = 8;
		b.begin(20);
		CHECK(b.take(1000) == 8);
		CHECK(b.take(1000) == 0); // the budget is spent for this frame
		CHECK(b.exhausted());

		// next frame resets the window
		b.begin(21);
		CHECK(!b.exhausted());
		CHECK(b.take(1000) == 8);

		// a second drain in the same frame stays capped, but a small queue is fine
		b.begin(22);
		CHECK(b.take(3) == 3);
		CHECK(b.remaining() == 5);
		CHECK(b.take(1000) == 5);
		CHECK(b.remaining() == 0);
	}

	// an empty queue must not consume the budget
	{
		SpawnBudget b;
		b.max_per_frame = 4;
		b.begin(30);
		CHECK(b.take(0) == 0);
		CHECK(b.remaining() == 4);
		CHECK(!b.exhausted());
		CHECK(b.take(4) == 4);
	}

	// a queue smaller than the cap drains completely
	{
		SpawnBudget b;
		b.max_per_frame = 8;
		b.begin(40);
		CHECK(b.take(2) == 2);
		CHECK(b.remaining() == 6);
		CHECK(b.take(0) == 0);
		CHECK(b.remaining() == 6);
	}

	// 0xFFFFFFFF frame ids must not be treated as "same frame"
	{
		SpawnBudget b;
		b.max_per_frame = 1;
		b.begin(0xFFFFFFFFu);
		CHECK(b.take(5) == 1);
		b.begin(0xFFFFFFFFu);
		CHECK(b.remaining() == 0);
	}

	std::printf("[tests] spawn_budget: 5 groups, %d checks\n", ::xrtest::checks());
}
