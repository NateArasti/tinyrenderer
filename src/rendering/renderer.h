#pragma once

#include <vector>
#include <memory>

#include <imgui.h>

#include "rhi.h"
#include "resource_manager.h"
#include "scene.h"
#include "camera.h"
#include "window.h"

namespace tr::Rendering {
    class Renderer {
    private:
        tr::App::Window& _window;
        RHI& _renderingInterface;
        tr::Data::ResourceManager& _resourceManager;
        std::vector<DrawCommand> _opaqueDrawQueue;
        std::vector<std::pair<DrawCommand, float>> _transparentDrawQueue;
        
    public:
        explicit Renderer(RHI& rhi, tr::Data::ResourceManager& resourceManager, tr::App::Window& window);
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void reloadResources();
        void clearState();
        void render(
            const tr::Data::Scene& scene,
            const tr::Data::Camera& camera,
            const tr::Data::Light& light,
            ImDrawData* uiDrawData
        );
    };
}
