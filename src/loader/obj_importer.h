#pragma once

#include "importer.h"

namespace tr::Loading {
    class OBJImporter : public Importer {
    public:
        void load(
            tr::Data::Scene& scene,
            LoadContext& context,
            const std::filesystem::path& path
        ) const override;
    };
}
