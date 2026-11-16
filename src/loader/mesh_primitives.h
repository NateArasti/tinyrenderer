#pragma once

#include <cstdint>

#include "mesh.h"

namespace tr::Loading::Primitives {
    Data::Mesh createPlaneMesh();
    Data::Mesh createCubeMesh();
    Data::Mesh createSphereMesh(uint32_t segments = 64, uint32_t rings = 32);
    Data::Mesh createTorusMesh(
        float innerRadius = 0.3f,
        float outerRadius = 0.7f,
        int numSlices = 32,
        int numStacks = 64
    );
}
