#include "stdafx.h"
#include "../../xrCore/mt_build.h"
#include "../../xrEngine/console_registry.h"
#include "../../xrEngine/igame_persistent.h"
#include "../xrRender/FBasicVisual.h"
#include "../xrRender/UpscalerCompat.h"
#include "../../xrEngine/customhud.h"
#include "../../xrEngine/xr_object.h"
#include "../xrRender/SkeletonCustom.h"
#include "../../xrParticles/ParticlesAsyncManager.h"

#include "../xrRender/QueryHelper.h"

#include	"../xrRender/dxRenderDeviceRender.h"
#include	"../xrRender/dxStatsRender.h"

void CRender::render_menu()
{
	PIX_EVENT(render_menu);
	//	Globals
	RCache.set_CullMode(CULL_CCW);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();

	// Main Render
	{
		Target->u_setrt(dlss::Configured() ? Target->rt_dlss_post_source : Target->rt_Generic_0, 0, 0, HW.pBaseZB); // LDR RT
		g_pGamePersistent->OnRenderPPUI_main(); // PP-UI
	}

	// Distort
	{
		FLOAT ColorRGBA[4] = {127.0f / 255.0f, 127.0f / 255.0f, 0.0f, 127.0f / 255.0f};
		const ref_rt& distort = dlss::Configured() ? Target->rt_dlss_post_color : Target->rt_Generic_1;
		Target->u_setrt(distort, 0, 0, HW.pBaseZB); // Now RT is a distortion mask
		HW.pContext->ClearRenderTargetView(distort->pRT, ColorRGBA);
		g_pGamePersistent->OnRenderPPUI_PP(); // PP-UI
	}

	// Actual Display
	Target->u_setrt(RenderScreenWidth(), RenderScreenHeight(), HW.pBaseRT,NULL,NULL, Target->u_SceneDepth());
	// Display sized target: never inherit the scene's render-sized viewport.
	Target->set_viewport_size(HW.pContext, float(RenderScreenWidth()), float(RenderScreenHeight()));
	RCache.set_Shader(Target->s_menu);
	RCache.set_Geometry(Target->g_menu);

	Fvector2 p0, p1;
	u32 Offset;
	auto C = color_rgba(255, 255, 255, 255);
	float _w = float(RenderScreenWidth());
	float _h = float(RenderScreenHeight());
	float d_Z = EPS_S;
	float d_W = 1.f;
	p0.set(.5f / _w, .5f / _h);
	p1.set((_w + .5f) / _w, (_h + .5f) / _h);

	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, Target->g_menu->vb_stride, Offset);
	pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
	pv++;
	pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
	pv++;
	pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
	pv++;
	pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
	pv++;
	RCache.Vertex.Unlock(4, Target->g_menu->vb_stride);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

// Scope resolution: while a magnifying optic (or a second viewport device) is in
// use, the pack magnifies the frame and the upscaler's pixel grid is magnified
// with it. Rebuild the upscaler and the scene targets at a higher internal
// resolution for as long as an optic is in use, then fall back after an idle time.
static bool  s_scopeResOn = false;
static float s_scopeResIdle = 0.f;

static bool ScopeResolutionRequested()
{
	if (!ps_r__upscaler_scope_lock || !dlss::Configured() || !g_pGamePersistent)
		return false;

	// PiP (T3 #6): with the second target set in charge of the lens, the resolution the
	// player actually reads is the one INSIDE the lens: the lens image is magnified into the
	// aperture, so the upscaler's internal resolution is what its pixels are made of. The
	// earlier version of this function returned false for this case ("the override has nothing
	// left to buy - the scope frame renders at that set's own extent"), and that is what the
	// player sees as the small sight going pixelated after a while: with the PiP path live the
	// boost could never even be REQUESTED while the lens was up, so it survived only on the
	// idle timer and then fell back to the default upscaler scale mid aim.
	if (Device.m_SecondViewport.IsSVPActive())
		return true;

	const float zoom = g_pGamePersistent->m_pGShaderConstants->hud_fov_params.x;
	return zoom > 1.25f || Device.m_SecondViewport.IsSVPActive();
}

// Returns true when the targets were rebuilt; the caller then skips this frame,
// which is what reset_end() expects (m_bFirstFrameAfterReset).
static bool UpdateScopeResolution()
{
	if (!dlss::Configured())
	{
		s_scopeResOn = false;
		s_scopeResIdle = 0.f;
		return false;
	}

	const bool requested = ScopeResolutionRequested();
	if (requested)	s_scopeResIdle = 0.f;
	else			s_scopeResIdle += Device.fTimeDelta;

	const bool want = requested || (s_scopeResOn && s_scopeResIdle < float(ps_r__upscaler_scope_delay));
	if (want == s_scopeResOn) return false;
	if (!want && dlss::RenderWidth() >= Device.dwWidth) return false;

	s_scopeResOn = want;

	// Rebuild the upscaler for the new size first, then the scene targets, exactly
	// like reset_begin()/reset_end() do for a display resolution change.
	dlss::SetScopeRenderScale(want ? float(ps_r__upscaler_scope_scale) : 0.f);
	RImplementation.reset_begin();
	dlss::InitializeTargets();
	RImplementation.reset_end();

	return true;
}

extern u32 g_r;

// PiP (T3 #6): two cameras per frame. The game publishes the normal view and the optic's
// lens FOV; the renderer copies the game camera (s_main) and derives the lens camera
// (s_zoom, the same view with the projection magnified by g_fov / lens FOV) instead of
// asking the game for it, then applies whichever one a pass needs.
namespace svpcam
{
	struct View
	{
		Fmatrix mView, mProject, mProjectHud, mFullTransform, mInvView;
		Fvector4 hudFov;
		bool valid;

		View() : valid(false) { hudFov.set(0.f, 0.f, 0.f, 0.f); }
	};

	View s_main, s_zoom;

	// Magnification actually applied to the lens camera by the last DeriveZoom() call.
	// Logged with the lens probe so a wrong lens image can be attributed to the camera
	// derivation instead of being guessed at.
	float s_lastMag = 0.f;
	// Smoothed lens FOV in degrees (see DeriveZoom): the lens follows a change of sight power
	// with the same 0.12 s feel the stock camera has. It is the FOV that is smoothed, not a
	// ratio, because the projection is rebuilt from the optic's authored angle.
	float s_fovSmooth = 0.f;

	// Live tuning of the lens magnification in percent of the optic's authored value.
	// Some packs leave a sight's zoom range undefined (the engine then reports g_fov as its
	// highest power, i.e. 1x), so the authored lens FOV is the only usable number and the
	// player needs a way to settle the exact power without a new build per guess. Default
	// 100 = exactly what the optic authors.
	int s_lensPowerPct = 100;

	// Diagnostic switch for the lens content probes. 0 disables everything, 1 logs the
	// throttled readbacks and the per pass lines, 2 additionally writes the lens images to
	// BMP in the logs directory (tens of megabytes per dump, so it is never on by default).
	// The probes were what found the faults in this chain and are cheap at level 1, but the
	// plan is to drop the default to 0 once the lens image is settled.
	int s_svpDebug = 1;

	void RegisterLensCvars()
	{
		xr_console_register_int_cvar("r__svp_lens_power", &s_lensPowerPct, 25, 400);
		xr_console_register_int_cvar("r__svp_debug", &s_svpDebug, 0, 2);
	}

