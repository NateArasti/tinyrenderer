#include "mesh_primitives.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace tr::Loading::Primitives {
    Data::Mesh createPlaneMesh() {
        Data::Mesh mesh;
        mesh.name = "plane";
        mesh.vertices = {
            { .position = { -0.5f, 0, -0.5f },  .normal = { 0, 1, 0 },  .uv = { 0, 0 } },
            { .position = { 0.5f, 0, -0.5f },   .normal = { 0, 1, 0 },  .uv = { 1, 0 } },
            { .position = { -0.5f, 0, 0.5f },   .normal = { 0, 1, 0 },  .uv = { 0, 1 } },
            { .position = { 0.5f, 0, 0.5f },    .normal = { 0, 1, 0 },  .uv = { 1, 1 } },
        };
        mesh.indices = { 0, 1, 2, 1, 3, 2 };
        mesh.subMeshData.push_back(6);
        mesh.recalculateBounds();
        return mesh;
    }

    Data::Mesh createCubeMesh() {
        Data::Mesh mesh;
        mesh.name = "cube";
        mesh.vertices = {

            { .position = { -0.5f, -0.5f, 0.5f },   .normal = { -1, 0, 0 },     .uv = { 0, 0 } },
            { .position = { -0.5f, -0.5f, -0.5f },  .normal = { -1, 0, 0 },     .uv = { 1, 0 } },
            { .position = { -0.5f, 0.5f, 0.5f },    .normal = { -1, 0, 0 },     .uv = { 0, 1 } },
            { .position = { -0.5f, 0.5f, -0.5f },   .normal = { -1, 0, 0 },     .uv = { 1, 1 } },

            { .position = { 0.5f, -0.5f, -0.5f },   .normal = { 1, 0, 0 },      .uv = { 0, 0 } },
            { .position = { 0.5f, -0.5f, 0.5f },    .normal = { 1, 0, 0 },      .uv = { 1, 0 } },
            { .position = { 0.5f, 0.5f, -0.5f },    .normal = { 1, 0, 0 },      .uv = { 0, 1 } },
            { .position = { 0.5f, 0.5f, 0.5f },     .normal = { 1, 0, 0 },      .uv = { 1, 1 } },

            { .position = { 0.5f, -0.5f, -0.5f },   .normal = { 0, -1, 0 },     .uv = { 0, 0 } },
            { .position = { -0.5f, -0.5f, -0.5f },  .normal = { 0, -1, 0 },     .uv = { 1, 0 } },
            { .position = { 0.5f, -0.5f, 0.5f },    .normal = { 0, -1, 0 },     .uv = { 0, 1 } },
            { .position = { -0.5f, -0.5f, 0.5f },   .normal = { 0, -1, 0 },     .uv = { 1, 1 } },

            { .position = { -0.5f, 0.5f, -0.5f },   .normal = { 0, 1, 0 },      .uv = { 0, 0 } },
            { .position = { 0.5f, 0.5f, -0.5f },    .normal = { 0, 1, 0 },      .uv = { 1, 0 } },
            { .position = { -0.5f, 0.5f, 0.5f },    .normal = { 0, 1, 0 },      .uv = { 0, 1 } },
            { .position = { 0.5f, 0.5f, 0.5f },     .normal = { 0, 1, 0 },      .uv = { 1, 1 } },

            { .position = { -0.5f, -0.5f, -0.5f },  .normal = { 0, 0, -1 },     .uv = { 0, 0 } },
            { .position = { 0.5f, -0.5f, -0.5f },   .normal = { 0, 0, -1 },     .uv = { 1, 0 } },
            { .position = { -0.5f, 0.5f, -0.5f },   .normal = { 0, 0, -1 },     .uv = { 0, 1 } },
            { .position = { 0.5f, 0.5f, -0.5f },    .normal = { 0, 0, -1 },     .uv = { 1, 1 } },

            { .position = { 0.5f, -0.5f, 0.5f },    .normal = { 0, 0, 1 },      .uv = { 0, 0 } },
            { .position = { -0.5f, -0.5f, 0.5f },   .normal = { 0, 0, 1 },      .uv = { 1, 0 } },
            { .position = { 0.5f, 0.5f, 0.5f },     .normal = { 0, 0, 1 },      .uv = { 0, 1 } },
            { .position = { -0.5f, 0.5f, 0.5f },    .normal = { 0, 0, 1 },      .uv = { 1, 1 } },
        };
        for (uint32_t i = 0; i < 6; ++i) {
            mesh.indices.insert(mesh.indices.end(), {
                0 + 4 * i, 1 + 4 * i, 2 + 4 * i,
                1 + 4 * i, 3 + 4 * i, 2 + 4 * i
            });
        }
        mesh.subMeshData.push_back(static_cast<uint32_t>(mesh.indices.size()));

        mesh.recalculateBounds();
        return mesh;
    }

    Data::Mesh createSphereMesh(uint32_t segments, uint32_t rings) {
        segments = std::max(segments, 3u);
        rings = std::max(rings, 2u);
        Data::Mesh mesh;
        mesh.name = "sphere";
        mesh.vertices.reserve((rings + 1) * (segments + 1));
        mesh.indices.reserve(6 * segments * (rings - 1));

        for (uint32_t ring = 0; ring <= rings; ++ring) {
            const float v = static_cast<float>(ring) / rings;
            const float theta = v * std::numbers::pi_v<float>;
            for (uint32_t segment = 0; segment <= segments; ++segment) {
                const float u = static_cast<float>(segment) / segments;
                const float phi = u * 2.0f * std::numbers::pi_v<float>;
                const glm::vec3 normal(
                    std::sin(theta) * std::cos(phi),
                    std::cos(theta),
                    -std::sin(theta) * std::sin(phi)
                );
                mesh.vertices.push_back({ .position = normal, .normal = normal, .uv = { u, v } });
            }
        }

        for (uint32_t ring = 0; ring < rings; ++ring) {
            for (uint32_t segment = 0; segment < segments; ++segment) {
                const uint32_t a = ring * (segments + 1) + segment;
                const uint32_t b = a + segments + 1;
                if (ring != 0) {
                    mesh.indices.insert(mesh.indices.end(), { a, a + 1, b });
                }
                if (ring != rings - 1) {
                    mesh.indices.insert(mesh.indices.end(), { a + 1, b + 1, b });
                }
            }
        }
        mesh.subMeshData.push_back(static_cast<uint32_t>(mesh.indices.size()));
        mesh.recalculateBounds();
        return mesh;
    }

    Data::Mesh createTorusMesh(float innerRadius, float outerRadius, int numSlices, int numStacks) {
        Data::Mesh mesh;
        mesh.name = "torus";

        float dp = (2 * glm::pi<float>()) / numSlices;
        float dt = (2 * glm::pi<float>()) / numStacks;

        for (int i = 0; i <= numStacks; ++i) {
            float theta = dt * i;
            float sinTheta = glm::sin(theta);
            float cosTheta = glm::cos(theta);
            for (int j = 0; j <= numSlices; ++j) {
                float phi = dp * j;
                float sinPhi = glm::sin(phi);
                float cosPhi = glm::cos(phi);

                mesh.vertices.push_back(
                    {
                        .position = glm::vec3(
                            cosTheta * (outerRadius + cosPhi * innerRadius),
                            sinPhi * innerRadius,
                            -sinTheta * (outerRadius + cosPhi * innerRadius)
                        ),
                        .normal = glm::vec3(cosTheta * cosPhi, sinPhi, -sinTheta * cosPhi),
                        .uv = { static_cast<float>(i) / numStacks, static_cast<float>(j) / numSlices }
                    }
                );
            }
        }
        for (int i = 0; i < numStacks; ++i) {
            for (int j = 0; j < numSlices; ++j) {
                uint32_t current = i * (numSlices + 1) + j;
                uint32_t nextRow = (i + 1) * (numSlices + 1) + j;
                mesh.indices.insert(mesh.indices.end(), {
                    current, current + 1, nextRow,
                    nextRow, current + 1, nextRow + 1
                });
            }
        }
        mesh.subMeshData.push_back(static_cast<uint32_t>(mesh.indices.size()));

        mesh.recalculateBounds();
        return mesh;
    }
}
