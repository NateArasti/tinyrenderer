#include "gbuffer.h"

namespace tr::Rendering::Vulkan {
    GBuffer::GBuffer(VulkanContext& context, ResourceFactory& factory, Swapchain& swapchain)
        : _context(context), _factory(factory), _swapchain(swapchain)
    {
        recreate();
    }

    void GBuffer::recreate() {
        colorImage = _factory.createImage({
            .format = _swapchain.format(),
            .extent = {
                _swapchain.extent().width,
                _swapchain.extent().height,
                1
            },
            .samples = _context.msaaSamples,
            .usage = vk::ImageUsageFlagBits::eColorAttachment
        });
        depthImage = _factory.createImage({
            .format = _context.depthFormat,
            .extent = {
                _swapchain.extent().width,
                _swapchain.extent().height,
                1
            },
            .samples = _context.msaaSamples,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .aspectMask = vk::ImageAspectFlagBits::eDepth
        });
    }
}
