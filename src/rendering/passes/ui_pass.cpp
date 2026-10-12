#include "ui_pass.h"

namespace tr::Rendering {
    UIPass::UIPass(RenderingResources& resources, UIRenderer& uiRenderer)
        : RenderPass("UI", resources), _uiRenderer(uiRenderer) { }

    void UIPass::prepareFrame() {
        _uiRenderer.beginFrame();
    }

    void UIPass::setup(RenderGraphBuilder& builder) {
        builder.read(target, TextureState::ColorAttachment);
        builder.write(target, TextureState::ColorAttachment);
    }

    void UIPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
        if (drawData == nullptr) {
            return;
        }

        cmd.beginRendering({
            .renderArea = { .extent = frame.extent },
            .colorAttachment = {
                .target = context.texture(target),
                .load = true,
            },
        });
        _uiRenderer.draw(cmd, drawData);
        cmd.endRendering();
    }
}
