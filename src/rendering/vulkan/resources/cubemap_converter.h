#pragma once

#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"
#include "vulkan_allocator.h"
#include "../vulkan_rendering_structs.h"
#include "shader.h"
#include "environment_data.h"
#include "procedural_sky_data.h"

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
        vk::raii::DescriptorSetLayout _proceduralDescriptorSetLayout = nullptr;
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSet _descriptorSet = nullptr;
        vk::raii::DescriptorSet _proceduralDescriptorSet = nullptr;
        vk::raii::Sampler _sourceSampler = nullptr;
        VulkanBuffer _environmentBuffer;
        VulkanBuffer _skyBuffer;

        ConversionPipeline _environmentPipeline;
        ConversionPipeline _proceduralPipeline;
        ConversionPipeline _irradiancePipeline;
        ConversionPipeline _prefilterPipeline;
        ConversionPipeline _brdfPipeline;

        vk::raii::CommandBuffer beginCommands();
        void submitCommands(const vk::raii::CommandBuffer& commandBuffer) const;
        void updateSourceTexture(const VulkanImage& source);

        ConversionPipeline createPipeline(
            const Shader& shader,
            vk::Format destinationFormat,
            std::span<const vk::DescriptorSetLayout> setLayouts,
            uint32_t pushConstantSize
        );
        VulkanImage renderFaces(
            const ConversionPipeline& pipeline,
            std::span<const vk::DescriptorSet> descriptorSets,
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
        VulkanImage renderProceduralSky(
            const EnvironmentData& environment,
            const Data::ProceduralSkyData& sky,
            uint32_t faceSize
        );
        VulkanImage convolveIrradiance(const VulkanImage& environment);
        VulkanImage prefilterEnvironment(const VulkanImage& environment);
        VulkanImage integrateBRDF();
    };
}
