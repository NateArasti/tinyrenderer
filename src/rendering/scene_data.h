#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering {
    struct SceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPos;
        float lightIntensity;
        glm::vec3 lightDirection;
        float _pad0;
        glm::vec4 lightColor;
        glm::mat4 lightViewProj;
    };
}
