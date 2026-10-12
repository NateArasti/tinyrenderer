#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace tr::Data {
    struct Light {
        glm::vec3 direction;
        float intensity;
        glm::vec4 color;
        bool enabled = true;
        bool shadowsEnabled = true;
    };
}
