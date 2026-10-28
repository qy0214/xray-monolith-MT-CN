#pragma once

class CBlender_sunshafts : public IBlender
{
public:
	virtual LPCSTR getComment() { return "OGSE: sunshafts"; }
	virtual BOOL canBeDetailed() { return FALSE; }
	virtual BOOL canBeLMAPped() { return FALSE; }

	virtual void Compile(CBlender_Compile& C);

	CBlender_sunshafts();
	virtual ~CBlender_sunshafts();
};

// Masks the sun's volumetric contribution around the sun's screen position so the
// screen-space shaft pattern is anchored to the sun instead of the view.
class CBlender_sunmask : public IBlender
{
public:
	virtual LPCSTR getComment() { return "engine: sun volumetric mask"; }
	virtual BOOL canBeDetailed() { return FALSE; }
	virtual BOOL canBeLMAPped() { return FALSE; }

	virtual void Compile(CBlender_Compile& C);

	CBlender_sunmask();
	virtual ~CBlender_sunmask();
};
