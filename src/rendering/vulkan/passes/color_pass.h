#pragma once

#include <span>

#include "draw_commands.h"
#include "../vulkan_context.h"
#include "../gbuffer.h"
#include "../utility/vulkan_utility.h"
#include "../resources/vulkan_resources.h"

namespace tr::Rendering::Vulkan {
    class ColorPass {
    private:
        VulkanContext& _vulkanContext;
        GBuffer& _gBuffer;
        VulkanResources& _resources;

        void begin(vk::raii::CommandBuffer& commandBuffer, const FrameContext& frame);
        void draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command, const FrameContext& frame);
        void end(vk::raii::CommandBuffer& commandBuffer);

    public:
        ColorPass(
            VulkanContext& context,
            GBuffer& gBuffer,
            VulkanResources& resources
        );
        
        void record(
            vk::raii::CommandBuffer& commandBuffer,
            const FrameContext& frame,
            std::span<const DrawCommand> drawCalls
        );
    };
}
