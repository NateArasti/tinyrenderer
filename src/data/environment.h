#pragma once

#include <glm/vec3.hpp>

#include "handle.h"
#include "cubemap.h"
#include "light.h"

namespace tr::Data {
    struct Environment {
        tr::Data::Handle<Cubemap> skyboxHandle;
        float skyboxRotation = 0.0f;
        float skyboxExposure = 1.0f;
        glm::vec3 skyboxTint = glm::vec3(1);
        glm::vec3 clearColor{ 0.05f, 0.05f, 0.05f };

        Light directionalLight;
    };
}
