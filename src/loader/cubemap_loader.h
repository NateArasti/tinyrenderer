#pragma once

#include <filesystem>
#include <memory>

#include "cubemap.h"
#include "rendering_resources.h"

namespace tr::Loading {
    class CubemapLoader {
    public:
        static tr::Data::Handle<Cubemap> loadCubemap(
            const std::filesystem::path& path,
            tr::Rendering::RenderingResources& resources
        );
    };
}
