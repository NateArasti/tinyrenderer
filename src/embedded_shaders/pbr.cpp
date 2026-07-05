#include "pbr.h"

#include <glm/glm.hpp>

namespace tr::Data::EmbeddedShaders {
    Pbr::Pbr() : Shader("pbr", getCode()) {
        vertName = "vertMain";
        fragName = "fragMain";
        cullMode = CullMode::None;
        params = {
            { "diffuseColor", glm::vec4(1) },
            { "ambientColor", glm::vec4(0.03f, 0.03f, 0.03f, 1.0f) },
            { "specularColor", glm::vec4(0.5) },
            { "metallicFactor", 0.0f },
            { "roughnessFactor", 1.0f },
            { "albedo", Resources::Handle<Texture>{} }
        };
    }
}
