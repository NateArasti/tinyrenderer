#pragma once

#include <glm/mat4x4.hpp>

namespace tr::Rendering {
    struct alignas(16) InstanceData {
        glm::mat4 model;
        glm::mat4 normal;
        uint32_t materialIndex = 0;
    };
}
