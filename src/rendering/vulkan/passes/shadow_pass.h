#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "../vulkan_context.h"
#include "../resources/vulkan_resources.h"
#include "draw_commands.h"
#include "shader.h"
#include "scene_data.h"

namespace tr::Rendering::Vulkan {
    class ShadowPass {
    private:
        static constexpr uint32_t SHADOW_MAP_SIZE = 4096;

        VulkanContext& _vulkanContext;
        VulkanResources& _resources;

        GPUImage _image;
        vk::raii::Sampler _sampler = nullptr;
        vk::Format _format = vk::Format::eD32Sfloat;
        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;
        
        void createPipeline(
            const Data::Shader& shader,
            vk::VertexInputBindingDescription bindingDescription,
            std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions
        );

        void begin(vk::raii::CommandBuffer& commandBuffer, const SceneData& sceneData);
        void draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command, const SceneData& sceneData);
        void end(vk::raii::CommandBuffer& commandBuffer);

    public:
        explicit ShadowPass(
            VulkanContext& context,
            ResourceFactory& resourceFactory,
            VulkanResources& resources
        );
        
        void record(
            vk::raii::CommandBuffer& commandBuffer,
            const SceneData& sceneData,
            std::span<const DrawCommand> drawCalls
        );

        vk::DescriptorImageInfo shadowMap() const;
    };
}
