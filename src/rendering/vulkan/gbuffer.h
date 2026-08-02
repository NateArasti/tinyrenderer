#pragma once

#include "swapchain.h"
#include "utility/gpu_image.h"
#include "resources/resource_factory.h"

namespace tr::Rendering::Vulkan {
    class GBuffer {
    private:
        VulkanContext& _context;
        ResourceFactory& _factory;
        Swapchain& _swapchain;

    public:
        GPUImage colorImage;
        GPUImage depthImage;

        GBuffer(
            VulkanContext& context,
            ResourceFactory& factory,
            Swapchain& swapchain
        );

        GBuffer(const GBuffer&) = delete;
        GBuffer& operator=(const GBuffer&) = delete;
        GBuffer(GBuffer&&) = delete;
        GBuffer& operator=(GBuffer&&) = delete;

        void recreate();
    };
}