	void Capture(View& v)
	{
		v.mView.set(Device.mView);
		v.mProject.set(Device.mProject);
		v.mProjectHud.set(Device.mProjectHud);
		v.mFullTransform.set(Device.mFullTransform);
		v.mInvView.set(Device.mInvView);
		if (g_pGamePersistent && g_pGamePersistent->m_pGShaderConstants)
			v.hudFov.set(g_pGamePersistent->m_pGShaderConstants->hud_fov_params);
		v.valid = true;
	}

	bool Ready() { return s_main.valid && s_zoom.valid; }

	void Apply(const View& v)
	{
		if (!v.valid)
			return;

		Device.mView.set(v.mView);
		Device.mProject.set(v.mProject);
		Device.mProjectHud.set(v.mProjectHud);
		Device.mFullTransform.set(v.mFullTransform);
		Device.mInvView.set(v.mInvView);
		if (g_pGamePersistent && g_pGamePersistent->m_pGShaderConstants)
			g_pGamePersistent->m_pGShaderConstants->hud_fov_params.set(v.hudFov);

		RCache.set_xform_view(Device.mView);
		RCache.set_xform_project(Device.mProject);
	}

	// The lens camera: the same eye and view as the main view, with the projection rebuilt
	// around the optic's own lens FOV. The reference derives a full optics camera here; this
	// covers the visible part of it.
	void DeriveZoom(View& z, const View& m)
	{
		z = m;

		// The packs state the sight's lens FOV in degrees in scope_lens_fov and state the power
		// as g_fov / scope_lens_fov, so the lens camera must be exactly that many degrees wide.
		// It used to be the main projection scaled by the ratio (g_fov / lensFov), and that is
		// not the same thing: build_projection() is built from tan(fov/2), so a 21.667 degree
		// sight was rendered at 24.0 degrees, i.e. about 10 percent less magnification than the
		// pack authors - the magnification the last round reported as wrong. The ratio also
		// inherited whatever FOV the main view happened to have, and the ADS animation moves it.
		// Rebuilding from the angle makes the camera, the pack's formula and this engine's own
		// scope_lense_main_view binder (which uses fFOV / lensFov) agree exactly.
		const float lensFov = Device.m_SecondViewport.svp_lens_fov;
		if (lensFov > 1.0f && lensFov < 89.0f)
		{
			// r__svp_lens_power scales the AUTHORED angle, so 200 percent is half the FOV and
			// therefore twice the power.
			const float target = clampr(lensFov * (100.0f / float(s_lensPowerPct)), 1.0f, 120.0f);
			// ~0.12 s time constant, i.e. the same feel as the stock zoom animation. The
			// smoothing must start from the target when the lens was not rendering just
			// before: ramping from zero opened every aim wider than the naked eye.
			static u32 s_lastLensFrame = 0;
			if (Device.dwFrame > s_lastLensFrame + 2)
				s_fovSmooth = target;
			s_lastLensFrame = Device.dwFrame;

			const float k = 1.0f - expf(-Device.fTimeDelta / 0.12f);
			s_fovSmooth += (target - s_fovSmooth) * k;

			// Mirror build_projection(): _22 = cot(fov/2), _11 = aspect * that, with the aspect
			// taken from the main projection itself so no convention has to be assumed.
			const float cot = 1.0f / tanf(deg2rad(s_fovSmooth) * 0.5f);
			const float aspect = (m.mProject._22 > 1e-6f) ? (m.mProject._11 / m.mProject._22) : 1.0f;
			z.mProject._11 = aspect * cot;
			z.mProject._22 = cot;
			z.mFullTransform.mul(z.mView, z.mProject);
			// Reported as the power the player actually sees: the main view's own FOV over the
			// lens FOV, which is exactly what the lens shaders compute as current_zoom. It is
			// for the log and the probes; the published sampling mapping is the identity.
			s_lastMag = (Device.fFOV > 1.0f) ? (Device.fFOV / s_fovSmooth) : 1.0f;
			z.valid = true;
			return;
		}

		float mag = 1.0f;
		if (g_pGamePersistent && g_pGamePersistent->m_pGShaderConstants)
		{
			const Fvector4& hud = g_pGamePersistent->m_pGShaderConstants->hud_params;
			mag = (hud.y > 1.01f) ? hud.y : hud.x;
		}

		if (mag > 1.01f)
		{
			z.mProject._11 *= mag;
			z.mProject._22 *= mag;
			z.mFullTransform.mul(z.mView, z.mProject);
		}
		s_lastMag = mag;
		z.valid = true;
	}
}

// Global-scope entry point for r4.cpp (the tuning cvars live in the svpcam namespace).
void SvpRegisterLensCvars()
{
	svpcam::RegisterLensCvars();
}

// Dump a render target as a BMP so the lens image can be looked at directly instead of
// being inferred from log numbers. 8 bit BGRA targets go out as 24 bit BMP; 16F targets are
// clamped to 0..1 and scaled, which is enough to see whether an image contains a scene.
static void svp_dump_rt(LPCSTR path, ref_rt& rt)
{
	if (!rt || !rt->pSurface)
		return;

	ID3D11Texture2D* tex = nullptr;
	if (FAILED(rt->pSurface->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex)) || !tex)
		return;

	D3D11_TEXTURE2D_DESC d = {};
	tex->GetDesc(&d);

	D3D11_TEXTURE2D_DESC s = d;
	s.Usage = D3D11_USAGE_STAGING;
	s.BindFlags = 0;
	s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	s.MiscFlags = 0;
	s.ArraySize = 1;
	s.MipLevels = 1;
	s.SampleDesc.Count = 1;
	s.SampleDesc.Quality = 0;

	ID3D11Texture2D* stg = nullptr;
	if (FAILED(HW.pDevice->CreateTexture2D(&s, nullptr, &stg)) || !stg)
	{
		_RELEASE(tex);
		return;
	}
	HW.pContext->CopyResource(stg, tex);

	D3D11_MAPPED_SUBRESOURCE m = {};
	if (SUCCEEDED(HW.pContext->Map(stg, 0, D3D11_MAP_READ, 0, &m)))
	{
		const bool float16 = (d.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);
		const u32 bpp = float16 ? 8u : 4u;
		const u32 rowBytes = ((d.Width * 3u) + 3u) & ~3u;

		FILE* f = fopen(path, "wb");
		if (f)
		{
			const u32 dataBytes = rowBytes * d.Height;
			u8 hdr[54] = {};
			hdr[0] = 'B'; hdr[1] = 'M';
			const u32 total = 54u + dataBytes;
			memcpy(&hdr[2], &total, 4);
			const u32 off = 54;
			memcpy(&hdr[10], &off, 4);
			const u32 info = 40;
			memcpy(&hdr[14], &info, 4);
			const s32 w = (s32)d.Width, h = (s32)d.Height;
			memcpy(&hdr[18], &w, 4);
			memcpy(&hdr[22], &h, 4);
			const u16 planes = 1, bppOut = 24;
			memcpy(&hdr[26], &planes, 2);
			memcpy(&hdr[28], &bppOut, 2);
			memcpy(&hdr[34], &dataBytes, 4);
			fwrite(hdr, 1, 54, f);

			xr_vector<u8> row(rowBytes, 0);
			for (s32 y = (s32)d.Height - 1; y >= 0; --y) // BMP is bottom up
			{
				const u8* src = (const u8*)m.pData + size_t(y) * m.RowPitch;
				for (u32 x = 0; x < d.Width; ++x)
				{
					u8 r, g, b;
					if (float16)
					{
						// half float -> float -> byte
						const u16* hp = (const u16*)(src + size_t(x) * bpp);
						auto h2f = [](u16 h) -> float {
							const u32 sign = (h >> 15) & 1u, exp = (h >> 10) & 0x1Fu, man = h & 0x3FFu;
							float v;
							if (exp == 0) v = man ? ldexpf(float(man), -24) : 0.f;
							else if (exp == 31) v = man ? 1.f : 1.f;
							else v = ldexpf(float(man) + 1024.f, int(exp) - 25);
							return sign ? -v : v;
						};
						const float rf = h2f(hp[0]), gf = h2f(hp[1]), bf = h2f(hp[2]);
						r = (u8)clampr(int(rf * 255.f), 0, 255);
						g = (u8)clampr(int(gf * 255.f), 0, 255);
						b = (u8)clampr(int(bf * 255.f), 0, 255);
					}
					else
					{
						const u8* p = src + size_t(x) * bpp;
						b = p[0]; g = p[1]; r = p[2];
					}
					row[x * 3 + 0] = b;
					row[x * 3 + 1] = g;
					row[x * 3 + 2] = r;
				}
				fwrite(row.data(), 1, rowBytes, f);
			}
			fclose(f);
			Msg("* [PIP] dumped %s (%ux%u)", path, d.Width, d.Height);
		}
		HW.pContext->Unmap(stg, 0);
	}
	_RELEASE(stg);
	_RELEASE(tex);
}

