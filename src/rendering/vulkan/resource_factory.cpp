#include "resource_factory.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    ResourceFactory::ResourceFactory(VulkanContext& vulkanContext)
        : _vulkanContext(vulkanContext)
    {
        vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = vulkanContext.queueIndex
        };

        _commandPool = vk::raii::CommandPool(vulkanContext.device, createInfo);
    }

	std::unique_ptr<vk::raii::CommandBuffer> ResourceFactory::beginSingleTimeCommands() {
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

	void ResourceFactory::endSingleTimeCommands(const vk::raii::CommandBuffer &commandBuffer) const {
		commandBuffer.end();

        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        _vulkanContext.queue.submit(submitInfo, nullptr);
		_vulkanContext.queue.waitIdle();
	}

    void ResourceFactory::transitionImageLayout(
        const vk::raii::Image& image,
        const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout,
        uint32_t mipLevels
    ) {
		const auto commandBuffer = beginSingleTimeCommands();

		vk::ImageMemoryBarrier barrier{
		    .oldLayout = oldLayout,
		    .newLayout = newLayout,
		    .image = image,
            .subresourceRange = {
                vk::ImageAspectFlagBits::eColor,
                0,
                mipLevels,
                0,
                1
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

    void ResourceFactory::copyBufferToImage(
        const vk::raii::Buffer& buffer,
        const vk::raii::Image& image,
        uint32_t width, uint32_t height
    ) {
		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}
        };
        commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
		endSingleTimeCommands(*commandBuffer);
    }

    void ResourceFactory::generateMipmaps(
        vk::raii::Image& image,
        vk::Format imageFormat,
        int32_t texWidth, int32_t texHeight,
        uint32_t mipLevels
    ) {
        vk::FormatProperties formatProperties = _vulkanContext.physicalDevice.getFormatProperties(imageFormat);

		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
		{
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();

        vk::ImageMemoryBarrier barrier = {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image
        };
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

			vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
			offsets[0] = vk::Offset3D(0, 0, 0);
			offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
			dstOffsets[0] = vk::Offset3D(0, 0, 0);
            dstOffsets[1] = vk::Offset3D(
                mipWidth > 1 ? mipWidth / 2 : 1,
                mipHeight > 1 ? mipHeight / 2 : 1,
                1
            );
            vk::ImageBlit blit = {
                .srcSubresource = {},
                .srcOffsets = offsets,
                .dstSubresource = {},
                .dstOffsets = dstOffsets
            };
            blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
			blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);

            commandBuffer->blitImage(
                image, vk::ImageLayout::eTransferSrcOptimal,
                image, vk::ImageLayout::eTransferDstOptimal,
                { blit },
                vk::Filter::eLinear
            );

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            commandBuffer->pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                {}, {}, {},
                barrier
            );

			if (mipWidth > 1)
				mipWidth /= 2;
			if (mipHeight > 1)
				mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {}, {}, {},
            barrier
        );

		endSingleTimeCommands(*commandBuffer);
    }

    uint32_t ResourceFactory::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
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

    GPUImage ResourceFactory::createImage(
        uint32_t width, uint32_t height,
        vk::Format format,
        uint32_t mipLevels,
        vk::SampleCountFlagBits samples,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties, 
        vk::ImageAspectFlags aspectFlags
    ) {
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = { width, height, 1 },
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = samples,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        };
        vk::raii::Image image(_vulkanContext.device, imageInfo);
        auto memReqs = image.getMemoryRequirements();
        vk::raii::DeviceMemory memory(_vulkanContext.device, vk::MemoryAllocateInfo{
            .allocationSize = memReqs.size,
            .memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties)
        });
        image.bindMemory(*memory, 0);
        vk::ImageViewCreateInfo viewInfo{
		    .image = image,
		    .viewType = vk::ImageViewType::e2D,
		    .format = format,
            .subresourceRange = {
                .aspectMask = aspectFlags,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::raii::ImageView imageView(_vulkanContext.device, viewInfo);
        return GPUImage{
            .image = std::move(image),
            .memory = std::move(memory),
            .view = std::move(imageView),
            .format = format,
            .extent = { width, height },
            .mipLevels = mipLevels,
            .samples = samples
        };
    }

    GPUBuffer ResourceFactory::createBuffer(
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
        return GPUBuffer{
            .buffer = std::move(buffer),
            .memory = std::move(bufferMemory),
            .size = size
        };
    }

    void ResourceFactory::copyBuffer(
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

    GPUImage ResourceFactory::uploadTexture(
        std::span<const std::byte> pixels,
        uint32_t width, uint32_t height,
        vk::Format format,
        bool shouldGenerateMipmaps
    ) {
        if (width == 0 || height == 0 || pixels.empty()) {
            throw std::invalid_argument("Cannot upload an empty texture");
        }

        const uint32_t mipLevels = shouldGenerateMipmaps
            ? static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1
            : 1;

        GPUBuffer stagingBuffer = createBuffer(
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

        GPUImage image = createImage(
            width, height,
            format,
            mipLevels,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            usage,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor
        );

        transitionImageLayout(
            image.image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            mipLevels
        );
        copyBufferToImage(stagingBuffer.buffer, image.image, width, height);

        if (shouldGenerateMipmaps) {
            generateMipmaps(image.image, format, width, height, mipLevels);
        }
        else {
            transitionImageLayout(
                image.image,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                mipLevels
            );
        }

        return image;
    }

    GPUBuffer ResourceFactory::uploadBuffer(
        std::span<const std::byte> data,
        vk::BufferUsageFlags finalUsage
    ) {
        if (data.empty()) {
            throw std::invalid_argument("Cannot upload an empty buffer");
        }

        GPUBuffer stagingBuffer = createBuffer(
            data.size_bytes(),
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        void* mapped = stagingBuffer.memory.mapMemory(0, stagingBuffer.size);
        std::memcpy(mapped, data.data(), data.size_bytes());
        stagingBuffer.memory.unmapMemory();

        GPUBuffer destination = createBuffer(
            data.size_bytes(),
            finalUsage | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        copyBuffer(stagingBuffer.buffer, destination.buffer, destination.size);

        return destination;
    }
}
