#pragma once

#include <vector>
#include <string>

#include "handle.h"
#include "transform.h"
#include "mesh.h"
#include "material.h"

namespace tr::Data {
    struct GameObject {
        std::string name;
        Transform transform;
        Handle<Mesh> mesh;
        std::vector<Handle<Material>> materials;
    };
}
