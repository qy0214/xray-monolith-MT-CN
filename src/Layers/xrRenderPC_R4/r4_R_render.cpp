#include "stdafx.h"
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

	Msg("[Upscaler] scope resolution %s: scale=%d%% delay=%ds scene=%ux%u display=%ux%u",
		want ? "on" : "off", ps_r__upscaler_scope_scale, ps_r__upscaler_scope_delay,
		dlss::RenderWidth(), dlss::RenderHeight(), Device.dwWidth, Device.dwHeight);
	return true;
}

extern u32 g_r;

void CRender::Render()
{
	PIX_EVENT_C(CRender_Render, dx10_marker_frame);
	dx10_annotate_frame();
	dlss::EndScene();

	rmNormal();

	compat::Tick();	// refresh engine-owned MOD-compat values (scope_lense_ads_is_pip)

	// pipdlss reads these two flags in the upscaler's SvpPassBound(); this engine
	// refreshes them once per frame from its own SecondVP state (see device.h).
	Device.true_pip_on = Device.m_SecondViewport.IsSVPActive();
	Device.m_SecondViewport.m_render_pass_is_svp = Device.m_SecondViewport.IsSVPFrame();

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
		return;
	}

	if (m_bFirstFrameAfterReset)
	{
		for (light* L : v_all_lights)//critical!!!
			L->m_moving_frames = 0;

		m_bFirstFrameAfterReset = false;
		return;
	}

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

	dlss::BeginScene();
	// Configure
	RImplementation.o.distortion = FALSE; // disable distorion
	Fcolor sun_color = ((light*)Lights.sun_adapted._get())->color;
	BOOL bSUN = ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r, sun_color.g, sun_color.b)>EPS) && !Core.ParamsData.test(ECoreParams::r4_dev);
	if (o.sunstatic) bSUN = FALSE;
	// Msg						("sstatic: %s, sun: %s",o.sunstatic?;"true":"false", bSUN?"true":"false");

	// HOM
	ViewBase.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
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

    GMBase.traverse(RImplementation.pLastSector, ViewBase, Device.vCameraPosition, Device.mFullTransform);
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
		GMBase.r_dsgraph_capture_hud();
		// DLSS: move the PDA/glass items out of the scene so they are not upscaled.
		// phase_dlss_postprocess always runs on the complex post-process path
		// (PP_Complex is forced on in phase_combine), so the display-resolution
		// pass that redraws these items is always there when DLSS is configured.
		if (dlss::Configured())
			GMBase.r_dsgraph_extract_native_ui();
		GMBase.r_dsgraph_render_hud();
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

			if (!Device.m_SecondViewport.IsSVPFrame())
			{
				Target->Matrix_previous.mul(mm_saved_viewproj, Device.mInvView);
				Target->Matrix_current.set(Device.mProject);
				mm_saved_viewproj.set(Device.mFullTransform);
			}
		}

		if (RImplementation.o.ssfx_sss && !Device.m_SecondViewport.IsSVPFrame())
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
	render_stats::Publish(render_stats::MsSince(statsFrameStart));

	// Periodic statistics line: the same numbers as the rs_stats overlay, written
	// to the log every few seconds so a session log carries them for analysis.
	static u32 statsLastLog = 0;
	if (Device.dwTimeGlobal - statsLastLog >= 5000)
	{
		statsLastLog = Device.dwTimeGlobal;
		unsigned sceneW = 0, sceneH = 0, scopePct = 0;
		RenderUpscalerStats(sceneW, sceneH, scopePct);
		const float fps = Device.fTimeDelta > 0.f ? 1.f / Device.fTimeDelta : 0.f;
		const float rfps = render_stats::frameMs > 0.f ? 1000.f / render_stats::frameMs : 0.f;
		Msg("* [STATS] fps=%.1f rfps=%.1f render=%.1fms verts=%u/%u polys=%u/%u dips=%u | scene=%ux%u->%ux%u scope=%u pip=%d heat=%d nvg=%d | trv=%.1f gbuf=%.1f light=%.1f comb=%.1f (hud=%.1f pp=%.1f) other=%.1f",
			fps, rfps, render_stats::frameMs,
			RCache.stat.verts, RCache.stat.calls ? RCache.stat.verts / RCache.stat.calls : 0,
			RCache.stat.polys, RCache.stat.calls ? RCache.stat.polys / RCache.stat.calls : 0,
			RCache.stat.calls,
			sceneW, sceneH, Device.dwWidth, Device.dwHeight, scopePct,
			Device.m_SecondViewport.IsSVPActive() ? 1 : 0, ps_r2_heatvision, ps_r2_nightvision,
			render_stats::sectionMs[render_stats::Traverse], render_stats::sectionMs[render_stats::GBuffer],
			render_stats::sectionMs[render_stats::Lights], render_stats::sectionMs[render_stats::Combine],
			render_stats::sectionMs[render_stats::HudSub], render_stats::sectionMs[render_stats::PpSub],
			render_stats::sectionMs[render_stats::Other]);
	}
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
		RT = &Target->rt_ui_pda;
		break;
	case rtSVP:
		RT = &Target->rt_secondVP;
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
