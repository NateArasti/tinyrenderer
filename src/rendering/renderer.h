#pragma once

#include <vector>
#include <array>
#include <memory>

#include <imgui.h>

#include "defines.h"
#include "rhi.h"
#include "scene.h"
#include "environment.h"
#include "camera.h"
#include "draw_commands.h"
#include "rendering_structs.h"

#include "skybox_pass.h"
#include "shadow_pass.h"
#include "color_pass.h"
#include "post_processing_pass.h"
#include "ui_pass.h"

namespace tr::Rendering {
    class Renderer {
    private:
        RHI& _rhi;
        std::vector<DrawCommand> _opaqueDrawQueue;
        std::vector<DrawCommand> _transparentDrawQueue;

        std::array<Handle<GPUBuffer>, MAX_FRAMES_IN_FLIGHT> _cameraDataBuffers;
        std::array<Handle<GPUBuffer>, MAX_FRAMES_IN_FLIGHT> _environmentDataBuffers;

        Format _colorFormat = Undefined;
        uint32_t _samples = 0;

        std::unique_ptr<SkyboxPass> _skyboxPass;
        std::unique_ptr<ShadowPass> _shadowPass;
        std::unique_ptr<ColorPass> _colorPass;
        std::unique_ptr<PostProcessingPass> _postProcessingPass;
        std::unique_ptr<UIPass> _uiPass;

        void updatePipelines(const FrameContext& frame);
        void updateFrameData(
            FrameContext& frame,
            const tr::Data::Scene& scene,
            const tr::Data::Camera& camera,
            const tr::Data::Environment& environment
        );
        void prepareDrawCalls(const tr::Data::Scene& scene, const tr::Data::Camera& camera);
        void setupGraph(
            RenderGraph& graph,
            const FrameContext& frame,
            const tr::Data::Environment& environment,
            const PostProcessingData& postProcessingData,
            ImDrawData* uiDrawData
        );
        void invalidateSceneCaches();

    public:
        explicit Renderer(RHI& rhi);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        RHI& getInterface() { return _rhi; }
        RenderingResources& getResources() { return _rhi.resources(); }

        void unloadSceneResources(tr::Data::Scene& scene);
        void prepareUI();
        void render(
            const tr::Data::Scene& scene,
            const tr::Data::Camera& camera,
            const tr::Data::Environment& env,
            const tr::Data::PostProcessingData& postProcessingData,
            ImDrawData* uiDrawData
        );
    };
}
