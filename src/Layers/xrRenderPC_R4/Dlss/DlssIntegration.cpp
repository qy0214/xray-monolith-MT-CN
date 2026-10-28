// MT-CN: written in this fork for the Anomaly engine. It drives the NVIDIA NGX SDK
// (sdk/DLSS, "NVIDIA RTX SDKs LICENSE", SPDX LicenseRef-NvidiaProprietary - its notices must
// not be stripped) and depends on a mod-side shader contract: the motion-vector layout comes
// from SSFX by ascii1457 (r3\screenspace_mvectors.h, https://www.moddb.com/mods/stalker-anomaly/
// addons/screen-space-shaders) and the screen-glass meshes named below belong to the
// Wearable Devices mod. Full ledger: docs/THIRD_PARTY_PROVENANCE.md.
#include "stdafx.h"
#include "DlssIntegration.h"
#include "DlssD3D11.h"
#include "Fsr3D3D11.h"
#include "../../../xrEngine/igame_persistent.h"
#include "../../../xrEngine/RendererError.h"

// The PiP renderer's diagnostic level (r4_R_render.cpp). Declared here so the stable overlay
// diagnostic below costs nothing unless a diagnostic session asks for it.
namespace svpcam { extern int s_svpDebug; }

namespace
{
dlss::D3D11Backend backend;
fsr3::D3D11Backend fsrBackend;
bool useFsr = false;
Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepth;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> sceneDepthView;
dlss::Size renderSize;
dlss::Offset rasterJitter;
dlss::Offset previousJitter;
bool inScene = false, mainView = false, configured = false;
unsigned jitterIndex = 0;
Fvector lastPosition, lastDirection;
float lastFov = 0;
bool haveCamera = false;
bool reportedEvaluation = false;
string512 backendError = {};
float scopeScale = 0.f;		// scope-aware render scale override, 0 = off

const char* Method() { return useFsr ? "FSR3" : "DLSS"; }
void LogBackendError(const char* method, const char* stage, unsigned result)
{
    xr_sprintf(backendError, "%s (0x%08X)", stage, result);
    Msg("! [%s] %s", method, backendError);
    string128 summary;
    xr_sprintf(summary, "[%s] Renderer error", method);
    SetRendererError(summary, backendError, "See the console/log for details. Reapply Upscaling or switch it Off in Video settings.");
}
void LogDlss(const char* stage, unsigned result) { LogBackendError("DLSS", stage, result); }
void LogFsr(const char* stage, unsigned result) { LogBackendError("FSR3", stage, result); }
void ActivationFailed(const char* reason, const char* recovery)
{
    string128 summary;
    xr_sprintf(summary, "[%s] NOT ACTIVE - rendering at native resolution", Method());
    Msg("! %s: %s", summary, reason);
    Msg("! [%s] %s", Method(), recovery);
    SetRendererError(summary, reason, recovery);
}
const char* BackendReason(const char* fallback) { return backendError[0] ? backendError : fallback; }
void ResetHistory() { backend.ResetHistory(); fsrBackend.ResetHistory(); }
// Device views (heat vision, night vision) rebuild the image every frame from an
// animated noise field plus screen-locked lens math, which the temporal upscaler
// cannot align - it would smear it and trail behind camera motion. While one of
// them is active the jitter is dropped and the history is invalidated every
// frame, so the upscale stays spatial and stable instead of ghosting.
bool StableOverlaysActive()
{
    // Device views (heat vision, night vision) rebuild the image every frame from
    // an animated noise field, which the temporal upscaler cannot align.
    if (ps_r2_heatvision > 0 || ps_r2_nightvision > 0) return true;
    // A second viewport optic (PiP scope) shows a deliberately frame-delayed copy
    // of the world inside its lens: reprojecting that stale image with the scene
    // motion vectors is what leaves the heavy trailing while panning.
    if (Device.m_SecondViewport.IsSVPActive()) return true;
    // Any optic is screen locked while the player is actually aiming: the sight's image - lens or
    // reticle - is drawn in the weapon layer and does not follow the scene motion vectors, so the
    // upscaler smears it and trails it behind camera motion.
    //
    // The clause above covers every optic whose lens this engine renders, which is why ordinary
    // magnifying sights never ghosted. It does NOT cover an optic whose lens the engine WITHHELD:
    // the thermal sights keep the 3DSS path (see svp_scope_state.h), so IsSVPActive() is false for
    // them and they fell through to the magnification test below. That test cannot work: this
    // pack's optics report factors at or below 1 - the log line for an M82 carrying a sight reads
    // rtZoom=0.0 - so a screen type thermal sight was treated as a stable overlay, reprojected with
    // the scene motion vectors, and trailed as a ring that only exists while panning. The player
    // narrowed it to thermal sights alone, which is exactly this case, so that is all this adds:
    // "the optic facts say a lens applies, but the engine is not rendering one". Iron sight aiming
    // has no lens data and stays excluded, as do the sights already handled above.
    if (Device.m_SecondViewport.svp_optic_magnifies && Device.m_SecondViewport.svp_lens_fov > 1.0f)
        return true;

    // ...and the gate below is the per frame aiming signal, not the optic facts alone: the facts
    // are only published while the weapon is the active item (Weapon.cpp UpdateSecondVP returns
    // early otherwise), so they keep their last values after the weapon is put away, and a
    // predicate built on them alone would stay true forever.
    if (!g_pGamePersistent) return false;
    const auto* constants = g_pGamePersistent->m_pGShaderConstants;
    if (constants->hud_params.x <= 0.05f) return false;
    if (Device.m_SecondViewport.svp_optic_magnifies && Device.m_SecondViewport.svp_lens_fov > 1.0f)
        return true;
    return constants->hud_fov_params.x > 1.25f;
}
}

