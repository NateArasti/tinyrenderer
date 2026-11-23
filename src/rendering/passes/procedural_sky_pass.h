#pragma once

#include "defines.h"
#include "render_graph.h"
#include "handle.h"
#include "rendering_structs.h"
#include "procedural_sky_data.h"

#include <array>

using namespace tr::Data;

namespace tr::Rendering {
    class ProceduralSkyPass : public RenderPass {
    private:
        Handle<GPUBuffer> _vertexBuffer;

        Handle<BindingLayout> _bindingsLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _bindingSets;
        std::array<Handle<GPUBuffer>, MAX_FRAMES_IN_FLIGHT> _dataBuffers;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _pipeline;

    public:
        RGHandle target;
        ProceduralSkyData proceduralSkyData;

        ProceduralSkyPass(RenderingResources& resources);
        ~ProceduralSkyPass() override;

        void createPipeline(Format format, uint32_t samples);

        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
