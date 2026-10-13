#pragma once

#include <array>
#include <unordered_map>

#include "render_graph.h"
#include "draw_commands.h"
#include "defines.h"
#include "pbr.h"

namespace tr::Rendering {
    class ColorPass : public RenderPass {
    private:
        struct MaterialBindings {
            Handle<BindingSet> set;
            Handle<GPUBuffer> buffer;
        };

        std::unordered_map<Handle<Material>, MaterialBindings> _materials;
        Data::EmbeddedShaders::Pbr _shader;

        Handle<BindingLayout> _cameraLayout;
        Handle<BindingLayout> _materialLayout;
        Handle<BindingLayout> _environmentLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _cameraSets;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _environmentSets;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _opaquePipeline;
        Handle<Pipeline> _transparentPipeline;

        Handle<BindingSet> materialBindings(Handle<Material> handle);
        void updateBindings(RenderGraphContext& context, const FrameContext& frame);
        void draw(
            CommandBuffer& cmd,
            const FrameContext& frame,
            std::span<const DrawCommand> draws,
            Handle<Pipeline> pipeline
        );

    public:
        RGHandle target;
        RGHandle depth;
        RGHandle resolve;
        RGHandle shadow;
        RGHandle irradiance;
        RGHandle prefiltered;
        RGHandle brdf;

        std::span<const DrawCommand> opaque;
        std::span<const DrawCommand> transparent;

        explicit ColorPass(RenderingResources& resources);
        ~ColorPass() override;

        void clearMaterials();
        void createPipelines(Format colorFormat, Format depthFormat, uint32_t samples);
        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
