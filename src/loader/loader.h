#pragma once

#include <filesystem>
#include <memory>

#include "rendering_resources.h"
#include "scene.h"

namespace tr::Loading {
    struct LoadContext {
        tr::Rendering::RenderingResources& resources;
    };

    class Loader {
    public:
        static std::unique_ptr<tr::Data::Scene> loadDefaultScene(LoadContext ctx);
        static std::unique_ptr<tr::Data::Scene> loadModel(
            LoadContext ctx,
            const std::filesystem::path& path
        );
    };
}