// PiP lens content probe (diagnostic, throttled): reads back a 16x16 block from the centre// of the second view's scene result and of the texture the lens material actually samples.
// A black lens has three very different causes and the log must separate them:
//   source black             -> the second view rendered nothing (camera, culling, combine)
//   source good, dst black   -> the blit did not land (format or size mismatch) or the
//                               destination is not the surface the shader resolves
//   both good                -> the content path is fine and the disagreement is on the
//                               shader side (scope_svp, motion, sampling)
static void svp_probe_resource(LPCSTR tag, ID3D11Resource* res)
{
	if (!res)
	{
		Msg("* [PIP] probe %s: no surface", tag);
		return;
	}

	ID3D11Texture2D* tex = nullptr;
	if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex)) || !tex)
	{
		Msg("* [PIP] probe %s: not a 2D texture", tag);
		return;
	}

	D3D11_TEXTURE2D_DESC d = {};
	tex->GetDesc(&d);

	D3D11_TEXTURE2D_DESC s = d;
	s.Usage = D3D11_USAGE_STAGING;
	s.BindFlags = 0;
	s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	s.MiscFlags = 0;
	s.ArraySize = 1;
	s.MipLevels = 1;
	s.SampleDesc.Count = 1;
	s.SampleDesc.Quality = 0;

	ID3D11Texture2D* stg = nullptr;
	if (FAILED(HW.pDevice->CreateTexture2D(&s, nullptr, &stg)) || !stg)
	{
		Msg("* [PIP] probe %s: staging failed fmt=%u %ux%u", tag, d.Format, d.Width, d.Height);
		_RELEASE(tex);
		return;
	}

	HW.pContext->CopyResource(stg, tex);

	D3D11_MAPPED_SUBRESOURCE m = {};
	if (SUCCEEDED(HW.pContext->Map(stg, 0, D3D11_MAP_READ, 0, &m)))
	{
		const u32 bpp = (d.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) ? 8u : 4u;
		const u32 cx = d.Width / 2, cy = d.Height / 2;
		u64 sum[4] = { 0, 0, 0, 0 };
		u8 mx[4] = { 0, 0, 0, 0 };
		u32 nz = 0, n = 0;
		for (u32 y = cy - 8; y < cy + 8; ++y)
			for (u32 x = cx - 8; x < cx + 8; ++x)
			{
				const u8* p = (const u8*)m.pData + size_t(y) * m.RowPitch + size_t(x) * bpp;
				bool any = false;
				for (int i = 0; i < 4; ++i)
				{
					sum[i] += p[i];
					if (p[i] > mx[i]) mx[i] = p[i];
					if (p[i]) any = true;
				}
				if (any) ++nz;
				++n;
			}

		// Coverage and variety over a coarse grid. The centre alone cannot tell a real image
		// from a uniform fog wall: a magnified view can be legitimately empty in the middle,
		// and a fog wall reads as fully non-zero everywhere. cover counts grid cells with any
		// non-zero byte and uniq the distinct colours seen, so "cover=64/64 uniq=1" is the flat
		// fog wall while a large uniq means a real frame.
		u32 cover = 0, uniq = 0;
		xr_set<u32> seen;
		for (u32 by = 0; by < 8; ++by)
			for (u32 bx = 0; bx < 8; ++bx)
			{
				const u32 x = bx * d.Width / 8, y = by * d.Height / 8;
				const u8* p = (const u8*)m.pData + size_t(y) * m.RowPitch + size_t(x) * bpp;
				if (p[0] || p[1] || p[2] || p[3]) ++cover;
				const u32 key = u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
				if (seen.insert(key).second) ++uniq;
			}

		Msg("* [PIP] probe %s: fmt=%u %ux%u avg b=%u g=%u r=%u a=%u cover=%u/64 uniq=%u",
			tag, d.Format, d.Width, d.Height,
			u32(sum[0] / n), u32(sum[1] / n), u32(sum[2] / n), u32(sum[3] / n),
			cover, uniq);
		HW.pContext->Unmap(stg, 0);
	}
	else
	{
		Msg("* [PIP] probe %s: map failed", tag);
	}

	_RELEASE(stg);
	_RELEASE(tex);
}

// The render targets of a set must be read through CRT::pSurface, never through
// pTexture->surface_get(): pTexture is a name keyed texture object that SetActive()
// re-points to whichever set is active, so probing "the main set's rt_Generic_0" while
// the second set is bound silently returns the SECOND set's texture. The identical
// values the two probes reported for the main and second set are what exposed this.
static void svp_probe_rt(LPCSTR tag, ref_rt& rt)
{
	if (!rt || !rt->pSurface)
	{
		Msg("* [PIP] probe %s: no texture", tag);
		return;
	}
	svp_probe_resource(tag, rt->pSurface);
}

static void svp_probe_composite()
{
	if (!HW.m_pSwapChain)
		return;
	ID3D11Texture2D* bb = nullptr;
	if (SUCCEEDED(HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) && bb)
	{
		svp_probe_resource("control(backbuffer)", bb);
		_RELEASE(bb);
	}
}

// PiP (T3 #6): compile time switch for the same-frame lens, kept at file scope so the early
// second-viewport allocation gate can read it. With it on, and only when the pack's PiP
// module is actually installed (pip_lens_shaders), the renderer draws the lens and the main
// view in the same frame, one pass each; the reachability of that whole chain is decided at
// run time by r__svp_same_frame and by the pack being present, so a stock 3DSS setup and a
// setup without the module both keep their previous behaviour untouched.
static const bool SVP_SAME_FRAME_READY = true;

// Startup stamp helper (see CRender::create): lets the log state which engine build is
// actually running and whether the same-frame PiP path is live, so a stale exe in the
// game bin can never masquerade as a fresh one again.
bool SvpSameFramePathEnabled() { return SVP_SAME_FRAME_READY; }

// PiP (T3 #6): the current frame's lens camera VP, published by the renderer for the lens
// shader's scope_lense_reproject binder (see Blender_Recorder_StandartBinding.cpp). The
// lens shader projects the main view's lens-area UVs into the second view texture through
// that matrix; in a same-frame implementation the second view is rendered in this very
// frame, so the matrix must be vp_lens * inv(vp_main) built from the current frame's lens
// camera - not the previous frame's VP, which is what the reference uses because it only
// captures the SVP every other frame. Referencing the previous frame displaced the
// projection and showed a lens image that was neither current nor correctly placed.
Fmatrix svp_vp_lens = Fidentity;
bool svp_vp_lens_valid = false;

