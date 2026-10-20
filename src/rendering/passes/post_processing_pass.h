#pragma once

#include "defines.h"
#include "render_graph.h"
#include "handle.h"
#include "rendering_structs.h"
#include "render_graph.h"
#include "post_processing_data.h"

#include <array>

using namespace tr::Data;

namespace tr::Rendering {
    class PostProcessingPass : public RenderPass {
    private:
        Handle<GPUBuffer> _vertexBuffer;

        Handle<BindingLayout> _bindingsLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _bindingSets;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _pipeline;

    public:
        PostProcessingData postProcessingData;
        RGHandle source;
        RGHandle target;

        PostProcessingPass(RenderingResources& resources);
        ~PostProcessingPass() override;

        void createPipeline(Format format);

        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
