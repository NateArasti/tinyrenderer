#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering {
    struct alignas(16) EnvironmentData {
        glm::vec3 clearColor;
        float skyboxRotation;
        glm::vec3 skyboxTint;
        float skyboxExposure;

        glm::vec3 lightDirection;
        float lightIntensity;
        glm::vec4 lightColor;
        glm::mat4 lightViewProj;

        float hasSkybox;
    };
}
