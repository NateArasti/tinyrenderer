#pragma once

#include <glm/glm.hpp>

namespace tr::Rendering {
    struct alignas(16) CameraData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPos;
    };
}
