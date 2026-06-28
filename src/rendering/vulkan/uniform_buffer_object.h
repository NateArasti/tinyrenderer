#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering::Vulkan {
    struct CameraData {
        glm::mat4 view;
        glm::mat4 proj;
    };
}
