#pragma once

#include <glm/glm.hpp>

#include "cubemap.h"
#include "handle.h"
#include "light.h"

namespace tr::Data {
    struct Environment {
        tr::Resources::Handle<Cubemap> skyboxHandle;
        glm::vec3 clearColor{ 0.05f, 0.05f, 0.05f };

        Data::Light directionalLight;
    };
}
