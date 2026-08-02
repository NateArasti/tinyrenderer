#pragma once

#include <cstddef>
#include <memory>
#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"
#include "gpu_buffer.h"
#include "gpu_image.h"

namespace tr::Rendering::Vulkan {
    class ResourceFactory {
    private:
        VulkanContext& _vulkanContext;
        vk::raii::CommandPool _commandPool = nullptr;

        std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();
        void endSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer) const;

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        void copyBufferToImage(
            const vk::raii::Buffer& buffer,
            const vk::raii::Image& image,
            uint32_t width, 
            uint32_t height
        );
        void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);
        
        void transitionImageLayout(
            const vk::raii::Image& image,
            const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout,
            uint32_t mipLevels
        );

        void generateMipmaps(
            vk::raii::Image& image,
            vk::Format imageFormat,
            int32_t texWidth, int32_t texHeight,
            uint32_t mipLevels
        );

    public:
        explicit ResourceFactory(VulkanContext& vulkanContext);

        ResourceFactory(const ResourceFactory&) = delete;
        ResourceFactory& operator=(const ResourceFactory&) = delete;
        ResourceFactory(ResourceFactory&&) = delete;
        ResourceFactory& operator=(ResourceFactory&&) = delete;

        GPUImage createImage(
            uint32_t width, uint32_t height,
            vk::Format format,
            uint32_t mipLevels,
            vk::SampleCountFlagBits samples,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties, 
            vk::ImageAspectFlags aspectFlags
        );

        GPUBuffer createBuffer(
            vk::DeviceSize size,
            vk::BufferUsageFlags usage,
            vk::MemoryPropertyFlags properties
        );

        GPUImage uploadTexture(
            std::span<const std::byte> pixels,
            uint32_t width, uint32_t height,
            vk::Format format,
            bool generateMipmaps
        );

        GPUBuffer uploadBuffer(
            std::span<const std::byte> data,
            vk::BufferUsageFlags finalUsage
        );
    };
}