// Throttle for the publish step's before/after readback (see the lens publish block).
static u32 s_publishProbeAt = 0;

void CRender::Render()
{
	PIX_EVENT_C(CRender_Render, dx10_marker_frame);
	dx10_annotate_frame();
	dlss::EndScene();

	rmNormal();

	compat::Tick();	// refresh engine-owned MOD-compat values (scope_lense_ads_is_pip)

	// Menu frame limiting (see r__menu_framelimit): mark the frame start so the
	// menu path can add the remaining slice of its target frame time.
	const u64 menuPaceStart = CPU::QPC();

	// pipdlss reads these two flags in the upscaler's SvpPassBound(); this engine
	// refreshes them once per frame from its own SecondVP state (see device.h).
	Device.true_pip_on = Device.m_SecondViewport.IsSVPActive();
	// PiP: while the lens is up, m_render_pass_is_svp is owned by
	// CRenderTarget::SetActive (per pass); without a second set the frame-level
	// predicate stays authoritative
	if (!Device.true_pip_on)
		Device.m_SecondViewport.m_render_pass_is_svp = false;

	bool _menu_pp = g_pGamePersistent ? g_pGamePersistent->OnRenderPPUI_query() : false;
	if (_menu_pp)
	{
		dlss::EndScene(true);
		render_menu();
		return;
	};

	IMainMenu* pMainMenu = g_pGamePersistent ? g_pGamePersistent->m_pMainMenu : 0;
	bool bMenu = pMainMenu ? pMainMenu->CanSkipSceneRendering() : false;

	if (!(g_pGameLevel && g_hud)
		|| bMenu)
	{
		dlss::EndScene(true);
		Target->u_setrt(RenderScreenWidth(), RenderScreenHeight(), HW.pBaseRT,NULL,NULL, Target->u_SceneDepth());

		// Optional menu frame cap (0 = unlimited, the default). The menu keeps a 3D
		// scene rendering, so capping it saves power and heat; gameplay is untouched
		// because this branch only runs while the main menu is up. Loading screens
		// take the other path and are never paced.
		if (bMenu && compat::r__menu_framelimit > 0)
		{
			const float targetMs = 1000.0f / float(compat::r__menu_framelimit);
			const float restMs = targetMs - render_stats::MsSince(menuPaceStart);
			if (restMs > 1.0f)
				Sleep(DWORD(restMs));
		}
		return;
	}

	if (m_bFirstFrameAfterReset)
	{
		for (light* L : v_all_lights)//critical!!!
			L->m_moving_frames = 0;

		m_bFirstFrameAfterReset = false;
		return;
	}

	// PiP (T3 #6): the second target set must exist before the scope-resolution
	// decision below, which retires the override while that set handles the lens
	// Create the lens set from the optic FACTS, never from the evaluated state: the state
	// requires the set to exist (target_set_ready) while the set used to be created only
	// once the state was already true, so nothing ever happened. The evaluated state is
	// what turns the lens pass on afterwards.
	//
	// A thermal optic is excluded here as well as in the state: its image is computed by the
	// pack's own shader on the 3DSS path, so it never needs a second target set and creating
	// one for it would only cost VRAM and a second scene pass (see svp_scope_state.h).
	const bool opticThermalImage = Device.m_SecondViewport.svp_optic_thermal
		|| Device.m_SecondViewport.svp_optic_image_type >= 2;
	if (SVP_SAME_FRAME_READY && compat::r__svp_same_frame && Device.m_SecondViewport.pip_lens_shaders
		&& Device.m_SecondViewport.svp_optic_magnifies && Device.m_SecondViewport.svp_lens_fov > 1.0f
		&& !opticThermalImage)
		EnsureTargetSVP();

	// Scope-aware internal resolution: a rebuild here skips this frame, so the new
	// targets start clean (the same contract as m_bFirstFrameAfterReset above).
	if (UpdateScopeResolution())
		return;

	if ((Device.dwFrame % (u32)ps_r__tex_evict_interval) == 0)
		dxRenderDeviceRender::Instance().Resources->EvictStalledTextures();

	//.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);

	// Frame statistics: a handful of QPC reads per frame, published for the stats
	// overlay and the periodic log line. No GPU work and no allocations.
	const u64 statsFrameStart = CPU::QPC();
	u64 statsMark = statsFrameStart;
	render_stats::BeginFrame();

	// PiP (T3 #6): the lens view and the normal view are drawn in the same frame, one pass
	// each, so the lens image is always current - unlike the legacy scheme, which rendered
	// the zoomed view on alternate frames, copied the swapchain into the lens texture and
	// skipped Present on the scope frames. Drawn before the main pass so the optic, which
	// the main pass draws over the lens image, samples a texture that is already filled.
	svp_lens_rendered = false;

	// PiP: no lens pass this frame means no current-frame lens VP; the binder falls back to
	// the previous-frame matrix (the legacy behaviour) so nothing reads a stale one.
	svp_vp_lens_valid = false;

	// C1/C2: the renderer reads the game published inputs exactly once per frame and
	// evaluates the one state everything else derives from. Reading them again later in
	// the frame would race with the game thread, which publishes them once per frame.
	{
		const float lensFov = Device.m_SecondViewport.svp_lens_fov;
		const SvpScopeInputs svpIn = svp_make_inputs(Device.m_SecondViewport.pip_lens_shaders,
			compat::r__svp_same_frame != 0, Device.m_SecondViewport.svp_optic_magnifies,
			lensFov > 1.0f, TargetSVP != nullptr, opticThermalImage);
		Device.m_SecondViewport.svp_scope = svp_evaluate_scope(svpIn);
	}

	// Single writer of the second viewport state: the pack's lens shaders (scope_svp and
	// m_blender_mode.z, both set here) read exactly what this frame renders, so "shaders
	// take the SVP path" and "engine renders a lens" can never disagree again.
	Device.m_SecondViewport.SetSVPActive(Device.m_SecondViewport.svp_scope.lens_rendered);

	// PiP state (throttled): the one line that makes any remaining mismatch diagnosable in
	// a single round - whether the lens rendered, whether the set exists, the kill switch
	// and the resolved lens FOV.
	if (Device.m_SecondViewport.svp_optic_magnifies && (Device.dwFrame % 300) == 0)
		Msg("* [PIP] state build=%d lens=%d pub=%d target=%d switch=%d lensFov=%.3f",
			MT_PIP_BUILD, Device.m_SecondViewport.svp_scope.lens_rendered ? 1 : 0,
			svp_lens_rendered ? 1 : 0, TargetSVP ? 1 : 0,
			compat::r__svp_same_frame, Device.m_SecondViewport.svp_lens_fov);

	// PiP: the lens shaders decide on ps_r2_heatvision / ps_r2_nightvision, and in the PiP
	// branch they no longer produce the imaging look themselves (the stock path calls
	// infrared() on the G-buffer, which the second-view branch skips).
	//
	// For a thermal optic that is exactly why the engine withholds the second viewport: the
	// pack's own shader renders the thermal image on the 3DSS path (infrared() over the
	// G-buffer, the heat map LUT, pixelation) and its PiP branch delegates nothing usable to
	// the engine, while the engine's only thermal pass is the masked device effect that writes
	// black with no device worn. Driving ps_r2_heatvision from the optic is the wrong mechanism
	// and was tried and reverted: it is a global switch with frame wide consequences (upscaler
	// bypass, flare path, a different shader element) and it expects the heat mask a worn device
	// fills. This line reports the decision, not a plan.
	if (opticThermalImage && (Device.dwFrame % 300) == 0)
		Msg("* [PIP] imaging: thermal optic (img=%d thermal=%d mode=%d, device thermal=%d) - the engine withholds the second viewport so the pack's own 3DSS thermal path renders the lens image",
			Device.m_SecondViewport.svp_optic_image_type,
			Device.m_SecondViewport.svp_optic_thermal ? 1 : 0,
			Device.m_SecondViewport.svp_optic_thermal_mode,
			compat::scope_lense_head_thermal_active);
	Device.true_pip_on = Device.m_SecondViewport.svp_scope.lens_rendered;	Device.m_SecondViewport.true_pip_rendering = Device.m_SecondViewport.svp_scope.lens_rendered;
	Device.m_SecondViewport.true_pip_same_frame = Device.m_SecondViewport.svp_scope.lens_rendered;

	const bool pipOn = SVP_SAME_FRAME_READY && Device.m_SecondViewport.svp_scope.lens_rendered;

	// PiP diagnostic (throttled): an optic the pack covers, but no second target set -
	// either its creation failed or the path is disabled. This is the failure the lens
	// going black used to hide behind.
	if (Device.m_SecondViewport.pip_lens_shaders && Device.m_SecondViewport.svp_optic_magnifies
		&& Device.m_SecondViewport.svp_lens_fov > 1.0f && TargetSVP == nullptr && (Device.dwFrame % 600) == 0)
		Msg("* [PIP] optic is PiP capable but the second target set is missing (creation failed or path disabled)");
	// PiP: from here on the renderer owns the lens view (the game alternation and the
	// game side flags are already derived from the state evaluated above)
	if (pipOn && TargetSVP)
	{
		svpcam::Capture(svpcam::s_main); // the game camera is the normal one now
		svpcam::DeriveZoom(svpcam::s_zoom, svpcam::s_main);
	}

	dlss::BeginScene();

	// One lens pass and one main pass. The discarded double render that separated "first pass
	// of the frame" effects from other causes is no longer needed: that question was answered
	// (the second viewport was handed the main set's depth buffer, so Direct3D threw away
	// every draw).
	const int lensRuns = pipOn ? 1 : 0;
	const int viewCount = lensRuns + 1;

	for (int view = 0; view < viewCount; ++view)
	{
		const bool lensPass = (view < lensRuns);
		const bool publish = lensPass && (view == lensRuns - 1);
		const u32 rcCalls0 = RCache.stat.calls, rcVerts0 = RCache.stat.verts;

		if (viewCount > 1)
		{
			CRenderTarget* passTarget = lensPass ? TargetSVP : TargetMain;
			if (passTarget)
				passTarget->SetActive();
			// Only the published lens run uses the derived lens camera; the client pass and
			// the (optional) discarded diagnostic run use the main camera.
			svpcam::Apply(publish ? svpcam::s_zoom : svpcam::s_main);
			render_stats::SetSuppress(lensPass);
		}

		// PiP: publish the mapping the lens shader uses to sample the second view.
		//
		// The contract is in the pack's anthology_pip_temporal.h: pip_project_capture() maps a
		// MAIN view NDC point through this matrix and samples the lens texture there -
		//     clip = mul(scope_lense_reproject, float4(main_ndc, 1, 1))
		// - and pip_capture_color() reads s_second_vp at the result, so the aperture shows
		// exactly what the mapping selects.
		//
		// IDENTITY, and the derivation is exact now that the lens camera is built at the
		// authored angle (DeriveZoom). Looking through the aperture of power mag, the eye at
		// apparent direction t sees the world direction t / mag, so the aperture pixel at
		// screen NDC n shows the world direction n * (fFOV / 2) / mag. The lens camera, whose
		// FOV is lensFov = fFOV / mag by construction, puts that same direction at
		//     NDC = [n * (fFOV / 2) / mag] / (lensFov / 2) = n * (fFOV / lensFov) / mag = n
		// i.e. at the aperture's own NDC, so the sample is the pixel's own uv. Scaling by mag
		// here instead pastes the naked eye view over the aperture (no magnification at all),
		// and a fully derived matrix was measured degenerate (sampled spans of 1.5, 0.62 and
		// 0 - the last one pinned a single texel over the aperture, black and direction
		// independent). The reference only needs a real matrix because it captures a cropped
		// sub rect of the display sized texture, which this engine does not do.
		if (publish && svpcam::Ready())
		{
			svp_vp_lens.identity();
			svp_vp_lens_valid = true;
		}
	// Configure
	RImplementation.o.distortion = FALSE; // disable distorion
	Fcolor sun_color = ((light*)Lights.sun_adapted._get())->color;
	BOOL bSUN = ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r, sun_color.g, sun_color.b)>EPS) && !Core.ParamsData.test(ECoreParams::r4_dev);
	if (o.sunstatic) bSUN = FALSE;
	// Msg						("sstatic: %s, sun: %s",o.sunstatic?;"true":"false", bSUN?"true":"false");

	// HOM
	// PiP: the lens pass renders with a very narrow projection (a 24x optic is about 2.5
	// degrees), and building the visibility frustum from it made the sector/portal traversal
	// and the occlusion culling reject the world - the lens pass then drew only the sky and
	// the fog, which is the uniform grey image that kept coming back while the main view was
	// fine. Build the frustum from the MAIN camera for the lens pass: it is a superset of the
	// lens view, so nothing that belongs in the lens can be culled, and the narrow projection
	// still decides which pixels are drawn. The HOM is left to the main pass, whose result the
	// rest of the frame uses.
	const bool svpWideFrustum = pipOn && view == 0 && svpcam::s_main.valid;
	const bool svpLensViewPass = pipOn && view == 0;
	Fmatrix& svpVisMatrix = svpWideFrustum ? svpcam::s_main.mFullTransform : Device.mFullTransform;
	ViewBase.CreateFromMatrix(svpVisMatrix, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
	if (!svpWideFrustum)
		HOM.MT_RENDER();

	Target->phase_scene_prepare();


	//******* Main calc - DEFERRER RENDERER
	phase = PHASE_NORMAL;
	
	/*if (RImplementation.o.ssfx_core) // SSS23: DEPRECATED
	{
		// HUD Masking rendering
		FLOAT ColorRGBA[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_hud->pRT, ColorRGBA);

		Target->u_setrt(Target->rt_ssfx_hud, NULL, NULL, HW.pBaseZB);
		r_dsgraph_render_hud(true);

		// Reset Depth
		HW.pContext->ClearDepthStencilView(HW.pBaseZB, D3D_CLEAR_DEPTH, 1.0f, 0);
	}*/

    GMBase.traverse(RImplementation.pLastSector, ViewBase, Device.vCameraPosition, svpVisMatrix);
    GMBase.r_dsgraph_capture_static();
    GMBase.r_dsgraph_capture_dynamic();

    if (RImplementation.o.ssfx_motionvectors)
    {
        Target->u_setrt(RenderScreenWidth(), RenderScreenHeight(), 0, 0, Target->rt_ssfx_motion_vectors->pRT, 0);

        FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        HW.pContext->ClearRenderTargetView(Target->rt_ssfx_motion_vectors->pRT, ColorRGBA);

        RCache.set_Stencil(FALSE);
        g_pGamePersistent->Environment().RenderSky(true);

        RCache.Index.Flush();
        RCache.Vertex.Flush();

        RCache.set_xform_world(Fidentity);
    }

	if (ps_r2_ls_flags.test(R2FLAG_TERRAIN_PREPASS))
	{
		Target->u_setrt(RenderScreenWidth(), RenderScreenHeight(), NULL, NULL, NULL, !RImplementation.o.dx10_msaa ? Target->u_SceneDepth() : Target->rt_MSAADepth->pZRT);
	}

	render_stats::Add(render_stats::Traverse, render_stats::MsSince(statsMark));
	statsMark = CPU::QPC();

	//******* Main render :: PART-0	-- first
	{
		PIX_EVENT_C(DEFER_PART0_SPLIT, dx10_marker_gbuffer);
		// level, SPLIT
		Target->phase_scene_begin();
		GMBase.r_dsgraph_render_static(0);
		GMBase.r_dsgraph_render_dynamic(0);
		Target->disable_aniso();

		// Mid pass probe: did the geometry of this pass land in THIS set's targets at the
		// moment it was submitted? An empty reading here means the pass drew nothing;
		// content here that is gone by the end of the pass means a later phase wiped it.
		{
			static u32 s_midProbe_at = 0;
			if (svpcam::s_svpDebug > 0 && pipOn && Device.dwFrame >= s_midProbe_at)
			{
				if (view == 0)
					s_midProbe_at = Device.dwFrame + 240;
				Msg("* [PIP] mid view=%d set=%s calls=+%u verts=+%u",
					view, lensPass ? "svp" : "main",
					RCache.stat.calls - rcCalls0, RCache.stat.verts - rcVerts0);
				svp_probe_rt(view == 0 ? "mid dry albedo" : (lensPass ? "mid pub albedo" : "mid main albedo"), Target->rt_Color);
				svp_probe_rt(view == 0 ? "mid dry position" : (lensPass ? "mid pub position" : "mid main position"), Target->rt_Position);
			}
		}
	}

	//  Redotix99: for 3D Shader Based Scopes 	
	if (scope_3D_fake_enabled)
	{
		ID3D11Resource* zbuffer_res;
		Target->u_SceneDepth()->GetResource(&zbuffer_res);
		HW.pContext->CopyResource(RImplementation.Target->rt_tempzb->pSurface, zbuffer_res);
		zbuffer_res->Release();
	}

	if (RImplementation.o.dx10_msaa)
		RCache.set_ZB(RImplementation.Target->rt_MSAADepth->pZRT);

	{
		PIX_EVENT(DEFER_TEST_LIGHT_VIS);
		//******* Occlusion testing of volume-limited light-sources
		Target->phase_occq();
		LP_normal.clear();
		LP_pending.clear();
		GMBase.r_dsgraph_capture_lights();
	}

	//******* Main render :: PART-1 (second)
	{
		PIX_EVENT_C(DEFER_PART1_SPLIT, dx10_marker_gbuffer);
		// level
		Target->phase_scene_begin();
		// PiP: the lens content is the world and nothing else. Drawing the first person
		// model here put the optic - and therefore the lens material, which samples the
		// second view - inside the second view, so the lens contained another lens, which
		// contained another: the nested rings the player described, one per magnification
		// step, and the reason the lens looked like a dark copy of the screen with a
		// magnified reticle instead of a magnified world. The optic and the reticle belong
		// to the main pass, which draws them over the lens image exactly like the stock
		// scope does.
		if (!svpLensViewPass)
		{
			GMBase.r_dsgraph_capture_hud();
			// DLSS: move the PDA/glass items out of the scene so they are not upscaled.
			// phase_dlss_postprocess always runs on the complex post-process path
			// (PP_Complex is forced on in phase_combine), so the display-resolution
			// pass that redraws these items is always there when DLSS is configured.
			if (dlss::Configured())
				GMBase.r_dsgraph_extract_native_ui();
			GMBase.r_dsgraph_render_hud();
		}
		GMBase.r_dsgraph_render_lods(true,true);
		if (Details) Details->Render();
		Target->phase_scene_end();
	}

	render_stats::Add(render_stats::GBuffer, render_stats::MsSince(statsMark));
	statsMark = CPU::QPC();

	// Wall marks
	if (Wallmarks)
	{
		PIX_EVENT(DEFER_WALLMARKS);
		Target->phase_wallmarks();

		Wallmarks->Render(); // wallmarks has priority as normal geometry
	}

	// full screen pass to mark msaa-edge pixels in highest stencil bit
	if (RImplementation.o.dx10_msaa)
	{
		PIX_EVENT(MARK_MSAA_EDGES);
		Target->mark_msaa_edges();
	}

	//	TODO: DX10: Implement DX10 rain.
	if (ps_r2_ls_flags.test(R3FLAG_DYN_WET_SURF))
	{
		PIX_EVENT(DEFER_RAIN);
		render_rain();
	}

	{
		// Save previus and current matrices
		{
			static Fmatrix mm_saved_viewproj;

			if (!Device.m_SecondViewport.m_render_pass_is_svp)
			{
				Target->Matrix_previous.mul(mm_saved_viewproj, Device.mInvView);
				Target->Matrix_current.set(Device.mProject);
				mm_saved_viewproj.set(Device.mFullTransform);
			}
		}

		if (RImplementation.o.ssfx_sss && !Device.m_SecondViewport.m_render_pass_is_svp)
		{
			static bool sss_rendered, sss_extended_rendered;

			// SSS Shadows
			if (ps_ssfx_sss_quality.z > 0)
			{
				Target->phase_ssfx_sss();
				sss_rendered = true;
			}
			else
			{
				if (sss_rendered) // Clear buffer
				{
					sss_rendered = false;
					FLOAT ColorRGBA[4] = { 1,1,1,1 };
					HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss->pRT, ColorRGBA);
				}
			}

			if (ps_ssfx_sss_quality.w > 0)
			{
				// Extra lights
				Target->phase_ssfx_sss_ext(RImplementation.LP_normal);
				sss_extended_rendered = true;
			}
			else
			{
				if (sss_extended_rendered) // Clear buffer
				{
					sss_extended_rendered = false;
					FLOAT ColorRGBA[4] = { 1,1,1,1 };
					HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss_tmp->pRT, ColorRGBA);
				}
			}
		}
	}

	// Directional light - fucking sun
	if (bSUN) //bSUN && Device.dwFrame & 1 --Delayed sun update. Worth to check it in future
	{
		PIX_EVENT_C(DEFER_SUN, dx10_marker_lights);
		RImplementation.stats.l_visible ++;
		render_sun_cascades();
		Target->increment_light_marker();
		Target->accum_direct_blend();
	}

	phase = PHASE_NORMAL;

	{
		PIX_EVENT_C(DEFER_SELF_ILLUM, dx10_marker_lights);
		Target->phase_accumulator();
		// Render emissive geometry, stencil - write 0x0 at pixel pos
		RCache.set_xform_project(Device.mProject);
		RCache.set_xform_view(Device.mView);
		// Stencil - write 0x1 at pixel pos - 
		if (!RImplementation.o.dx10_msaa)
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0xff, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
			                   D3DSTENCILOP_KEEP);
		else
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
			                   D3DSTENCILOP_KEEP);
		//RCache.set_Stencil				(TRUE,D3DCMP_ALWAYS,0x00,0xff,0xff,D3DSTENCILOP_KEEP,D3DSTENCILOP_REPLACE,D3DSTENCILOP_KEEP);
		RCache.set_CullMode(CULL_CCW);
		RCache.set_ColorWriteEnable();
		GMBase.r_dsgraph_render_emissive(RImplementation.o.ssfx_bloom ? false : true);
	}

	if (RImplementation.o.ssfx_bloom)
	{
		// Render Emissive on `rt_ssfx_bloom_emissive`
		FLOAT ColorRGBA[4] = { 0,0,0,0 };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_bloom_emissive->pRT, ColorRGBA);
		Target->u_setrt(Target->rt_ssfx_bloom_emissive, NULL, NULL, !RImplementation.o.dx10_msaa ? Target->u_SceneDepth() : Target->rt_MSAADepth->pZRT);
		// Keep regular HUD geometry out of the emissive target. It is already
		// present in the scene texture sampled by the bloom build pass; drawing it
		// here again treats every strict-sorted HUD surface as an emissive source.
		GMBase.r_dsgraph_render_emissive(true, false);
	}

	// Lighting, non dependant on OCCQ
	{
		PIX_EVENT_C(DEFER_LIGHT_NO_OCCQ, dx10_marker_lights);
		Target->phase_accumulator();
		render_lights(LP_normal);
	}

	// Lighting, dependant on OCCQ
	{
		PIX_EVENT_C(DEFER_LIGHT_OCCQ, dx10_marker_lights);
		render_lights(LP_pending);
	}

	{
		if (RImplementation.o.ssfx_volumetric)
			Target->phase_ssfx_volumetric_blur();
	}

	phase = PHASE_NORMAL;

	render_stats::Add(render_stats::Lights, render_stats::MsSince(statsMark));
	statsMark = CPU::QPC();

	// Postprocess
	{
		PIX_EVENT_C(DEFER_LIGHT_COMBINE, dx10_marker_lights);
		Target->phase_combine();
	}

	if (Details)
		Details->details_clear();

	render_stats::Add(render_stats::Combine, render_stats::MsSince(statsMark));

		// Binding evidence per pass: which set the renderer thinks is active, what the
		// device extent is and what the upscaler thinks the render extent is. A mismatch
		// here explains a pass that draws into something other than this set's targets.
		if (svpcam::s_svpDebug > 0 && (lensPass || viewCount > 1))
		{
			Msg("* [PIP] pass view=%d%s camera=%s active=%s dev=%ux%u dlss=%d/%ux%u rsw=%u set=%ux%u mag=%.2f heat=%.0f nvg=%.0f",
				view, lensPass ? (publish ? "(lens,publish)" : "(lens,dry)") : "(main)",
				publish ? "lens" : "main",
				(RImplementation.Target == (lensPass ? (CRenderTarget*)TargetSVP : (CRenderTarget*)TargetMain)) ? "this" : "OTHER",
				Device.dwWidth, Device.dwHeight, dlss::Configured() ? 1 : 0,
				dlss::RenderWidth(), dlss::RenderHeight(),
				// The extent the pass actually renders the scene at: RenderScreenWidth() is the
				// upscaler's internal size for the main view but the target's own extent for the
				// second viewport, so this field is what tells the two apart (the lens texture is
				// display sized either way, which is why the texture dimensions alone cannot).
				RenderScreenWidth(),
				lensPass ? (TargetSVP ? TargetSVP->Width : 0) : (TargetMain ? TargetMain->Width : 0),
				lensPass ? (TargetSVP ? TargetSVP->Height : 0) : (TargetMain ? TargetMain->Height : 0),
				svpcam::s_lastMag, ps_r2_heatvision, ps_r2_nightvision);
		}

		// Probe throttle shared by BOTH passes so the lens and the main set are read in the
		// same frames and their numbers can be compared directly.
		static u32 s_lensProbe_at = 0;

		if (lensPass && TargetSVP && TargetSVP->rt_Generic_0)
		{
			// Probe after every lens run so the discarded main-camera run and the
			// published lens-camera run can be compared directly.
			if (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_lensProbe_at)
			{
				if (view == 0)
					s_lensProbe_at = Device.dwFrame + 240;
				Msg("* [PIP] lens mag=%.3f fFOV=%.3f lensFov=%.3f hdr10=%d",
					svpcam::s_lastMag, Device.fFOV, Device.m_SecondViewport.svp_lens_fov,
					RImplementation.o.dx11_hdr10 ? 1 : 0);
				svp_probe_composite();
				svp_probe_rt(view == 0 ? "dry(generic0)" : "pub(generic0)", TargetSVP->rt_Generic_0);
				svp_probe_rt(view == 0 ? "dry rt_Color" : "pub rt_Color", TargetSVP->rt_Color);
				svp_probe_rt(view == 0 ? "dry rt_Position" : "pub rt_Position", TargetSVP->rt_Position);
				svp_probe_rt(view == 0 ? "dry rt_Accum" : "pub rt_Accum", TargetSVP->rt_Accumulator);
			}
		}
		else if (TargetMain && TargetMain->rt_Generic_0)
		{
			// The MAIN set, same frame, same throttle, same targets. This is the measurement
			// that separates the two explanations for a lens that reads dark: a lens render
			// whose lighting really is weaker (the rt_Accum values differ) against a lens
			// render that is only missing the post process tail, which the lens never runs
			// (rt_Accum matches, generic0 does not). The averages are content weighted - a
			// narrow lens FOV is mostly world while the main view is half sky - so the albedo
			// and the accumulator are the comparable numbers, not the combined images.
			if (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_lensProbe_at && Device.dwFrame > 0)
			{
				svp_probe_rt("main(generic0)", TargetMain->rt_Generic_0);
				svp_probe_rt("main rt_Color", TargetMain->rt_Color);
				svp_probe_rt("main rt_Accum", TargetMain->rt_Accumulator);
			}
		}

		if (publish && TargetSVP && TargetSVP->rt_Generic_0)
		{
			// Publish the lens pass's scene result where the lens material really samples
			// it. Two facts fix the naming here (r4_rendertarget.cpp):
			//  - createUnique() suffixes every second set RT with the set name, so
			//    TargetSVP->rt_secondVP is "$user$viewport2$<set>" - a texture no shader
			//    reads;
			//  - the lens material resolves the un-suffixed "$user$viewport2", which is
			//    the MAIN set's rt_secondVP - exactly the surface the legacy
			//    RenderToTarget(rtSVP) fills (see the switch below).
			CRenderTarget* lensDst = TargetMain ? TargetMain : Target;
			if (lensDst && lensDst->rt_secondVP && TargetSVP->rt_Generic_0->pSurface)
			{
				const bool probeNow = (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_publishProbeAt);
				if (probeNow)
				{
					s_publishProbeAt = Device.dwFrame + 600;
					Msg("* [PIP] publish thermal=%d mode=%d mag=%.2f lensFov=%.3f",
						Device.m_SecondViewport.svp_optic_thermal ? 1 : 0,
						Device.m_SecondViewport.svp_optic_thermal_mode,
						svpcam::s_lastMag, Device.m_SecondViewport.svp_lens_fov);
					// Measure the projection instead of judging it by eye: map a few main-view
					// NDC points through scope_lense_reproject (the same maths the lens shader
					// does) and report the UVs they land on. A lens that samples the magnified
					// patch maps the centre to 0.5 and a small offset to a small delta; one that
					// maps the whole texture lands near 0 and 1 instead.
					if (svp_vp_lens_valid)
					{
						Fmatrix m = svp_vp_lens;
						// Two readings, because whether this engine's set_c() uploads an Fmatrix
						// as is or transposed decides which one the pack's mul(M, v) sees. The
						// correct reading is the one whose spans are about 1/mag, so the log
						// answers the question instead of a guess deciding it.
						auto mapNdc = [&](float x, float y, bool transposed, float& outU, float& outV) {
							Fvector4 v, c;
							v.set(x, y, 1.f, 1.f);
							if (transposed)
							{
								c.x = m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14 * v.w;
								c.y = m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24 * v.w;
								c.w = m._41 * v.x + m._42 * v.y + m._43 * v.z + m._44 * v.w;
							}
							else
							{
								c.x = m._11 * v.x + m._21 * v.y + m._31 * v.z + m._41 * v.w;
								c.y = m._12 * v.x + m._22 * v.y + m._32 * v.z + m._42 * v.w;
								c.w = m._14 * v.x + m._24 * v.y + m._34 * v.z + m._44 * v.w;
							}
							const float inv = (c.w > 1e-6f) ? 1.f / c.w : 0.f;
							outU = c.x * inv * 0.5f + 0.5f;
							outV = 0.5f - c.y * inv * 0.5f;
						};
						// Acceptance: the published mapping must be the identity, i.e. the
						// aperture pixel at screen uv samples the lens texture at the same uv.
						// Probe the centre and a half-NDC offset; identity answers (0.5,0.5)
						// and (0.75,0.5) in both row and column reading, so this line also
						// catches a matrix that is a scale or that is transposed.
						float au, av, bu, bv;
						mapNdc(0.f, 0.f, false, au, av);
						mapNdc(0.5f, 0.f, false, bu, bv);
						Msg("* [PIP] reproject centre=(%.3f,%.3f) ndc(0.5)=(%.3f,%.3f) [want (0.500,0.500) and (0.750,0.500) = identity, mag=%.3f]",
							au, av, bu, bv, svpcam::s_lastMag);
					}
				}
				// Publish the lens pass's COMBINED image, before any post processing, exactly
				// as the 3DSS path publishes $user$generic_temp. The lens shader grades it
				// itself (models_scope_reticle.ps applies grade_second_vp unless the pack's
				// imaging flags are on). The engine's post process chain stays with the main
				// pass: it is display sized, it ends in the temporal upscaler, and running it
				// twice per frame is what left the upscaler state invalid. The exposure the
				// lens combine needs is taken from the main set instead (phase_combine), since
				// that chain is also what computes it.
				HW.pContext->CopyResource(lensDst->rt_secondVP->pSurface, TargetSVP->rt_Generic_0->pSurface);
				svp_lens_rendered = true;

				// Diagnostic dump (r__svp_debug): write the images out as BMP so the lens can
				// be looked at directly instead of inferred from numbers. The lens texture is
				// what the optic samples; generic0 is its source; the accumulator shows
				// whether the lighting ran; the main set's generic0 is a reference frame.
				static u32 s_dumpAt = 0;
				// Level 1 dumps the first instance and then rarely (the files are tens of
				// megabytes); level 2 dumps every instance while an optic is up.
				const u32 interval = (svpcam::s_svpDebug >= 2) ? 600u : 6000u;
				if (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_dumpAt)
				{
					s_dumpAt = Device.dwFrame + interval;
					string_path p;
					FS.update_path(p, "$logs$", "pip_lens.bmp");        svp_dump_rt(p, lensDst->rt_secondVP);
					FS.update_path(p, "$logs$", "pip_src.bmp");         svp_dump_rt(p, TargetSVP->rt_Generic_0);
					FS.update_path(p, "$logs$", "pip_accum.bmp");       svp_dump_rt(p, TargetSVP->rt_Accumulator);
					FS.update_path(p, "$logs$", "pip_albedo.bmp");      svp_dump_rt(p, TargetSVP->rt_Color);
					if (TargetMain)
					{
						FS.update_path(p, "$logs$", "pip_main.bmp");    svp_dump_rt(p, TargetMain->rt_Generic_0);
					}
				}
			}
		}
	}	// end of the per-view loop

	render_stats::SetSuppress(false);

	// PiP: the main set stays active for the rest of the frame (HUD, PP, UI). Reaching
	// this with the lens still active would mean the frame ended on the second set and
	// every display-sized consumer would have seen the lens extent - that is a state
	// leak, so say so instead of silently restoring (the main pass is always last).
	if (viewCount > 1 && TargetMain && Target != TargetMain)
	{
		TargetMain->SetActive(true);
		Msg("! [PIP] frame ended on the lens target set; main set restored (state leak)");
	}

	render_stats::Publish(render_stats::MsSince(statsFrameStart));
}
#include "../xrRender/CHudInitializer.h"

