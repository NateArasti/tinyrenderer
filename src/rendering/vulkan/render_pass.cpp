#include "render_pass.h"

namespace tr::Rendering::Vulkan {
    void RenderPass::render(
        vk::raii::CommandBuffer& commandBuffer,
        std::span<const DrawCommand> commands,
        const Context& context
    ) {
        _context = &context;
        begin(commandBuffer);
        for (const auto& command : commands) {
            draw(commandBuffer, command);
        }
        end(commandBuffer);
        _context = nullptr;
    }

    void RenderPass::transitionImageLayout(
        vk::raii::CommandBuffer& commandBuffer,
        vk::Image image,
        vk::ImageLayout old_layout, vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask,
        vk::ImageAspectFlags image_aspect_flags
    ) {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = image_aspect_flags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::DependencyInfo dependency_info = {
            .dependencyFlags = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };
        commandBuffer.pipelineBarrier2(dependency_info);
    }
}
