#pragma once
#include "../xrGame/ParticlesObject.h"
#include <atomic>
#include "../xrCore/xrSyncronize.h"

class CPS_Instance;

class CParticlesAsync
{
public:
	static void Play();
	static void Wait();

	static void ForceUpdate(intrusive_ptr<CPS_Instance> Obj);
	static bool NeedForceUpdate();

private:
	void UpdateParticle(intrusive_ptr<CPS_Instance> particle) const;
	static void Start();

public:
	CParticlesAsync();

private:
	// Review (zwork run 372f0ebd, planner #1): the flag was a volatile bool and
	// ForceUpdate() ran with no exclusion against the worker, so a particle
	// played from the game thread inside the render window could be OnFrame'd
	// concurrently with the worker iterating the same list. The flag is an
	// atomic with acquire/release (Wait() must also observe the worker's writes
	// before proceeding), and the worker's whole update section shares one
	// recursive critical section with ForceUpdate().
	std::atomic<bool> IsStarted = { false };
	static xrCriticalSection UpdateLock;
};
