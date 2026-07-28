#pragma once

#include <filesystem>
#include <string>
#include <memory>

#include "loader.h"

namespace tr::Loading {
    class Importer {
    public:
        virtual ~Importer() = default;

        virtual void load(
            tr::Data::Scene& scene,
            LoadContext& context,
            const std::filesystem::path& path
        ) const = 0;
    };
}
