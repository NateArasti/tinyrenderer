#pragma once

#include <cstddef>
#include <memory>
#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "../vulkan_context.h"
#include "../utility/gpu_buffer.h"
#include "../utility/gpu_image.h"

namespace tr::Rendering::Vulkan {
    struct ImageDescription {
        vk::ImageCreateFlags flags{};
        vk::ImageType imageType = vk::ImageType::e2D;
        vk::ImageViewType viewType = vk::ImageViewType::e2D;
        vk::Format format = vk::Format::eUndefined;
        vk::Extent3D extent{ 1, 1, 1 };
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
        vk::ImageUsageFlags usage{};
        vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    };

    struct ImageViewDescription {
        vk::ImageViewType viewType = vk::ImageViewType::e2D;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
        uint32_t baseMipLevel = 0;
        uint32_t mipLevels = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLayers = 1;
    };

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

        GPUImage createImage(const ImageDescription& description);
        vk::raii::ImageView createImageView(const vk::raii::Image& target, const ImageViewDescription& description);

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
