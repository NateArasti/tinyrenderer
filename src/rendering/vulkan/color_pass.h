#pragma once

#include "render_pass.h"
#include "resource_factory.h"
#include "swapchain.h"

namespace tr::Rendering::Vulkan {
    class ColorPass : public RenderPass {
    private:
        VulkanContext& _vulkanContext;
        ResourceFactory& _factory;
        Swapchain& _swapchain;
        GPUImage _colorImage;
        GPUImage _depthImage;

        void createAttachments();
        void begin(vk::raii::CommandBuffer& commandBuffer) override;
        void draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command) override;
        void end(vk::raii::CommandBuffer& commandBuffer) override;

    public:
        ColorPass(
            VulkanContext& context,
            ResourceFactory& factory,
            Swapchain& swapchain,
            VulkanResources& resources
        );

        using RenderPass::render;
        void recreate();
    };
}
