#pragma once

#include <filesystem>
#include <memory>

#include "rendering_resources.h"
#include "scene.h"

namespace tr::Loading {
    class Loader {
    public:
        static void loadDefaultScene(
            tr::Data::Scene& scene,
            tr::Rendering::RenderingResources& resources
        );
        
        static void loadModel(
            tr::Data::Scene& scene,
            tr::Rendering::RenderingResources& resources,
            const std::filesystem::path& path
        );
    };
}
