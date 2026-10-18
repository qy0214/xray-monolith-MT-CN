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

	// PiP (T3 #6): with the second target set in charge of the lens, the override
	// has nothing left to buy - the scope frame renders at that set's own extent.
	// It stays as the fallback for when the set could not be created (out of VRAM)
	// and for the pure screen-space magnification cases below.
	if (compat::r__svp_same_frame && RImplementation.TargetSVP && Device.m_SecondViewport.IsSVPActive())
		return false;

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

// PiP (T3 #6): the game alternates the camera per frame (the SVP frame carries the
// zoomed one), so the renderer keeps a copy of both cameras instead of deriving the
// optics zoom itself, and applies whichever one a pass needs.
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
	// Smoothed magnification and its target (see DeriveZoom): the lens follows a change of
	// sight power with the same 0.12 s feel the stock camera has.
	float s_magSmooth = 0.f;
	float s_lastMagTarget = 0.f;

	// Live tuning of the lens magnification in percent of the optic's authored value.
	// Some packs leave a sight's zoom range undefined (the engine then reports g_fov as its
	// highest power, i.e. 1x), so the authored lens FOV is the only usable number and the
	// player needs a way to settle the exact power without a new build per guess. Default
	// 100 = exactly what the optic authors.
	int s_lensPowerPct = 100;

	// Diagnostic switch for the lens content probes (staging readbacks, mid pass probes, the
	// per pass binding lines and the image dumps). The probes earned their place while the
	// black lens was being chased, but they cost a readback every few seconds and belong
	// behind a switch. Default 1 while the lens image is still being diagnosed.
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

	// The lens camera: the same eye and HUD projection as the main view, with the
	// projection magnified by the optic's zoom factor (hud_params.y is the second
	// viewport zoom, hud_params.x the ADS zoom). The reference derives a full optics
	// camera here; this covers the visible part of it.
	void DeriveZoom(View& z, const View& m)
	{
		z = m;

		// Preferred: the optic's own lens FOV, published by the game (Weapon.cpp). The
		// modpack tunes those values for the default g_fov of 65 and states the
		// magnification as g_fov / scope_lens_fov, so the reference for the ratio is the
		// game's BASE fov, not the current main view fov: the main view is deliberately
		// kept wide while the lens is up, and using its smoothed value left the lens short
		// of the sight's real power. The ratio is smoothed here because the stock camera
		// smooths its fov as well, and the main view no longer does that for the lens.
		const float lensFov = Device.m_SecondViewport.svp_lens_fov;
		const float baseFov = Device.m_SecondViewport.svp_base_fov > 1.0f
			? Device.m_SecondViewport.svp_base_fov : Device.fFOV;
		if (lensFov > 1.0f && lensFov < 89.0f && baseFov > 1.0f)
		{
			const float target = baseFov / lensFov * (float(s_lensPowerPct) * 0.01f);
			// ~0.12 s time constant, i.e. the same feel as the stock zoom animation. The
			// smoothing must start from the target when the lens was not rendering just
			// before: ramping from zero made the first frames of every aim LESS magnified
			// than the naked eye (mag below 1), which is visibly wrong.
			static u32 s_lastMagFrame = 0;
			if (Device.dwFrame > s_lastMagFrame + 2)
				s_magSmooth = target;
			s_lastMagFrame = Device.dwFrame;

			const float k = 1.0f - expf(-Device.fTimeDelta / 0.12f);
			s_lastMagTarget = target;
			s_magSmooth += (target - s_magSmooth) * k;
			const float mag = s_magSmooth;
			if (mag > 1.001f)
			{
				z.mProject._11 *= mag;
				z.mProject._22 *= mag;
				z.mFullTransform.mul(z.mView, z.mProject);
			}
			s_lastMag = mag;
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

// PiP (T3 #6): parked at file scope so the early second-viewport allocation gate can see
// it. The same-frame lens is compiled in but off: the pack's optics keep their own 3DSS
// path (which works and which the player knows), no second target set is created, the main
// camera keeps its ADS zoom and the mouse keeps its stock ratio. The pieces stay in the
// tree for the eventual port, and are switched on again only once the lens content is
// finished and verified without the player acting as the test rig.
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
	if (SVP_SAME_FRAME_READY && compat::r__svp_same_frame && Device.m_SecondViewport.pip_lens_shaders
		&& Device.m_SecondViewport.svp_optic_magnifies && Device.m_SecondViewport.svp_lens_fov > 1.0f)
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

	// PiP (T3 #6): the game alternates the camera per frame (IsSVPFrame() carries the
	// zoomed one), so keep a copy of both cameras and draw the lens view and the
	// normal view in the same frame, one pass each. The lens is then current, unlike
	// the legacy scheme which copied the swapchain and skipped Present.
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
			lensFov > 1.0f, TargetSVP != nullptr);
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

	// PiP: the lens shaders read ps_r2_heatvision to decide whether to apply heat vision, and
	// the pack marks its thermal sights with scope_lense_thermal - but nothing in the engine
	// ever sets that console variable for an equipped optic, which is why a thermal sight
	// rendered a plain image inside the lens while the stock 3DSS path, where the pack drove
	// the effect itself, showed heat. Enable it on the frames that render a thermal lens and
	// restore the player's own setting afterwards, so a manually enabled heat vision survives.
	{
		static int s_svpHeatRestore = -1;
		const bool wantHeat = Device.m_SecondViewport.svp_scope.lens_rendered
			&& Device.m_SecondViewport.svp_optic_thermal;
		if (wantHeat)
		{
			if (s_svpHeatRestore < 0)
				s_svpHeatRestore = ps_r2_heatvision;
			const int mode = Device.m_SecondViewport.svp_optic_thermal_mode;
			ps_r2_heatvision = (mode > 0) ? mode : 1;
		}
		else if (s_svpHeatRestore >= 0)
		{
			ps_r2_heatvision = s_svpHeatRestore;
			s_svpHeatRestore = -1;
		}
	}

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

		// PiP: publish the current frame's lens camera VP for the shader's projection matrix.
		if (publish && svpcam::Ready())
		{
			svp_vp_lens.mul(svpcam::s_zoom.mProject, svpcam::s_zoom.mView);
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
			Msg("* [PIP] pass view=%d%s camera=%s active=%s dev=%ux%u dlss=%d/%ux%u set=%ux%u mag=%.2f heat=%.0f nvg=%.0f",
				view, lensPass ? (publish ? "(lens,publish)" : "(lens,dry)") : "(main)",
				publish ? "lens" : "main",
				(RImplementation.Target == (lensPass ? (CRenderTarget*)TargetSVP : (CRenderTarget*)TargetMain)) ? "this" : "OTHER",
				Device.dwWidth, Device.dwHeight, dlss::Configured() ? 1 : 0,
				dlss::RenderWidth(), dlss::RenderHeight(),
				lensPass ? (TargetSVP ? TargetSVP->Width : 0) : (TargetMain ? TargetMain->Width : 0),
				lensPass ? (TargetSVP ? TargetSVP->Height : 0) : (TargetMain ? TargetMain->Height : 0),
				svpcam::s_lastMag, ps_r2_heatvision, ps_r2_nightvision);
		}

		if (lensPass && TargetSVP && TargetSVP->rt_Generic_0)
		{
			// Probe after every lens run so the discarded main-camera run and the
			// published lens-camera run can be compared directly.
			static u32 s_lensProbe_at = 0;
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
				// Publish the lens pass's scene result where the lens material really
				// samples it, through the tone map / gamma / colour grading the main view
				// gets (phase_lens_postprocess): the lens pass returns from phase_combine
				// before the post process tail, so a plain blit would publish a linear
				// image that renders nearly black.
				// Evidence for the step: the published texture before and after it, so a
				// post process that does not run (or runs without effect) is visible instead
				// of being assumed.
				const bool probeNow = (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_publishProbeAt);
				if (probeNow)
				{
					s_publishProbeAt = Device.dwFrame + 600;
					Msg("* [PIP] publish thermal=%d mode=%d",
						Device.m_SecondViewport.svp_optic_thermal ? 1 : 0,
						Device.m_SecondViewport.svp_optic_thermal_mode);
					svp_probe_rt("pre postprocess(viewport2)", lensDst->rt_secondVP);
				}
				TargetSVP->phase_lens_postprocess(lensDst->rt_secondVP);
				svp_lens_rendered = true;
				if (probeNow)
					svp_probe_rt("post postprocess(viewport2)", lensDst->rt_secondVP);

				// Diagnostic dump (r__svp_debug): write the images out as BMP so the lens can
				// be looked at directly instead of inferred from numbers. The lens texture is
				// what the optic samples; generic0 is its source; the accumulator shows
				// whether the lighting ran; the main set's generic0 is a reference frame.
				static u32 s_dumpAt = 0;
				if (svpcam::s_svpDebug > 0 && Device.dwFrame >= s_dumpAt)
				{
					s_dumpAt = Device.dwFrame + 600;
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
