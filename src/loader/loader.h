#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

#include "handle.h"
#include "scene.h"
#include "shader.h"

namespace tr::Data {
    struct ResourceManager;
}

namespace tr::Loading {
    class Loader {
    public:
        static std::unique_ptr<tr::Data::Scene> loadDefaultScene(
            tr::Data::ResourceManager& resourceManager,
            tr::Resources::Handle<tr::Data::Shader> baseShader
        );

        static std::unique_ptr<tr::Data::Scene> loadModel(
            tr::Data::ResourceManager& resourceManager,
            tr::Resources::Handle<tr::Data::Shader> baseShader,
            std::span<const std::byte> content
        );
    };
}
