#pragma once

#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"
#include "vulkan_allocator.h"
#include "../vulkan_rendering_structs.h"
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
        VulkanAllocator& _allocator;

        vk::raii::CommandPool _commandPool = nullptr;
        VulkanBuffer _vertexBuffer;

        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSet _descriptorSet = nullptr;
        vk::raii::Sampler _sourceSampler = nullptr;

        ConversionPipeline _environmentPipeline;
        ConversionPipeline _irradiancePipeline;
        ConversionPipeline _prefilterPipeline;
        ConversionPipeline _brdfPipeline;

        vk::raii::CommandBuffer beginCommands();
        void submitCommands(const vk::raii::CommandBuffer& commandBuffer) const;

        ConversionPipeline createPipeline(
            const tr::Data::Shader& shader,
            vk::Format destinationFormat,
            bool usesSourceTexture,
            uint32_t pushConstantSize
        );
        VulkanImage renderFaces(
            const VulkanImage& source,
            CubemapConverter::ConversionPipeline& pipeline,
            uint32_t faceSize,
            bool generateMipmaps = false
        );

    public:
        CubemapConverter(
            VulkanContext& context,
            VulkanAllocator& allocator
        );

        CubemapConverter(const CubemapConverter&) = delete;
        CubemapConverter& operator=(const CubemapConverter&) = delete;
        CubemapConverter(CubemapConverter&&) = delete;
        CubemapConverter& operator=(CubemapConverter&&) = delete;

        VulkanImage convertEquirectangular(const VulkanImage& source);
        VulkanImage convolveIrradiance(const VulkanImage& environment);
        VulkanImage prefilterEnvironment(const VulkanImage& environment);
        VulkanImage integrateBRDF();
    };
}
