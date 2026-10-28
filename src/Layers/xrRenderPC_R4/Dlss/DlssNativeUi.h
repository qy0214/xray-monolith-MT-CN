// MT-CN: written in this fork for the Anomaly engine. It drives the NVIDIA NGX SDK
// (sdk/DLSS, "NVIDIA RTX SDKs LICENSE", SPDX LicenseRef-NvidiaProprietary - its notices must
// not be stripped) and depends on a mod-side shader contract: the motion-vector layout comes
// from SSFX by ascii1457 (r3\screenspace_mvectors.h, https://www.moddb.com/mods/stalker-anomaly/
// addons/screen-space-shaders) and the screen-glass meshes named below belong to the
// Wearable Devices mod. Full ledger: docs/THIRD_PARTY_PROVENANCE.md.
#pragma once
#include <algorithm>
#include <string_view>

namespace dlss
{
inline bool IsWearableScreenGlass(std::string_view name)
{
    // WearableDevices draws its screen widgets directly, but its glass is a
    // separate sorted mesh. Move that glass too so it blends over the native UI.
    return name == "wd\\promin\\promin_screen_glass" ||
        name == "wd\\promin\\promin_screen_glass_broken" ||
        name == "wd\\vektor\\vektor_screen_glass";
}

inline bool IsNativeHudUiTexture(std::string_view name)
{
    return name == "$user$ui" || IsWearableScreenGlass(name);
}

// The queues retain the original draw packets (including skinning and object
// transforms) until the display-resolution pass in the SAME rendered view.
template<class Queue, class Predicate>
void ExtractNativeUi(Queue& scene, Queue& display, Predicate isUi)
{
    display.clear();
    scene.erase(std::remove_if(scene.begin(), scene.end(), [&](const auto& item)
    {
        if (!isUi(item)) return false;
        display.push_back(item);
        return true;
    }), scene.end());
}

template<class Queue, class DisplayQueue>
void RemoveNativeUiDuplicates(Queue& scene, const DisplayQueue& display)
{
    scene.erase(std::remove_if(scene.begin(), scene.end(), [&](const auto& item)
    {
        return std::any_of(display.begin(), display.end(), [&](const auto& ui)
        {
            return item.pVisual == ui.pVisual && item.pMatrix == ui.pMatrix;
        });
    }), scene.end());
}
}
