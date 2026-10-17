#pragma once
// ---------------------------------------------------------------------------
// Per-frame spawn budget.
//
// The engine defers spawns into its own game_spawn_queue and drains it once per
// frame. Draining everything at once produces one long hitch when a level load or a
// script burst queues thousands of entities; the budget spreads that over frames.
//
// Deliberately dependency free (no engine headers) so it can be hosted by the test
// project and reasoned about on its own.
//
// Semantics, kept here so they can be tested instead of being re-derived:
//   max_per_frame == 0  -> unlimited, i.e. the legacy behaviour (drain everything)
//   otherwise           -> at most max_per_frame entities per frame, and never more
//                          than the queue actually holds (an empty queue must not
//                          consume the budget)
//
// Leftovers stay in the engine's own queue, so nothing is lost: only the frame an
// entity is spawned on changes.
// ---------------------------------------------------------------------------

struct SpawnBudget
{
	unsigned max_per_frame = 0;       // 0 = unlimited (legacy)
	unsigned consumed = 0;            // entities granted in the current frame
	unsigned frame = 0xFFFFFFFFu;     // frame the counters belong to

	// Starts a new frame's window. Cheap enough to call on every drain attempt.
	void begin(unsigned new_frame)
	{
		if (new_frame == frame)
			return;
		frame = new_frame;
		consumed = 0;
	}

	// How many entities the caller may drain right now, given how many are available.
	// Grants only what the caller can actually use, so an empty queue leaves the
	// budget untouched.
	unsigned take(unsigned available)
	{
		if (max_per_frame == 0)
			return available;

		const unsigned left = (consumed < max_per_frame) ? (max_per_frame - consumed) : 0u;
		const unsigned granted = (available < left) ? available : left;
		consumed += granted;
		return granted;
	}

	unsigned remaining() const
	{
		if (max_per_frame == 0)
			return 0xFFFFFFFFu;
		return (consumed < max_per_frame) ? (max_per_frame - consumed) : 0u;
	}

	bool exhausted() const { return max_per_frame != 0 && consumed >= max_per_frame; }
};
