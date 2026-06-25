#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

namespace tr::Data {
    struct Mesh {
        struct Vertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec4 color = glm::vec4(1);
        };

        std::string name;
        
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> subMeshData;
    };
}
