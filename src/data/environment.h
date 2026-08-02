#pragma once

#include <glm/glm.hpp>

#include "cubemap.h"
#include "handle.h"
#include "light.h"

namespace tr::Data {
    struct Environment {
        tr::Resources::Handle<Cubemap> skyboxHandle;
        float skyboxRotation = 0.0f;
        glm::vec3 skyboxColor = glm::vec3(1);
        glm::vec3 clearColor{ 0.05f, 0.05f, 0.05f };

        Data::Light directionalLight;
    };
}
