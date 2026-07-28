#pragma once

#include <filesystem>
#include <memory>

#include "handle.h"
#include "scene.h"
#include "shader.h"

namespace tr::Data {
    struct ResourceManager;
}

namespace tr::Loading {
    struct LoadContext {
        Data::ResourceManager& resourceManager;
        Resources::Handle<Data::Shader> baseOpaqueShader;
        Resources::Handle<Data::Shader> baseTransparentShader;
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