void CRender::render_forward()
{
	const u64 statsHud = CPU::QPC();
	RImplementation.o.distortion = RImplementation.o.distortion_enabled; // enable distorion

	//******* Main render - second order geometry (the one, that doesn't support deffering)
	//.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
	{
		// level
		phase = PHASE_NORMAL;
		//	Igor: we don't want to render old lods on next frame.
		GMBase.r_dsgraph_render_static(1); // normal level, secondary priority
		CParticlesAsync::Wait();
		GMBase.r_dsgraph_render_dynamic(1);
		GMBase.fade_render(); // faded-portals
		GMBase.r_dsgraph_render_sorted(false); // strict-sorted geoms
		g_pGamePersistent->Environment().RenderLast(); // rain/thunder-bolts

		// Item and camera attachment UI draws before the sorted HUD so nearer glass and the post chain cover it
		if (g_hud)
		{
			PROF_EVENT("render_hud");
			// With the upscaler on these widgets are drawn in the display-resolution
			// pass instead, so the reconstruction does not magnify their pixels.
			if (!dlss::Configured() && g_hud->RenderActiveItemUIQuery())
				GMBase.r_dsgraph_render_hud_ui();
			if (!dlss::Configured() && g_hud->RenderCamAttachedUIQuery())
				GMBase.r_dsgraph_render_cam_ui();
		}

		GMBase.r_dsgraph_render_sorted_hud();
	}

	RImplementation.o.distortion = FALSE; // disable distorion
	render_stats::Add(render_stats::HudSub, render_stats::MsSince(statsHud));
}

