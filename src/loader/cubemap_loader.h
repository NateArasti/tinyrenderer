#pragma once

#include <filesystem>
#include <memory>

#include "cubemap.h"
#include "rendering_resources.h"

namespace tr::Loading {
    class CubemapLoader {
    public:
        static std::unique_ptr<tr::Data::Cubemap> loadCubemap(
            const std::filesystem::path& path,
            tr::Rendering::RenderingResources& resources
        );
    };
}
