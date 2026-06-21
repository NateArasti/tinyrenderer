#pragma once

#include <vector>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "handle.h"
#include "transform.h"
#include "mesh.h"
#include "material.h"

namespace tr::Data {
    struct GameObject {
        std::string name;
        Transform transform;
        tr::Resources::Handle<Data::Mesh> mesh;
        std::vector<tr::Resources::Handle<Data::Material>> materials;
    };
}
