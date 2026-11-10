#pragma once

#include <cstdint>

namespace tr::Data {
    struct alignas(16) ProceduralSkyData {
        float density = 1.0f;
        int32_t opticalDepthSamples = 8;
        int32_t scatteringSamples = 16;

        float sunRadius = 0.15f;
        float sunGlowWidth = 0.1f;
        float sunGlowStrength = 0.25f;
        float moonRadius = 0.075f;
        float moonReflectance = 0.75f;
        float moonGlowWidth = 0.01f;
        float moonGlowStrength = 0.1f;

        float starScale = 120.0f;
        float starDensity = 0.075f;
        float starCoreWidth = 0.15f;
        float starGlowWidth = 0.12f;
        float starGlowStrength = 0.04f;
        float starBrightness = 0.15f;
        float starMinBrightness = 0.1f;
        float starBrightnessVariation = 2.0f;
        float starBrightnessFalloff = 8.0f;
    };
}
