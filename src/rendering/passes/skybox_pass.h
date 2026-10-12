#pragma once

#include "defines.h"
#include "render_graph.h"
#include "handle.h"
#include "rendering_structs.h"
#include "cubemap.h"

#include <array>

using namespace tr::Data;

namespace tr::Rendering {
    class SkyboxPass : public RenderPass {
    private:
        Handle<GPUBuffer> _vertexBuffer;

        Handle<BindingLayout> _bindingsLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _bindingSets;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _pipeline;

    public:
        RGHandle cubemap;
        RGHandle target;

        SkyboxPass(RenderingResources& resources);

        ~SkyboxPass() override;
        void createPipeline(Format format, uint32_t samples);

        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
