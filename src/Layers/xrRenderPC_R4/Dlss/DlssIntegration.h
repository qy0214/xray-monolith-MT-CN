// MT-CN: written in this fork for the Anomaly engine. It drives the NVIDIA NGX SDK
// (sdk/DLSS, "NVIDIA RTX SDKs LICENSE", SPDX LicenseRef-NvidiaProprietary - its notices must
// not be stripped) and depends on a mod-side shader contract: the motion-vector layout comes
// from SSFX by ascii1457 (r3\screenspace_mvectors.h, https://www.moddb.com/mods/stalker-anomaly/
// addons/screen-space-shaders) and the screen-glass meshes named below belong to the
// Wearable Devices mod. Full ledger: docs/THIRD_PARTY_PROVENANCE.md.
#pragma once
#include "DlssTemporal.h"
#include "../../xrRender/RenderDimensions.h"

namespace dlss
{
// Shared temporal scene integration (historical DLSS name). The selected backend
// is latched at target creation; console/UI changes only apply on vid_restart.
void InitializeTargets();
void ReleaseTargets();
void ShutdownDevice();
void BeginScene();
// Scope-aware internal resolution: percent of the display size to render at while
// a magnifying optic is in use (0 = use the configured quality again).
void SetScopeRenderScale(float percent);
void EndScene(bool invalidateHistory = false);
bool Active();
bool Configured();
unsigned RenderWidth();
unsigned RenderHeight();
Offset RasterJitter();
Offset PreviousRasterJitter();
ID3DDepthStencilView* SceneDepth();
ID3D11Texture2D* SceneDepthTexture();
ID3D11Texture2D* Output();
bool Evaluate(ID3D11Texture2D* color, ID3D11Texture2D* motion);
}