// Redotix99: for 3D Shader Based Scopes
void CRender::render_Reticle()
{
	VERIFY(0 == GMBase.RGraph.mapHUDSorted.Distort.size() + GMBase.RGraph.mapStaticSorted.Distort.size() + GMBase.RGraph.mapDynamicSorted.Distort.size());
	RImplementation.o.distortion = RImplementation.o.distortion_enabled;

	GMBase.r_dsgraph_render_ScopeSorted();

	RImplementation.o.distortion = FALSE;
}

void CRender::RenderToTarget(RRT target)
{
	ref_rt* RT = nullptr;

	switch (target)
	{
	case rtPDA:
		// PiP: the PDA/glass belong to the main view, never to the second set
		RT = &(TargetMain ? TargetMain : Target)->rt_ui_pda;
		break;
	case rtSVP:
		// PiP (T3 #6): only skip the legacy swapchain copy when this frame really
		// rendered the lens into the second target set and published it into
		// $user$viewport2; otherwise (e.g. the mod drives PiP itself) the copy is the
		// only thing that fills the lens texture, and skipping it shows a black disc
		if (compat::r__svp_same_frame && svp_lens_rendered)
			return;
		RT = &(TargetMain ? TargetMain : Target)->rt_secondVP;
		break;
	default:
		Debug.fatal(DEBUG_INFO, "None or wrong Target specified: %i", target);
		break;
	}

	ID3DTexture2D* pBuffer = nullptr;
	HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
	HW.pContext->CopyResource((*RT)->pSurface, pBuffer);
	pBuffer->Release();
}
