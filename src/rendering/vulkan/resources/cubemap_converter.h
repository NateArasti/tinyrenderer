#pragma once

#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "../vulkan_context.h"
#include "resource_factory.h"
#include "../utility/gpu_buffer.h"
#include "../utility/gpu_image.h"

namespace tr::Rendering::Vulkan {
    class CubemapConverter {
    private:
        struct ConversionPipeline {
            vk::raii::PipelineLayout layout = nullptr;
            vk::raii::Pipeline pipeline = nullptr;
        };

        VulkanContext& _context;
        ResourceFactory& _factory;

        vk::raii::CommandPool _commandPool = nullptr;
        GPUBuffer _vertexBuffer;

        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSet _descriptorSet = nullptr;
        vk::raii::Sampler _sourceSampler = nullptr;

        std::unordered_map<vk::Format, ConversionPipeline> _pipelines;

        const ConversionPipeline& pipeline(vk::Format destinationFormat);
        vk::raii::CommandBuffer beginCommands();
        void submitCommands(const vk::raii::CommandBuffer& commandBuffer) const;

    public:
        CubemapConverter(
            VulkanContext& context,
            ResourceFactory& factory
        );

        CubemapConverter(const CubemapConverter&) = delete;
        CubemapConverter& operator=(const CubemapConverter&) = delete;
        CubemapConverter(CubemapConverter&&) = delete;
        CubemapConverter& operator=(CubemapConverter&&) = delete;

        GPUImage convertEquirectangular(
            const GPUImage& source,
            vk::Format destinationFormat
        );
    };
}
