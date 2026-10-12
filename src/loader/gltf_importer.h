#pragma once

#include "importer.h"

namespace tr::Loading {
    class GLTFImporter : public Importer {
    public:
        void load(
            tr::Data::Scene& scene,
            tr::Rendering::RenderingResources& resources,
            const std::filesystem::path& path
        ) const override;
    };
}
