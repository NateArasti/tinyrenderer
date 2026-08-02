#pragma once

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

        vk::raii::DescriptorSetLayout _cubemapLayout = nullptr;
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSet _cubemapSet = nullptr;

        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;

        Resources::Handle<Data::Cubemap> _boundCubemap;

        void bindCubemap(const VulkanCubemap& cubemap);
        void createPipeline(vk::Format colorFormat);

    public:
        SkyboxPass(
            VulkanContext& context,
            VulkanResources& resources,
            GBuffer& gBuffer,
            ResourceFactory& factory,
            vk::DescriptorSetLayout sceneLayout,
            vk::Format colorFormat
        );

        void recreate(vk::Format colorFormat);

        void record(
            vk::raii::CommandBuffer& commandBuffer,
            const FrameContext& frame,
            const Data::Environment& environment
        );
    };
}
