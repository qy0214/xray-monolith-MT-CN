#pragma once

// Device dimensions stay at display resolution (UI, input, swapchain). Only the
// DX11 scene changes resolution. Other renderers retain their existing behavior.
#if defined(USE_DX11)
unsigned RenderScreenWidth();
unsigned RenderScreenHeight();
// Current upscaler/render extent plus the scope-resolution override, for the
// render stats overlay. The normal getters depend on being inside a scene.
void RenderUpscalerStats(unsigned& sceneWidth, unsigned& sceneHeight, unsigned& scopePercent);
#else
inline unsigned RenderScreenWidth() { return Device.dwWidth; }
inline unsigned RenderScreenHeight() { return Device.dwHeight; }
inline void RenderUpscalerStats(unsigned& sceneWidth, unsigned& sceneHeight, unsigned& scopePercent)
{
	sceneWidth = Device.dwWidth;
	sceneHeight = Device.dwHeight;
	scopePercent = 0;
}
#endif
