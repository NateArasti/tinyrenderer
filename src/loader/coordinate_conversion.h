#pragma once

#include "mesh.h"

namespace tr::Loading {
    inline void convertToLeftHanded(tr::Data::Mesh& mesh) {
        // Reflect RH source data; retaining indices keeps exterior faces CCW in LH space.
        for (auto& vertex : mesh.vertices) {
            vertex.position.z = -vertex.position.z;
            vertex.normal.z = -vertex.normal.z;
        }
    }

    inline glm::mat4 convertToLeftHanded(const glm::mat4& matrix) {
        const glm::mat4 reflection(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, -1, 0,
            0, 0, 0, 1
        );
        return reflection * matrix * reflection;
    }
}
