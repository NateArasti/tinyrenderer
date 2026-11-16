#pragma once

#include <filesystem>

#include "handle.h"
#include "cubemap.h"

namespace tr::Rendering {
    class RenderingResources;
}

namespace tr::Loading {
    tr::Data::Handle<tr::Data::Cubemap> loadCubemap(
        const std::filesystem::path& path,
        tr::Rendering::RenderingResources& resources
    );
}
