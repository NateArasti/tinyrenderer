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

        void recalculateNormals() {
            for (size_t i = 0; i < indices.size(); i += 3) {
                const uint32_t i0 = indices[i + 0];
                const uint32_t i1 = indices[i + 1];
                const uint32_t i2 = indices[i + 2];

                const glm::vec3 edgeA = vertices[i1].position - vertices[i0].position;
                const glm::vec3 edgeB = vertices[i2].position - vertices[i0].position;
                const glm::vec3 normal = glm::normalize(glm::cross(edgeA, edgeB));

                vertices[i0].normal = normal;
                vertices[i1].normal = normal;
                vertices[i2].normal = normal;
            }
        }
    };
}