// The upscaler render size belongs to the MAIN viewport only; during an SVP pass
// every consumer must read the live Device dims instead.
static bool SvpPassBound() { return Device.true_pip_on && Device.m_SecondViewport.m_render_pass_is_svp; }
unsigned RenderScreenWidth()
{
	if (inScene && !SvpPassBound())
		return dlss::RenderWidth();
	// C2 pilot: prefer the explicit pass extent the active target published, falling
	// back to the device dimensions (their values are identical today).
	return Device.m_SecondViewport.svp_pass_width ? Device.m_SecondViewport.svp_pass_width : Device.dwWidth;
}
unsigned RenderScreenHeight()
{
	if (inScene && !SvpPassBound())
		return dlss::RenderHeight();
	return Device.m_SecondViewport.svp_pass_height ? Device.m_SecondViewport.svp_pass_height : Device.dwHeight;
}
void RenderUpscalerStats(unsigned& sceneWidth, unsigned& sceneHeight, unsigned& scopePercent)
{
    sceneWidth = renderSize.Valid() ? renderSize.width : Device.dwWidth;
    sceneHeight = renderSize.Valid() ? renderSize.height : Device.dwHeight;
    scopePercent = scopeScale > 0.f ? unsigned(scopeScale + 0.5f) : 0u;
}

