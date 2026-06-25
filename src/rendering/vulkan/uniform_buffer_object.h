#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering::Vulkan {
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
    };
}
