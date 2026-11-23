#pragma once

#include "scene_builder.h"

#include <span>
#include <string_view>

namespace tr::Loading {
    struct DemoScene {
        std::string_view id;
        std::string_view name;
        void (*build)(SceneBuilder& builder);
    };

    std::span<const DemoScene> getDemoScenes();

    void createSimpleExampleScene(SceneBuilder& builder);
    void createMetalRoughGridScene(SceneBuilder& builder);
    void create3DGridScene(SceneBuilder& builder);
    void createTransparencyScene(SceneBuilder& builder);
    void createVertexColorsScene(SceneBuilder& builder);
}
