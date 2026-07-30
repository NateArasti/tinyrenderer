#pragma once

#include <filesystem>
#include <memory>

#include "rhi.h"
#include "scene.h"

namespace tr::Loading {
    struct LoadContext {
        tr::Rendering::RHI& renderingInterface;
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