namespace dlss
{
void SetScopeRenderScale(float percent)
{
    scopeScale = clampr(percent, 0.f, 100.f);
}

void InitializeTargets()
{
    // Tripwire: a re-init while a scene is in progress means some mid-frame path (e.g. an
    // SVP scope target create/destroy) still reaches here; that frame loses the temporal
    // upscale and its viewports fall back to native dims (visible one-frame crop).
    if (inScene)
        Msg("! [Upscaler] reconfiguration requested mid-frame (frame=%u); one frame may fall back to spatial upscale", Device.dwFrame);
    ReleaseTargets();
    renderSize = {Device.dwWidth, Device.dwHeight};
    backendError[0] = 0;
    if (!ps_r_upscaler || (ps_r_upscaler == 1 && !ps_r_dlss_quality))
    {
        SetRendererError(nullptr);
        Msg("[Upscaler] Off: rendering at native resolution.");
        return;
    }
    useFsr = ps_r_upscaler == 2;
    string_path marker;
    // The upscaler cannot work without these loose shaders: screenspace_mvectors.h is the SSFX
    // (ascii1457) motion-vector convention this engine reads, the rest are this fork's own DLSS
    // contract files, shipped in MT-CN. A missing one means the mod set changed, not that the
    // engine is broken, so refuse the upscaler with the name instead of rendering garbage.
    for (const char* shader : {"r3\\screenspace_mvectors.h", "r3\\dlss_contract.h", "r3\\dlss_ui_depth.ps",
        "r3\\dlss_copy.ps", "r3\\dlss_sharpen.ps", "r3\\dlss_cas.h"})
    {
        if (FS.exist(marker, "$game_shaders$", shader)) continue;
        string256 reason;
        xr_sprintf(reason, "Required shader is missing: %s", shader);
        ActivationFailed(reason, "Install the shader compatibility patch matching this engine, then restart the game.");
        return;
    }
    if (!RImplementation.o.ssfx_motionvectors)
    {
        ActivationFailed("SSS motion vectors are unavailable.",
            "Install the matching SSS 23 shader compatibility patch, then restart the game.");
        return;
    }
    if (RImplementation.o.dx10_msaa)
    {
        ActivationFailed("MSAA is enabled and cannot be combined with temporal upscaling.",
            "Turn MSAA Off in Video settings and Apply again (or run vid_restart).");
        return;
    }
    Size size;
    const Size display{Device.dwWidth, Device.dwHeight};
    // Scope override: render closer to the display size while a magnifying optic
    // magnifies the frame, so the magnified image keeps its detail.
    const bool scopeSize = scopeScale > 0.f;
    if (scopeSize)
    {
        const float k = scopeScale * 0.01f;
        size.width = clampr(u32(float(display.width) * k + 0.5f), u32(16), display.width);
        size.height = clampr(u32(float(display.height) * k + 0.5f), u32(16), display.height);
    }
    if (useFsr)
    {
        // Release NGX as well when switching methods; FSR does not require NVIDIA.
        backend.Shutdown();
        if (!scopeSize && !fsr3::D3D11Backend::OptimalSize(ps_r_fsr3_quality, display, size))
        {
            ActivationFailed("Could not determine the FSR 3 render resolution.",
                "Select a valid display resolution and FSR 3 quality, then Apply again.");
            return;
        }
        if (!fsrBackend.Create(HW.pDevice, size, display, RImplementation.o.dx11_hdr10, LogFsr))
        {
            ActivationFailed(BackendReason("FSR 3 context creation failed."),
                "FSR 3 requires Direct3D feature level 11.1. Check the console/log, then reapply or switch Upscaling Off.");
            return;
        }
    }
    else
    {
        string_path cache;
        FS.update_path(cache, "$app_data_root$", "");
        wchar_t wideCache[MAX_PATH] = {};
        if (!MultiByteToWideChar(CP_ACP, 0, cache, -1, wideCache, MAX_PATH))
        {
            LogDlss("Cannot convert the application-data path for NGX", GetLastError());
            ActivationFailed(backendError, "Check the application-data path in fsgame.ltx, then restart the game.");
            return;
        }
        if (!backend.Initialize(HW.pDevice, wideCache, LogDlss))
        {
            ActivationFailed(BackendReason("NVIDIA NGX initialization failed."),
                "Check RTX GPU/driver support and nvngx_dlss.dll beside the executable, then restart the game.");
            return;
        }
        const auto quality = static_cast<Quality>(ps_r_dlss_quality);
        if (!scopeSize && !backend.OptimalSize(quality, display, size))
        {
            ActivationFailed(BackendReason("Could not determine the DLSS render resolution."),
                "Select a valid display resolution and DLSS quality, then Apply again.");
            return;
        }
        if (!backend.Create(quality, size, display, RImplementation.o.dx11_hdr10, static_cast<Preset>(ps_r_dlss_preset)))
        {
            ActivationFailed(BackendReason("DLSS feature creation failed."),
                "Check the console/log and GPU driver/runtime. Reapply Upscaling or switch it Off.");
            return;
        }
    }

    D3D11_TEXTURE2D_DESC depth = {};
    depth.Width = size.width;
    depth.Height = size.height;
    depth.MipLevels = depth.ArraySize = depth.SampleDesc.Count = 1;
    depth.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    D3D11_DEPTH_STENCIL_VIEW_DESC view = {};
    view.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    HRESULT result = HW.pDevice->CreateTexture2D(&depth, nullptr, &sceneDepth);
    if (SUCCEEDED(result)) result = HW.pDevice->CreateDepthStencilView(sceneDepth.Get(), &view, &sceneDepthView);
    if (FAILED(result))
    {
        (useFsr ? LogFsr : LogDlss)("Scene depth allocation failed; retaining native resolution", result);
        ActivationFailed(backendError, "Check GPU memory/device errors in the console/log, then restart the game.");
        ReleaseTargets();
        return;
    }
    renderSize = size;
    configured = true;
    // Once per session: this runs again every time the scope resolution override toggles, which is
    // once per aim (the override raises the internal resolution while an optic is up), so the same
    // two lines were being written on every aim. The first one carries all the information.
    static bool reportedInit = false;
    if (!reportedInit)
    {
        if (useFsr)
            Msg("[FSR3] Initialized; awaiting first scene evaluation: quality=%u scene=%ux%u display=%ux%u",
                ps_r_fsr3_quality, size.width, size.height, display.width, display.height);
        else
            Msg("[DLSS] Initialized; awaiting first scene evaluation: quality=%u requested preset=%u scene=%ux%u display=%ux%u",
                ps_r_dlss_quality, ps_r_dlss_preset, size.width, size.height, display.width, display.height);
        reportedInit = true;
    }
}

void ReleaseTargets()
{
    inScene = mainView = configured = false;
    jitterIndex = 0;
    haveCamera = false;
    reportedEvaluation = false;
    rasterJitter = {};
    previousJitter = {};
    backend.ReleaseFeature();
    fsrBackend.Destroy();
    useFsr = false;
    sceneDepthView.Reset();
    sceneDepth.Reset();
    renderSize = {};
}

void ShutdownDevice() { ReleaseTargets(); backend.Shutdown(); }
unsigned RenderWidth() { return renderSize.Valid() ? renderSize.width : Device.dwWidth; }
unsigned RenderHeight() { return renderSize.Valid() ? renderSize.height : Device.dwHeight; }
bool Configured() { return configured; }
bool Active() { return configured && inScene && mainView && (useFsr ? fsrBackend.Ready() : backend.Ready()); }
Offset RasterJitter() { return rasterJitter; }
Offset PreviousRasterJitter() { return previousJitter; }
ID3DDepthStencilView* SceneDepth() { return inScene && configured ? sceneDepthView.Get() : HW.pBaseZB; }
ID3D11Texture2D* SceneDepthTexture() { return sceneDepth.Get(); }
ID3D11Texture2D* Output() { return useFsr ? fsrBackend.Output() : backend.Output(); }

void BeginScene()
{
    inScene = true;
    if (configured)
    {
        const D3D11_VIEWPORT viewport{0, 0, float(RenderWidth()), float(RenderHeight()), 0, 1};
        HW.pContext->RSSetViewports(1, &viewport);
    }
    // Scope frames must neither consume a jitter sample nor overwrite the history.
    mainView = !Device.m_SecondViewport.m_render_pass_is_svp;
    const bool stableOverlay = StableOverlaysActive();
    // Thermal optic diagnostic, once per episode (not on a timer, and not behind the debug switch):
    // this is the optic whose image ghosts while panning, three hypotheses have been disproved
    // against it (the lens reflection, the zoom factor gated stable overlay clause, the camera
    // motion blur), and the values that decide the engine side of it should be in the log of any
    // session where the artifact is looked at. One line per aim costs nothing.
    {
        const bool thermalOptic = Device.m_SecondViewport.svp_optic_magnifies
            && (Device.m_SecondViewport.svp_optic_image_type >= 2 || Device.m_SecondViewport.svp_optic_thermal);
        static bool lastThermalOptic = false;
        if (thermalOptic != lastThermalOptic)
        {
            // The guards mirror StableOverlaysActive(): BeginScene also runs on
            // menu/main-menu frames where the game persistent state is gone.
            const auto* constants = g_pGamePersistent ? g_pGamePersistent->m_pGShaderConstants : nullptr;
            Msg("* [PIP] thermal optic: unstable=%d svp=%d lensFov=%.2f ads=%.2f zoom=%.2f motionblur=%.2f img=%d",
                stableOverlay ? 1 : 0, Device.m_SecondViewport.IsSVPActive() ? 1 : 0,
                Device.m_SecondViewport.svp_lens_fov,
                constants ? constants->hud_params.x : -1.0f,
                constants ? constants->hud_fov_params.x : -1.0f, ps_ssfx_motionblur.y,
                Device.m_SecondViewport.svp_optic_image_type);
            lastThermalOptic = thermalOptic;
        }
    }
    if (Active())
    {
        // A device view changes the whole image every frame without matching the
        // motion vectors; accumulating it would smear it and trail the camera.
        if (stableOverlay) ResetHistory();
        if (haveCamera && (lastPosition.distance_to_sqr(Device.vCameraPosition) > 64.0f * 64.0f ||
            lastDirection.dotproduct(Device.vCameraDirection) < 0.25f || _abs(lastFov - Device.fFOV) > 10.0f))
        {
            ResetHistory();
            jitterIndex = 0;
        }
        lastPosition = Device.vCameraPosition;
        lastDirection = Device.vCameraDirection;
        lastFov = Device.fFOV;
        haveCamera = true;
        // Match the reference build: two times log2 of the render scale (0.5 -> -2.0),
        // which keeps texture detail at the resolution the upscaler reconstructs to.
        SSManager.SetMipLODBias(ps_r__tf_Mipbias + 2.0f * log2f(float(RenderWidth()) / Device.dwWidth));
    }
    // Single gate for the whole jitter chain (binders, NGX/FSR3 jitterOffset).
    // A device view also drops the jitter: without history it would only shimmer.
    rasterJitter = (Active() && ps_r_upscaler_jitter != 0 && !stableOverlay)
        ? Jitter(jitterIndex++, renderSize, {Device.dwWidth, Device.dwHeight}) : Offset{};
}

void EndScene(bool invalidateHistory)
{
    if (configured && inScene && mainView) previousJitter = rasterJitter;
    if (configured && inScene) SSManager.SetMipLODBias(ps_r__tf_Mipbias);
    inScene = mainView = false;
    rasterJitter = {};
    if (invalidateHistory) { ResetHistory(); jitterIndex = 0; haveCamera = false; previousJitter = {}; }
}

bool Evaluate(ID3D11Texture2D* color, ID3D11Texture2D* motion)
{
    if (!Active()) return false;
    fsr3::D3D11Backend::Frame frame;
    frame.color = color;
    frame.depth = sceneDepth.Get();
    frame.motion = motion;
    frame.jitter = rasterJitter;
    frame.deltaMilliseconds = Device.fTimeDelta * 1000.0f;
    frame.frame = Device.dwFrame;
    // Near/far/FOV from the actual projection, including camera overrides.
    frame.cameraNear = -Device.mProject._43 / Device.mProject._33;
    frame.cameraFar = -Device.mProject._43 / (Device.mProject._33 - 1.0f);
    frame.verticalFov = 2.0f * atanf(1.0f / Device.mProject._22);
    const bool success = useFsr ? fsrBackend.Evaluate(frame) : backend.Evaluate(frame);
    // Both of these repeat once per aim: the evaluation flag is reset when the targets are rebuilt
    // for the scope resolution override, and a failure repeats every frame while it lasts. The log
    // gets the first occurrence of each.
    static bool reportedActiveOnce = false;
    static bool reportedFailureOnce = false;
    if (success && !reportedEvaluation)
    {
        SetRendererError(nullptr);
        if (!reportedActiveOnce)
        {
            Msg("- [%s] ACTIVE - first scene evaluation succeeded: frame=%u, scene=%ux%u, jitter=(%.4f,%.4f), MV scale=(-%u,-%u)",
                Method(), Device.dwFrame, RenderWidth(), RenderHeight(), rasterJitter.x, rasterJitter.y, RenderWidth(), RenderHeight());
            reportedActiveOnce = true;
        }
        reportedEvaluation = true;
        reportedFailureOnce = false;
    }
    else if (!success)
    {
        string128 summary;
        xr_sprintf(summary, "[%s] STOPPED - using spatial upscale without temporal reconstruction", Method());
        const char* reason = BackendReason("Scene evaluation failed.");
        const char* recovery = "Check the console/log. Reapply Upscaling (vid_restart) or switch it Off in Video settings.";
        if (!reportedFailureOnce)
        {
            Msg("! %s: %s", summary, reason);
            reportedFailureOnce = true;
        }
        Msg("! [%s] %s", Method(), recovery);
        SetRendererError(summary, reason, recovery);
    }
    return success;
}
}
