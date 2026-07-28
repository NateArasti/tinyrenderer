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
            { "emissionColor", glm::vec4(0) },
            { "specularColor", glm::vec4(0.04f, 0.04f, 0.04f, 1.0f) },
            { "metallicFactor", 0.0f },
            { "roughnessFactor", 1.0f },
            { "normalScale", 1.0f },
            { "hasDiffuseMap", 0.0f },
            { "hasEmissionMap", 0.0f },
            { "hasNormalMap", 0.0f },
            { "hasMetallicMap", 0.0f },
            { "hasRoughnessMap", 0.0f },
            { "hasSpecularMap", 0.0f },
            { "hasOpacityMap", 0.0f },
            { "diffuseMap", Resources::Handle<Texture>{} },
            { "emissionMap", Resources::Handle<Texture>{} },
            { "normalMap", Resources::Handle<Texture>{} },
            { "metallicMap", Resources::Handle<Texture>{} },
            { "roughnessMap", Resources::Handle<Texture>{} },
            { "specularMap", Resources::Handle<Texture>{} },
            { "opacityMap", Resources::Handle<Texture>{} }
        };
    }
}
