#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering {
    struct SceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 lightDirection;
        float lightIntensity;
        glm::vec4 lightColor;
        glm::vec3 cameraPos; 
    };
}
