#pragma once

#include <array>

#include "defines.h"
#include "render_graph.h"
#include "draw_commands.h"

namespace tr::Rendering {
    class ShadowPass : public RenderPass {
    private:
        static constexpr uint32_t SHADOW_MAP_SIZE = 4096;
        
        Handle<BindingLayout> _bindingLayout;
        std::array<Handle<BindingSet>, MAX_FRAMES_IN_FLIGHT> _sets;
        Handle<PipelineLayout> _pipelineLayout;
        Handle<Pipeline> _pipeline;

    public:
        RGHandle target;
        std::span<const DrawCommand> draws;

        explicit ShadowPass(RenderingResources& resources);
        ~ShadowPass() override;

        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
