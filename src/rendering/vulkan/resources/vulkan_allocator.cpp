#include "vulkan_allocator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    VulkanAllocator::VulkanAllocator(VulkanContext& vulkanContext)
        : _vulkanContext(vulkanContext)
    {
        vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = vulkanContext.queueIndex
        };

        _commandPool = vk::raii::CommandPool(vulkanContext.device, createInfo);
    }

	std::unique_ptr<vk::raii::CommandBuffer> VulkanAllocator::beginSingleTimeCommands() {
		vk::CommandBufferAllocateInfo allocInfo{
		    .commandPool = _commandPool,
		    .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        auto commandBuffer = std::make_unique<vk::raii::CommandBuffer>(
            std::move(vk::raii::CommandBuffers(_vulkanContext.device, allocInfo).front())
        );

		vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        commandBuffer->begin(beginInfo);

		return commandBuffer;
	}

	void VulkanAllocator::endSingleTimeCommands(const vk::raii::CommandBuffer &commandBuffer) const {
		commandBuffer.end();

        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        _vulkanContext.queue.submit(submitInfo, nullptr);
		_vulkanContext.queue.waitIdle();
	}

    void VulkanAllocator::transitionImageLayout(
        const vk::raii::Image& image,
        const vk::ImageLayout oldLayout,
        const vk::ImageLayout newLayout,
        uint32_t mipLevels,
        uint32_t layerCount
    ) {
		const auto commandBuffer = beginSingleTimeCommands();

		vk::ImageMemoryBarrier barrier{
		    .oldLayout = oldLayout,
		    .newLayout = newLayout,
		    .image = image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layerCount
            }
        };

		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}
		commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
		endSingleTimeCommands(*commandBuffer);
	}

    void VulkanAllocator::copyBufferToImage(
        const vk::raii::Buffer& buffer,
        const vk::raii::Image& image,
        uint32_t width,
        uint32_t height,
        uint32_t layerCount
    ) {
		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = layerCount
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { width, height, 1 }
        };
        commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
		endSingleTimeCommands(*commandBuffer);
    }

    void VulkanAllocator::generateMipmaps(
        vk::raii::CommandBuffer& commandBuffer,
        const VulkanImage& image,
        vk::ImageLayout baseLayout
    ) {
        if (baseLayout != vk::ImageLayout::eColorAttachmentOptimal &&
            baseLayout != vk::ImageLayout::eTransferDstOptimal) {
            throw std::invalid_argument("Unsupported mipmap base layout");
        }
        const bool rendered = baseLayout == vk::ImageLayout::eColorAttachmentOptimal;
        const auto baseStage = rendered ? vk::PipelineStageFlagBits2::eColorAttachmentOutput : vk::PipelineStageFlagBits2::eTransfer;
        const auto baseAccess = rendered ? vk::AccessFlagBits2::eColorAttachmentWrite : vk::AccessFlagBits2::eTransferWrite;
        if (image.mipLevels > 1) {
            const auto required = vk::FormatFeatureFlagBits::eBlitSrc |
                vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
            const auto features = _vulkanContext.physicalDevice.getFormatProperties(image.format).optimalTilingFeatures;
            if ((features & required) != required) {
                throw std::runtime_error("Image format does not support linear mipmap blitting");
            }
        }
        auto transition = [&](uint32_t level, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                              vk::PipelineStageFlags2 srcStage, vk::AccessFlags2 srcAccess,
                              vk::PipelineStageFlags2 dstStage, vk::AccessFlags2 dstAccess) {
            const vk::ImageMemoryBarrier2 barrier{
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = *image.image,
                .subresourceRange = { image.aspectMask, level, 1, 0, image.arrayLayers }
            };
            commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });
        };
        for (uint32_t level = 0; level < image.mipLevels; ++level) {
            const bool baseLevel = level == 0;
            const bool hasNext = level + 1 < image.mipLevels;
            transition(level,
                baseLevel ? baseLayout : vk::ImageLayout::eTransferDstOptimal,
                hasNext ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
                baseLevel ? baseStage : vk::PipelineStageFlagBits2::eTransfer,
                baseLevel ? baseAccess : vk::AccessFlagBits2::eTransferWrite,
                hasNext ? vk::PipelineStageFlagBits2::eTransfer : vk::PipelineStageFlagBits2::eFragmentShader,
                hasNext ? vk::AccessFlagBits2::eTransferRead : vk::AccessFlagBits2::eShaderRead);
            if (!hasNext) break;

            transition(level + 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                vk::PipelineStageFlagBits2::eTopOfPipe, {},
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
            const int32_t srcWidth = static_cast<int32_t>(std::max(1u, image.extent.width >> level));
            const int32_t srcHeight = static_cast<int32_t>(std::max(1u, image.extent.height >> level));
            vk::ImageBlit blit{
                .srcSubresource = { image.aspectMask, level, 0, image.arrayLayers },
                .dstSubresource = { image.aspectMask, level + 1, 0, image.arrayLayers }
            };
            blit.srcOffsets[1] = vk::Offset3D(srcWidth, srcHeight, 1);
            blit.dstOffsets[1] = vk::Offset3D(std::max(1, srcWidth / 2), std::max(1, srcHeight / 2), 1);
            commandBuffer.blitImage(*image.image, vk::ImageLayout::eTransferSrcOptimal,
                *image.image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);
            transition(level, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead);
        }

    }

    uint32_t VulkanAllocator::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = _vulkanContext.physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
            if ((typeFilter & (1 << i))
                && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}

    VulkanImage VulkanAllocator::createImage(const ImageDescription& description) {
        vk::ImageCreateInfo imageInfo{
            .flags = description.flags,
            .imageType = description.imageType,
            .format = description.format,
            .extent = description.extent,
            .mipLevels = description.mipLevels,
            .arrayLayers = description.arrayLayers,
            .samples = description.samples,
            .tiling = description.tiling,
            .usage = description.usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        };
        vk::raii::Image image(_vulkanContext.device, imageInfo);
        auto memReqs = image.getMemoryRequirements();
        vk::raii::DeviceMemory memory(_vulkanContext.device, vk::MemoryAllocateInfo{
            .allocationSize = memReqs.size,
            .memoryTypeIndex = findMemoryType(
                memReqs.memoryTypeBits,
                description.memoryProperties
            )
        });
        image.bindMemory(*memory, 0);
        vk::raii::ImageView imageView = createImageView(image, {
            .viewType = description.viewType,
            .format = description.format,
            .aspectMask = description.aspectMask,
            .mipLevels = description.mipLevels,
            .arrayLayers = description.arrayLayers,
        });
        return VulkanImage{
            .memory = std::move(memory),
            .image = std::move(image),
            .view = std::move(imageView),
            .format = description.format,
            .extent = description.extent,
            .mipLevels = description.mipLevels,
            .arrayLayers = description.arrayLayers,
            .samples = description.samples,
            .aspectMask = description.aspectMask
        };
    }

    vk::raii::ImageView VulkanAllocator::createImageView(
        const vk::raii::Image& target,
        const ImageViewDescription& description
    ) {
        vk::ImageViewCreateInfo viewInfo{
		    .image = *target,
		    .viewType = description.viewType,
		    .format = description.format,
            .subresourceRange = {
                .aspectMask = description.aspectMask,
                .baseMipLevel = description.baseMipLevel,
                .levelCount = description.mipLevels,
                .baseArrayLayer = description.baseArrayLayer,
                .layerCount = description.arrayLayers
            }
        };
        return vk::raii::ImageView(_vulkanContext.device, viewInfo);
    }

    VulkanBuffer VulkanAllocator::createBuffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties
    ) {
        vk::BufferCreateInfo bufferInfo {
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive
        };;
        vk::raii::Buffer buffer = vk::raii::Buffer(_vulkanContext.device, bufferInfo);
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
        };
        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(_vulkanContext.device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
        return VulkanBuffer{
            .memory = std::move(bufferMemory),
            .buffer = std::move(buffer),
            .size = size
        };
    }

    void VulkanAllocator::copyBuffer(
        vk::raii::Buffer& srcBuffer,
        vk::raii::Buffer& dstBuffer,
        vk::DeviceSize size
    ) {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = _commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        vk::raii::CommandBuffer commandCopyBuffer = std::move(_vulkanContext.device.allocateCommandBuffers(allocInfo).front());
        commandCopyBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
		commandCopyBuffer.end();
        _vulkanContext.queue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
		_vulkanContext.queue.waitIdle();
    }

    VulkanImage VulkanAllocator::uploadTexture(
        std::span<const std::byte> pixels,
        uint32_t width, uint32_t height,
        vk::Format format,
        bool shouldGenerateMipmaps,
        uint32_t arrayLayers,
        bool isCube
    ) {
        if (width == 0 || height == 0 || pixels.empty()) {
            throw std::invalid_argument("Cannot upload an empty texture");
        }
        if (isCube && arrayLayers != 6) {
            throw std::invalid_argument("Cube textures must have 6 layers");
        }

        const uint32_t mipLevels = shouldGenerateMipmaps
            ? static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1
            : 1;

        VulkanBuffer stagingBuffer = createBuffer(
            pixels.size_bytes(),
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        void* mapped = stagingBuffer.memory.mapMemory(0, stagingBuffer.size);
        std::memcpy(mapped, pixels.data(), pixels.size_bytes());
        stagingBuffer.memory.unmapMemory();

        vk::ImageUsageFlags usage =
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled;
        if (shouldGenerateMipmaps) {
            usage |= vk::ImageUsageFlagBits::eTransferSrc;
        }

        VulkanImage image = createImage({
            .flags = isCube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
            .viewType = isCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
            .format = format,
            .extent = { width, height, 1 },
            .mipLevels = mipLevels,
            .arrayLayers = arrayLayers,
            .usage = usage,
        });

        transitionImageLayout(
            image.image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            mipLevels,
            arrayLayers
        );
        copyBufferToImage(stagingBuffer.buffer, image.image, width, height, arrayLayers);

        if (shouldGenerateMipmaps) {
            auto commandBuffer = beginSingleTimeCommands();
            generateMipmaps(*commandBuffer, image, vk::ImageLayout::eTransferDstOptimal);
            endSingleTimeCommands(*commandBuffer);
        }
        else {
            transitionImageLayout(
                image.image,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                mipLevels,
                arrayLayers
            );
        }

        return image;
    }

    VulkanBuffer VulkanAllocator::uploadBuffer(
        std::span<const std::byte> data,
        vk::BufferUsageFlags finalUsage
    ) {
        if (data.empty()) {
            throw std::invalid_argument("Cannot upload an empty buffer");
        }

        VulkanBuffer stagingBuffer = createBuffer(
            data.size_bytes(),
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        void* mapped = stagingBuffer.memory.mapMemory(0, stagingBuffer.size);
        std::memcpy(mapped, data.data(), data.size_bytes());
        stagingBuffer.memory.unmapMemory();

        VulkanBuffer destination = createBuffer(
            data.size_bytes(),
            finalUsage | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        copyBuffer(stagingBuffer.buffer, destination.buffer, destination.size);

        return destination;
    }
}
