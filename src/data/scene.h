#pragma once

#include <cstddef>
#include <limits>
#include <vector>
#include <memory>

#include "pool.h"
#include "gameobject.h"
#include "light.h"

namespace tr::Data {
    struct MeshBounds {
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
        bool valid = false;
    };

    class Scene {
    private:
        std::vector<std::unique_ptr<GameObject>> _objects;
        bool _hasBounds = false;
    
    public:
        tr::Resources::Pool<Material> materials;

        glm::vec3 sceneMin = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 sceneMax = glm::vec3(std::numeric_limits<float>::lowest());
        float scale = 1.0f;
        
        size_t verticesCount = 0;
        size_t polygonCount = 0;

        Scene() = default;
        ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = default;
        Scene& operator=(Scene&&) = default;

        auto& getObjects() { return _objects; }
        const auto& getObjects() const { return _objects; }

        MeshBounds registerMesh(const Mesh& mesh) {
            verticesCount += mesh.vertices.size();
            polygonCount += mesh.indices.size() / 3;

            MeshBounds bounds;
            for (const auto& vertex : mesh.vertices) {
                bounds.min = glm::min(bounds.min, vertex.position);
                bounds.max = glm::max(bounds.max, vertex.position);
                bounds.valid = true;
            }
            return bounds;
        }

        void includeBounds(const MeshBounds& bounds, const glm::mat4& transform = glm::mat4(1.0f)) {
            if (!bounds.valid) {
                return;
            }

            for (uint32_t corner = 0; corner < 8; ++corner) {
                const glm::vec3 localPosition{
                    (corner & 1) ? bounds.max.x : bounds.min.x,
                    (corner & 2) ? bounds.max.y : bounds.min.y,
                    (corner & 4) ? bounds.max.z : bounds.min.z
                };
                const glm::vec3 worldPosition = transform * glm::vec4(localPosition, 1.0f);
                sceneMin = glm::min(sceneMin, worldPosition);
                sceneMax = glm::max(sceneMax, worldPosition);
            }
            _hasBounds = true;
        }

        std::pair<glm::vec3, glm::vec3> getSceneBounds() const {
            if (!_hasBounds) {
                return { glm::vec3(0.0f), glm::vec3(0.0f) };
            }

            glm::vec3 sceneSize = sceneMax - sceneMin;
            glm::vec3 sceneCenter = sceneMin + 0.5f * sceneSize;

            return { sceneCenter, sceneSize };
        }
    };
}
