#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "handle.h"
#include "material.h"
#include "mesh.h"

namespace tr::Rendering {
    struct DrawCommand {
        Handle<Mesh> mesh;
        uint32_t subMeshIndex = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 1;
        glm::vec3 sortPosition = glm::vec3(0.0f);
    };
}
