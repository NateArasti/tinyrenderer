#pragma once

#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "../vulkan_context.h"
#include "../gbuffer.h"
#include "../utility/vulkan_utility.h"
#include "../resources/vulkan_resources.h"
#include "environment.h"

namespace tr::Rendering::Vulkan {
    class SkyboxPass {
    private:
        VulkanContext& _vulkanContext;
        GBuffer& _gBuffer;
        VulkanResources& _resources;

        GPUBuffer _vertexBuffer;

        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet> _descriptorSets;

        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;

        void createPipeline(vk::Format colorFormat);

    public:
        SkyboxPass(
            VulkanContext& context,
            VulkanResources& resources,
            GBuffer& gBuffer,
            ResourceFactory& factory,
            uint32_t frameCount,
            vk::Format colorFormat
        );

        void recreate(vk::Format colorFormat);

        void record(
            vk::raii::CommandBuffer& commandBuffer,
            const FrameContext& frame,
            const Data::Environment& environment,
            uint32_t frameIndex,
            const vk::DescriptorBufferInfo& sceneBuffer,
            const vk::DescriptorBufferInfo& environmentBuffer
        );
    };
}
