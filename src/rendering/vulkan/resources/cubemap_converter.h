#pragma once

#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"
#include "resource_factory.h"
#include "gpu_buffer.h"
#include "gpu_image.h"
#include "shader.h"

namespace tr::Rendering::Vulkan {
    class CubemapConverter {
    private:
        static constexpr vk::Format CUBEMAPS_FORMAT = vk::Format::eR16G16B16A16Sfloat;

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

        ConversionPipeline _environmentPipeline;
        ConversionPipeline _irradiancePipeline;

        ConversionPipeline createPipeline(const tr::Data::Shader& shader, vk::Format destinationFormat);
        vk::raii::CommandBuffer beginCommands();
        void submitCommands(const vk::raii::CommandBuffer& commandBuffer) const;
        GPUImage renderFaces(
            const GPUImage& source,
            CubemapConverter::ConversionPipeline& pipeline,
            uint32_t faceSize,
            bool generateMipmaps = false
        );

    public:
        CubemapConverter(
            VulkanContext& context,
            ResourceFactory& factory
        );

        CubemapConverter(const CubemapConverter&) = delete;
        CubemapConverter& operator=(const CubemapConverter&) = delete;
        CubemapConverter(CubemapConverter&&) = delete;
        CubemapConverter& operator=(CubemapConverter&&) = delete;

        GPUImage convertEquirectangular(const GPUImage& source);
        GPUImage convolveIrradiance(const GPUImage& environment);
    };
}
