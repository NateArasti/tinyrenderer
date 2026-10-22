#pragma once

#include <array>
#include <unordered_map>

#include "render_graph.h"
#include "draw_commands.h"
#include "defines.h"

namespace tr::Rendering {
    class ColorPass : public RenderPass {
    private:
        const Data::Shader _shader;

        Handle<BindingLayout> _sceneLayout;
        Handle<BindingLayout> _materialLayout;
        Handle<BindingLayout> _environmentLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _sceneSets;
        Handle<BindingSet> _materialSet;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _environmentSets;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _opaquePipeline;
        Handle<Pipeline> _transparentPipeline;

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

        explicit ColorPass(Data::Shader shader, RenderingResources& resources);
        ~ColorPass() override;

        void setMaterialTable(Handle<GPUBuffer> materials, std::span<const Handle<GPUTexture>> textures);
        void createPipelines(Format colorFormat, Format depthFormat, uint32_t samples);
        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
