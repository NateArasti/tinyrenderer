#include "pbr.h"

#include <glm/glm.hpp>

namespace tr::Data::EmbeddedShaders {
    Pbr::Pbr() : Shader("pbr", getCode()) {
        vertName = "vertMain";
        fragName = "fragMain";
        cullMode = CullMode::Back;
        params = {
            { "diffuseColor", glm::vec4(1) },
            { "ambientColor", glm::vec4(0.03f, 0.03f, 0.03f, 1.0f) },
            { "emissionColor", glm::vec4(0) },
            { "specularColor", glm::vec4(0.04f, 0.04f, 0.04f, 1.0f) },
            { "metallicFactor", 0.0f },
            { "roughnessFactor", 1.0f },
            { "normalScale", 1.0f },
            { "diffuseMap", 0u },
            { "emissionMap", 0u },
            { "normalMap", 0u },
            { "metallicMap", 0u },
            { "roughnessMap", 0u },
            { "specularMap", 0u },
            { "opacityMap", 0u }
        };
    }
}
