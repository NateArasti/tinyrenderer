#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "handle.h"
#include "material.h"
#include "mesh.h"

namespace tr::Rendering {
    struct DrawCommand {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        tr::Data::Handle<tr::Data::Mesh> mesh;
        uint32_t subMeshIndex = 0;
        tr::Data::Handle<tr::Data::Material> material;
    };
}
