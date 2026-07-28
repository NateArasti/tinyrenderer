#pragma once

#include <vector>
#include <memory>

#include "gameobject.h"
#include "light.h"

namespace tr::Data {
    class Scene {
    private:
        std::vector<std::unique_ptr<GameObject>> _objects;
        
    public:
        glm::vec3 sceneCenter = glm::vec3(0.0f);
        glm::vec3 sceneSize = glm::vec3(0.0f);
        float scale = 1.0f;

        Scene() = default;
        ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = default;
        Scene& operator=(Scene&&) = default;
        
        auto& getObjects() { return _objects; }
        const auto& getObjects() const { return _objects; }
    };
}
