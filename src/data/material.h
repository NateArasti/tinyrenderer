#pragma once

#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "handle.h"
#include "shader.h"
#include "texture.h"

namespace tr::Data {
    struct Material {
        std::string name;
        Resources::Handle<Data::Shader> shader;
        std::vector<Resources::Handle<Data::Texture>> textures;
        
        explicit Material(Resources::Handle<Data::Shader> shader) : shader(shader) {}
    };
}
