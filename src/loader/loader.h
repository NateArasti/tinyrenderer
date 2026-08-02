#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "rendering_resources.h"
#include "scene.h"

namespace tr::Loading {
    struct LoadContext {
        tr::Rendering::RenderingResources& resources;
    };

    class ImportError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
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
