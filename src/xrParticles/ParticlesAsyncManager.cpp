#include "stdafx.h"
#include "ParticlesAsyncManager.h"

#include "../Include/xrRender/ParticleCustom.h"
#include "../Include/xrRender/RenderVisual.h"
#include "../xrEngine/IGame_Persistent.h"

static CParticlesAsync Instance;

xrCriticalSection CParticlesAsync::UpdateLock;

void CParticlesAsync::Play()
{
	if (DevicePtr && !Device.ParticleWorkerCallback)
		Device.ParticleWorkerCallback.bind(&CParticlesAsync::Start);

	if (!psDeviceFlags.test(mtParticles))
	{
		Instance.Start();
	}
}

void CParticlesAsync::Start()
{
	Instance.IsStarted.store(true, std::memory_order_release);

	// One (recursive) critical section over both loops: ForceUpdate() from the
	// game thread takes the same section, so a particle played mid-run can
	// never be OnFrame'd twice concurrently. Recursive, because a particle's
	// own Update can spawn another effect (Play -> ForceUpdate) on this thread.
	xrCriticalSectionGuard guard(UpdateLock);
	{
		PROF_EVENT("Particle Update");
        for (intrusive_ptr<CPS_Instance>& particle: g_pGamePersistent->ps_active)
        {
            if (particle->m_bDead)
                continue;

            Instance.UpdateParticle(particle);
        }
    }

    PROF_EVENT("Particle Shedule");
    for (intrusive_ptr<CPS_Instance>& particle : g_pGamePersistent->ps_active)
    {
		particle->Update(Device.dwTimeDelta);
	}

	Instance.IsStarted.store(false, std::memory_order_release);
}

void CParticlesAsync::Wait()
{
	if (psDeviceFlags.test(mtParticles))
	{
		PROF_EVENT("Particles Wait");
        xrSpinWait w;
		while (Instance.IsStarted.load(std::memory_order_acquire))
		{
			w();
		}
	}
}

void CParticlesAsync::ForceUpdate(intrusive_ptr<CPS_Instance> Obj)
{
	if (!Instance.IsStarted.load(std::memory_order_acquire))
		return;

	// Serialize with the worker's whole update section (see Start): the flag
	// was checked before the lock, so re-check under it - the worker may have
	// finished while this thread waited.
	xrCriticalSectionGuard guard(UpdateLock);
	if (!Instance.IsStarted.load(std::memory_order_acquire))
		return;

	Instance.UpdateParticle(Obj);
}

bool CParticlesAsync::NeedForceUpdate()
{
	return Instance.IsStarted.load(std::memory_order_acquire);
}

CParticlesAsync::CParticlesAsync()
{
	if (!DevicePtr)
		return;

	if (g_dedicated_server)
		return;

    Device.ParticleWorkerCallback.bind(&CParticlesAsync::Start);
}

void CParticlesAsync::UpdateParticle(intrusive_ptr<CPS_Instance> particle) const
{
	u32 dt = Device.dwTimeGlobal - particle->dwLastTime;
	IParticleCustom* V = smart_cast<IParticleCustom*>(particle->renderable.visual);

	if (V == nullptr)
		return;

	PROF_EVENT("Particle OnFrame");
	V->OnFrame(dt);

	particle->dwLastTime = Device.dwTimeGlobal;
}
