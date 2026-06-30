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
        Light directionalLight;
        
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
