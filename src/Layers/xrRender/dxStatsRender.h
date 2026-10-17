#ifndef	dxStatsRender_included
#define	dxStatsRender_included
#pragma once

#include "..\..\Include\xrRender\StatsRender.h"

// CPU cost of the main render path. The renderer measures it once per frame and
// publishes it here for the stats overlay (rs_stats on) and for a periodic log
// line, so a session log carries the numbers without anyone reading the screen.
namespace render_stats
{
	enum Section
	{
		Traverse,		// portal traversal, captures, terrain prepass, motion vectors
		GBuffer,		// scene geometry parts 0/1 (includes the HUD model)
		Lights,			// wallmarks, rain, sun and local lights
		Combine,		// phase_combine (contains the two below)
		HudSub,			// strict sorted HUD, item and camera attachment UI (in Combine)
		PpSub,			// phase_pp: resolve, upscaler, post effects (in Combine)
		Other,			// the rest of the frame
		SectionCount
	};
	extern const char* const sectionNames[SectionCount];
	extern float sectionMs[SectionCount];	// last completed frame
	extern float frameMs;					// whole CRender::Render() call

	float MsSince(u64 start);
	void BeginFrame();
	void Add(Section section, float ms);
	void Publish(float totalMs);
}

class dxStatsRender : public IStatsRender
{
public:
	virtual void Copy(IStatsRender& _in);

	virtual void OutData1(CGameFont& F);
	virtual void OutData2(CGameFont& F);
	virtual void OutData3(CGameFont& F);
	virtual void OutData4(CGameFont& F);
	virtual void GuardVerts(CGameFont& F);
	virtual void GuardDrawCalls(CGameFont& F);
	virtual void SetDrawParams(IRenderDeviceRender* pRender);
private:
};

#endif	//	dxStatsRender_included
