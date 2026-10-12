#pragma once

#include "render_graph.h"
#include "ui_renderer.h"

namespace tr::Rendering {
    class UIPass : public RenderPass {
    private:
        UIRenderer& _uiRenderer;

    public:
        RGHandle target;
        ImDrawData* drawData = nullptr;

        UIPass(RenderingResources& resources, UIRenderer& uiRenderer);

        void prepareFrame();
        void setup(RenderGraphBuilder& builder) override;
        void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) override;
    };
}
