#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render_pass.h"
#include "vulkan_context.h"
#include "resource_factory.h"
#include "draw_commands.h"
#include "shader.h"

namespace tr::Rendering::Vulkan {
    class ShadowPass : public RenderPass {
    private:
        static constexpr uint32_t SHADOW_MAP_SIZE = 4096;

        VulkanContext& _vulkanContext;

        GPUImage _image;
        vk::raii::Sampler _sampler = nullptr;
        vk::Format _format = vk::Format::eD32Sfloat;
        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;

    public:
        explicit ShadowPass(
            VulkanContext& context,
            ResourceFactory& resourceFactory,
            VulkanResources& resources
        );

        ShadowPass(const ShadowPass&) = delete;
        ShadowPass& operator=(const ShadowPass&) = delete;
        ShadowPass(ShadowPass&&) = delete;
        ShadowPass& operator=(ShadowPass&&) = delete;
        
        void createPipeline(
            const Data::Shader& shader,
            vk::VertexInputBindingDescription bindingDescription,
            std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions
        );
        vk::DescriptorImageInfo descriptorInfo() const;

        using RenderPass::render;

    private:
        void begin(vk::raii::CommandBuffer& commandBuffer) override;
        void draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command) override;
        void end(vk::raii::CommandBuffer& commandBuffer) override;
    };
}
