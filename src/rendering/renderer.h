#pragma once

#include <vector>
#include <memory>

#include "rhi.h"
#include "resource_manager.h"
#include "scene.h"
#include "camera.h"

namespace tr::Rendering {
    class Renderer {
    private:
        RHI* _renderingInterface = nullptr;
        tr::Data::ResourceManager* _resourceManager = nullptr;
        std::vector<std::pair<DrawCommand, float>> _transparentDrawQueue;
        
    public:
        explicit Renderer(RHI* rhi, tr::Data::ResourceManager* resourceManager);
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void reloadResources();
        void clearState();
        void renderScene(const tr::Data::Camera& camera, const tr::Data::Scene& scene);
    };
}
