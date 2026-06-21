#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

namespace tr::Data {
    struct Mesh {
        std::string name;
        
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec4> colors;

        std::vector<uint32_t> indices;
        std::vector<uint32_t> subMeshData;
    };
}
