#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

#include "draw_commands.h"
#include "scene_data.h"
#include "vulkan_resources.h"

namespace tr::Rendering::Vulkan {
    class RenderPass {
    public:
        struct Context {
            const SceneData* sceneData = nullptr;
            vk::DescriptorSet sceneDescriptorSet = nullptr;
            vk::Image targetImage = nullptr;
            vk::ImageView targetImageView = nullptr;
            vk::Extent2D extent{};
        };

    protected:
        VulkanResources& _resources;
        const Context* _context = nullptr;

        explicit RenderPass(VulkanResources& resources) : _resources(resources) { }

        void transitionImageLayout(
            vk::raii::CommandBuffer& commandBuffer,
            vk::Image image,
            vk::ImageLayout old_layout, vk::ImageLayout new_layout,
            vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask,
            vk::ImageAspectFlags image_aspect_flags
        );

        virtual void begin(vk::raii::CommandBuffer& commandBuffer) = 0;
        virtual void draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command) = 0;
        virtual void end(vk::raii::CommandBuffer& commandBuffer) = 0;

    public:
        ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;

        void render(
            vk::raii::CommandBuffer& commandBuffer,
            std::span<const DrawCommand> commands,
            const Context& context
        );
    };
}
