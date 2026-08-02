#pragma once

#include <vector>
#include <memory>

#include <imgui.h>

#include "rhi.h"
#include "scene.h"
#include "camera.h"
#include "window.h"

namespace tr::Rendering {
    class Renderer {
    private:
        tr::App::Window& _window;
        RHI& _renderingInterface;
        std::vector<DrawCommand> _opaqueDrawQueue;
        std::vector<std::pair<DrawCommand, float>> _transparentDrawQueue;
        std::vector<DrawCommand> _colorDrawQueue;
        
    public:
        explicit Renderer(RHI& rhi, tr::App::Window& window);
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        RHI& getInterface() { return _renderingInterface; }
        RenderingResources& getResources() { return _renderingInterface.resources(); }

        void clearState();
        void render(
            const tr::Data::Scene& scene,
            const tr::Data::Camera& camera,
            const tr::Data::Light& light,
            ImDrawData* uiDrawData
        );
    };
}
