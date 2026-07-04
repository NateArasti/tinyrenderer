#include "vulkan_renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "scene_data.h"

namespace tr::Rendering::Vulkan {
#ifdef NDEBUG
        constexpr bool enableValidationLayers = false;
#else
        constexpr bool enableValidationLayers = true;
#endif
        
        VulkanRenderer::VulkanRenderer(const tr::App::Application& application) : _application(application) {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        _msaaSamples = getMaxSampleCount();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createDescriptorSetLayout();
        createCommandPool();
        createColorResources(); 
        createDepthResources();
        createShadowResources();
        createFallbackTexture();
        createCommandBuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
        createSyncObjects();
        _initialized = true;
    }

    VulkanRenderer::~VulkanRenderer() {
        if (!_initialized) {
            return;
        }

        _device.waitIdle();
        cleanupSwapchain();
        clearResources();
        _commandBuffers.clear();
        _descriptorSets.clear();
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
        _uniformBuffers.clear();
    }

    void VulkanRenderer::clearResources() {
        _shadersMap.clear();
        _materialsMap.clear();
        _meshesMap.clear();
    }

    void VulkanRenderer::resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            _swapchainDirty = true;
            return;
        }

        recreateSwapchain();
    }

#pragma region Helpers

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0, 
            .stride = sizeof(tr::Data::Mesh::Vertex), 
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
        using Vertex = tr::Data::Mesh::Vertex;
        return {{
            {
                .location = 0, 
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, position)
            },
            {
                .location = 1, 
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, normal)
            },
            {
                .location = 2, 
                .binding = 0,
                .format = vk::Format::eR32G32Sfloat,
                .offset = offsetof(Vertex, uv)
            },
            {
                .location = 3,
                .binding = 0,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = offsetof(Vertex, color)
            }
        }};
    }

    void VulkanRenderer::setDebugName(vk::ObjectType type, uint64_t handle, const char* name) {
        if (!enableValidationLayers) return;

        vk::DebugUtilsObjectNameInfoEXT nameInfo{
            .objectType = type,
            .objectHandle = handle,
            .pObjectName = name
        };
        _device.setDebugUtilsObjectNameEXT(nameInfo);
    }

	std::unique_ptr<vk::raii::CommandBuffer> VulkanRenderer::beginSingleTimeCommands() {
		vk::CommandBufferAllocateInfo allocInfo{
		    .commandPool = _commandPool,
		    .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        std::unique_ptr<vk::raii::CommandBuffer> commandBuffer =
            std::make_unique<vk::raii::CommandBuffer>(
                std::move(vk::raii::CommandBuffers(_device, allocInfo).front())
            );

		vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        commandBuffer->begin(beginInfo);

		return commandBuffer;
	}

	void VulkanRenderer::endSingleTimeCommands(const vk::raii::CommandBuffer &commandBuffer) const {
		commandBuffer.end();

        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        _queue.submit(submitInfo, nullptr);
		_queue.waitIdle();
	}

	uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		vk::PhysicalDeviceMemoryProperties memProperties = _physicalDevice.getMemoryProperties();
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

    std::tuple<vk::raii::Image, vk::raii::DeviceMemory> VulkanRenderer::createImage(
        uint32_t width, uint32_t height,
        vk::Format format,
        uint32_t mipLevels,
        vk::SampleCountFlagBits samples,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties
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
        vk::raii::Image image(_device, imageInfo);
        auto memReqs = image.getMemoryRequirements();
        vk::raii::DeviceMemory memory(_device, vk::MemoryAllocateInfo{
            .allocationSize = memReqs.size,
            .memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties)
        });
        image.bindMemory(*memory, 0);
        return { std::move(image), std::move(memory) };
    }

    vk::raii::ImageView VulkanRenderer::createImageView(
        vk::Image const& image,
        vk::Format format,
        vk::ImageAspectFlags aspectFlags,
        uint32_t mipLevels
    ) {
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
        return vk::raii::ImageView(_device, viewInfo);
	}

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> VulkanRenderer::createBuffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties
    ) {
        vk::BufferCreateInfo bufferInfo {
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive
        };;
        vk::raii::Buffer buffer = vk::raii::Buffer(_device, bufferInfo);
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
        };
        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(_device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
        return {std::move(buffer), std::move(bufferMemory)};
    }

    void VulkanRenderer::copyBuffer(
        vk::raii::Buffer& srcBuffer,
        vk::raii::Buffer& dstBuffer,
        vk::DeviceSize size
    ) {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = _commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        vk::raii::CommandBuffer commandCopyBuffer = std::move(_device.allocateCommandBuffers(allocInfo).front());
        commandCopyBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
		commandCopyBuffer.end();
        _queue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
		_queue.waitIdle();
    }

    vk::Format VulkanRenderer::findSupportedFormat(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features
    ) {
        for (const auto format : candidates) {
            vk::FormatProperties props = _physicalDevice.getFormatProperties(format);

            if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
                ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
            {
            return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    void VulkanRenderer::transitionImageLayout(
	    vk::Image image,
	    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	    vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask,
        vk::ImageAspectFlags image_aspect_flags
    ) {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = image_aspect_flags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::DependencyInfo dependency_info = {
            .dependencyFlags = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };
        _commandBuffers[_currentFrame].pipelineBarrier2(dependency_info);
    }

    void VulkanRenderer::transitionImageLayout(
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

    void VulkanRenderer::copyBufferToImage(
        const vk::raii::Buffer& buffer,
        const vk::raii::Image& image,
        uint32_t width, uint32_t height
    ) {
		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();
		vk::BufferImageCopy region {
		                         .bufferOffset      = 0,
		                         .bufferRowLength   = 0,
		                         .bufferImageHeight = 0,
		                         .imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
		                         .imageOffset       = {0, 0, 0},
		                         .imageExtent       = {width, height, 1}};
		commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
		endSingleTimeCommands(*commandBuffer);
    }

    void VulkanRenderer::generateMipmaps(
        vk::raii::Image& image,
        vk::Format imageFormat,
        int32_t texWidth, int32_t texHeight,
        uint32_t mipLevels
    ) {
        vk::FormatProperties formatProperties = _physicalDevice.getFormatProperties(imageFormat);

		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
		{
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();

		vk::ImageMemoryBarrier barrier          = {.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead, .oldLayout = vk::ImageLayout::eTransferDstOptimal, .newLayout = vk::ImageLayout::eTransferSrcOptimal, .srcQueueFamilyIndex = vk::QueueFamilyIgnored, .dstQueueFamilyIndex = vk::QueueFamilyIgnored, .image = image};
		barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount     = 1;
		barrier.subresourceRange.levelCount     = 1;

		int32_t mipWidth  = texWidth;
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

#pragma endregion

#pragma region Vulkan initialization

    std::vector<const char*> getRequiredInstanceExtensions() {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    void VulkanRenderer::createInstance()
    {
        vk::ApplicationInfo appInfo{
            .pApplicationName = _application.name.data(),
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: parse application version?
            .pEngineName = _application.name.data(),
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(_validationLayers.begin(), _validationLayers.end());
        }
        auto layerProperties = _context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(
            requiredLayers,
            [&layerProperties](auto const& requiredLayer) {
                return std::ranges::none_of(
                    layerProperties,
                    [requiredLayer](auto const& layerProperty) {
                        return strcmp(layerProperty.layerName, requiredLayer) == 0;
                    }
                );
            }
        );
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        auto requiredExtensions = getRequiredInstanceExtensions();
        auto extensionProperties = _context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(
            requiredExtensions,
            [&extensionProperties](auto const& requiredExtension) {
                return std::ranges::none_of(
                    extensionProperties,
                    [requiredExtension](auto const& extensionProperty) {
                        return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                    }
                );
            }
        );
        if (unsupportedPropertyIt != requiredExtensions.end())
        {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };
        _instance = vk::raii::Instance(_context, createInfo);
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
        return vk::False;
    }

    void VulkanRenderer::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlags,
            .pfnUserCallback = &debugCallback
        };
        
        _debugMessenger = _instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void VulkanRenderer::createSurface() {
        VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

        auto result = glfwCreateWindowSurface(
            static_cast<VkInstance>(*_instance),
            static_cast<GLFWwindow*>(_application.window->nativeHandle()),
            nullptr,
            &rawSurface
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("glfwCreateWindowSurface failed");
        }

        _surface = vk::raii::SurfaceKHR(_instance, rawSurface);
    }

    bool isDeviceSuitable(
        const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::SurfaceKHR& surface,
        const std::vector<const char*>& requiredDeviceExtension
    ) {
        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

        if (!supportsVulkan1_3) {
            return false;
        }

        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphicsAndPresent = false;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, *surface)) {
                supportsGraphicsAndPresent = true;
                break;
            }
        }

        if (!supportsGraphicsAndPresent) {
            return false;
        }

        bool supportsAllRequiredExtensions = true;
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        for (const auto& requiredDeviceExtension : requiredDeviceExtension) {
            bool isSupported = false;
            for (const auto& availableDeviceExtension : availableDeviceExtensions) {
                if (strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0) {
                    isSupported = true;
                    break;
                }
            }
            if (!isSupported) {
                supportsAllRequiredExtensions = false;
                break;
            }
        }

        if (!supportsAllRequiredExtensions) {
            return false;
        }

        auto features = physicalDevice.template getFeatures2
            <
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            >();
        bool supportsRequiredFeatures =
            features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        if (!supportsRequiredFeatures) {
            return false;
        }

        if (physicalDevice.getSurfaceFormatsKHR(*surface).empty() ||
            physicalDevice.getSurfacePresentModesKHR(*surface).empty()) {
            return false;
        }

        return true;
    }

    void VulkanRenderer::pickPhysicalDevice() {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = _instance.enumeratePhysicalDevices();
        for (const auto& physicalDevice : physicalDevices) {
            if (isDeviceSuitable(physicalDevice, _surface, _requiredDeviceExtension)) {
                _physicalDevice = physicalDevice;
                break;
            }
        }

        if (_physicalDevice == nullptr)
        {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
    }

    void VulkanRenderer::createLogicalDevice() {
        auto queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                _physicalDevice.getSurfaceSupportKHR(qfpIndex, *_surface))
            {
                _queueIndex = qfpIndex;
                break;
            }
        }
        if (_queueIndex == ~0)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        > featureChain = {
                {
                    .features = {
                        .samplerAnisotropy = true
                    }
                },
                {
                    .synchronization2 = true,
                    .dynamicRendering = true
                },
                {
                    .extendedDynamicState = true
                }
        };

        // create a Device with single queue
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = _queueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(_requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = _requiredDeviceExtension.data()
        };

        _device = vk::raii::Device(_physicalDevice, deviceCreateInfo);
        _queue = vk::raii::Queue(_device, _queueIndex, 0);
    }

    void VulkanRenderer::createSwapchain() {
        auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
        auto formats = _physicalDevice.getSurfaceFormatsKHR(*_surface);
        auto presentModes = _physicalDevice.getSurfacePresentModesKHR(*_surface);

        vk::SurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& format : formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                surfaceFormat = format;
                break;
            }
        }

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        for (const auto& mode : presentModes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                presentMode = mode;
                break;
            }
        }

        vk::Extent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<uint32_t>::max()) {
            extent.width = std::clamp(
                _application.window->width(),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );
            extent.height = std::clamp(
                _application.window->height(),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            );
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo{
            .surface = *_surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = vk::True,
            .oldSwapchain = _swapchain != nullptr ? *_swapchain : VK_NULL_HANDLE
        };

        _swapchain = vk::raii::SwapchainKHR(_device, createInfo);
        _swapchainImages = _swapchain.getImages();
        _swapchainImageFormat = surfaceFormat.format;
        _swapchainExtent = extent;
    }

    void VulkanRenderer::createImageViews() {
        _swapchainImageViews.clear();
        _swapchainImageViews.reserve(_swapchainImages.size());

        for (auto image : _swapchainImages) {
            _swapchainImageViews.emplace_back(
                createImageView(image, _swapchainImageFormat, vk::ImageAspectFlagBits::eColor, 1)
            );
        }
    }

    vk::SampleCountFlagBits VulkanRenderer::getMaxSampleCount() {
        auto limits = _physicalDevice.getProperties().limits;
        auto counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        for (auto count : { vk::SampleCountFlagBits::e8, vk::SampleCountFlagBits::e4, vk::SampleCountFlagBits::e2 }) {
            if (counts & count) return count;
        }
        return vk::SampleCountFlagBits::e1;
    }

    void VulkanRenderer::createColorResources() {
        auto [image, memory] = createImage(
            _swapchainExtent.width, _swapchainExtent.height,
            _swapchainImageFormat,
            1, _msaaSamples,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        _colorImage = std::move(image);
        _colorImageMemory = std::move(memory);
        _colorImageView = vk::raii::ImageView(_device, vk::ImageViewCreateInfo{
            .image = *_colorImage,
            .viewType = vk::ImageViewType::e2D,
            .format = _swapchainImageFormat,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1
            }
        });
    }

    void VulkanRenderer::createDepthResources() {
        _depthFormat = findSupportedFormat(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
        std::tie(_depthImage, _depthImageMemory) = createImage(
            _swapchainExtent.width, _swapchainExtent.height,
            _depthFormat,
            1,
            _msaaSamples,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        _depthImageView = createImageView(_depthImage, _depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
    }

    void VulkanRenderer::createShadowShader(const tr::Data::Shader& shader) {
        const auto& code = shader.getCode();
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule{ _device, createInfo };

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = shader.vertName.c_str()
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo };

        auto bindingDescription = getBindingDescription();
        auto attributeDescriptions = getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()
        };
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList
        };
        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1
        };
		
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::True,
            .depthBiasConstantFactor = 1.25f,
            .depthBiasSlopeFactor = 1.75f,
            .lineWidth = 1.0f
        };

		vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1, 
            .sampleShadingEnable = vk::False
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
        };

		std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport, 
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
             .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
             .pDynamicStates = dynamicStates.data() 
        };

        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = 2 * sizeof(glm::mat4)
        };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };
        _shadowPipelineLayout = vk::raii::PipelineLayout(_device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {
                    .stageCount = 1,
                    .pStages = shaderStages,
                    .pVertexInputState = &vertexInputInfo,
                    .pInputAssemblyState = &inputAssembly,
                    .pViewportState = &viewportState,
                    .pRasterizationState = &rasterizer,
                    .pMultisampleState = &multisampling,
                    .pDepthStencilState  = &depthStencil,
                    .pDynamicState = &dynamicState,
                    .layout = _shadowPipelineLayout,
                    .renderPass = nullptr,
                },
                {
                    .depthAttachmentFormat = _shadowFormat
                }
        };

        _shadowPipeline = vk::raii::Pipeline(
            _device,
            nullptr,
            pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
        );
    }

    void VulkanRenderer::createShadowResources() {
        std::tie(_shadowImage, _shadowImageMemory) = createImage(
            SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
            _shadowFormat,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        _shadowImageView = createImageView(_shadowImage, _shadowFormat, vk::ImageAspectFlagBits::eDepth, 1);

        setDebugName(vk::ObjectType::eImage, uint64_t(static_cast<VkImage>(*_shadowImage)), "ShadowMap");
        setDebugName(vk::ObjectType::eImageView, uint64_t(static_cast<VkImageView>(*_shadowImageView)), "ShadowMapView");

        vk::SamplerCreateInfo samplerInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .addressModeU = vk::SamplerAddressMode::eClampToBorder,
            .addressModeV = vk::SamplerAddressMode::eClampToBorder,
            .addressModeW = vk::SamplerAddressMode::eClampToBorder,
            .compareEnable = vk::True,
            .compareOp = vk::CompareOp::eLess,
            .borderColor = vk::BorderColor::eFloatOpaqueWhite,
        };
        _shadowSampler = vk::raii::Sampler(_device, samplerInfo);
    }

    void VulkanRenderer::createFallbackTexture() {
        Data::Texture tex;
        tex.width = 1;
        tex.height = 1;
        tex.channels = 4;
        tex.pixels = { 0xFF, 0xFF, 0xFF, 0xFF };

        auto [stagingBuffer, stagingMemory] = createBuffer(
            4,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        void* data = stagingMemory.mapMemory(0, 4);
        memcpy(data, tex.pixels.data(), 4);
        stagingMemory.unmapMemory();

        std::tie(_fallbackImage, _fallbackImageMemory) = createImage(
            1, 1,
            vk::Format::eR8G8B8A8Srgb,
            1, vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        transitionImageLayout(
            _fallbackImage,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            1
        );
        copyBufferToImage(stagingBuffer, _fallbackImage, 1, 1);
        transitionImageLayout(
            _fallbackImage,
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            1
        );

        _fallbackImageView = createImageView(
            *_fallbackImage,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageAspectFlagBits::eColor,
            1
        );

        _fallbackSampler = vk::raii::Sampler(
            _device,
            vk::SamplerCreateInfo{
                .magFilter = vk::Filter::eNearest,
                .minFilter = vk::Filter::eNearest,
                .mipmapMode = vk::SamplerMipmapMode::eNearest,
                .addressModeU = vk::SamplerAddressMode::eRepeat,
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat,
            }
        );
    }

	void VulkanRenderer::createDescriptorSetLayout() {
		std::array bindings = {
            vk::DescriptorSetLayoutBinding(
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                nullptr
            ),
            vk::DescriptorSetLayoutBinding(
                1,
                vk::DescriptorType::eCombinedImageSampler,
                1,
                vk::ShaderStageFlagBits::eFragment,
                nullptr
            ),
        };
        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };
        _descriptorSetLayout = vk::raii::DescriptorSetLayout(_device, layoutInfo);
    }

    void VulkanRenderer::createCommandPool() {
        vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = _queueIndex
        };

        _commandPool = vk::raii::CommandPool(_device, createInfo);
    }

    void VulkanRenderer::createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = *_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };

        _commandBuffers = vk::raii::CommandBuffers(_device, allocateInfo);
    }

    void VulkanRenderer::createUniformBuffers() {
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
        _uniformBuffersMapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DeviceSize bufferSize = sizeof(SceneData);
			auto [buffer, bufferMem]  = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );
            _uniformBuffers.emplace_back(std::move(buffer));
			_uniformBuffersMemory.emplace_back(std::move(bufferMem));
			_uniformBuffersMapped.emplace_back(_uniformBuffersMemory.back().mapMemory(0, bufferSize));
		}
    }

	void VulkanRenderer::createDescriptorPool() {
		std::array poolSize {
		    vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_MATERIALS + MAX_FRAMES_IN_FLIGHT)
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT + MAX_MATERIALS,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()
        };
        _descriptorPool = vk::raii::DescriptorPool(_device, poolInfo);
    }

    void VulkanRenderer::createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *_descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

		_descriptorSets = _device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
            vk::DescriptorBufferInfo bufferInfo{
                .buffer = _uniformBuffers[i],
                .offset = 0,
                .range = sizeof(SceneData)
            };
            vk::WriteDescriptorSet sceneDataDescriptorWrite{
                .dstSet = _descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &bufferInfo
            };
            vk::DescriptorImageInfo shadowMapInfo{
                .sampler = _shadowSampler,
                .imageView = _shadowImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            };
            vk::WriteDescriptorSet shadowMapDescriptorWrite{
                .dstSet = _descriptorSets[i],
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &shadowMapInfo
            };
            _device.updateDescriptorSets({ sceneDataDescriptorWrite,shadowMapDescriptorWrite }, {});
		}
    }

    void VulkanRenderer::createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _imageAvailableSemaphores[i] = vk::raii::Semaphore(_device, semaphoreInfo);
            _renderFinishedSemaphores[i] = vk::raii::Semaphore(_device, semaphoreInfo);
            _inFlightFences[i] = vk::raii::Fence(_device, fenceInfo);
        }
    }

    void VulkanRenderer::cleanupSwapchain() {
        _swapchainImageViews.clear();
        _swapchainImages.clear();
        _swapchain = nullptr;
    }

    void VulkanRenderer::recreateSwapchain() {
        if (_application.window->width() == 0 || _application.window->height() == 0) {
            _swapchainDirty = true;
            return;
        }

        _device.waitIdle();
        cleanupSwapchain();
        createSwapchain();
        createImageViews();
        createColorResources();
		createDepthResources();
        _swapchainDirty = false;
    }

#pragma endregion
    
#pragma region Resources
    
    void VulkanRenderer::createShader(
        tr::Resources::Handle<tr::Data::Shader> handle,
        const tr::Data::Shader& shader
    ) {
        const auto& code = shader.getCode();
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule{ _device, createInfo };

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = shader.vertName.c_str()
        };
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = shader.fragName.c_str()
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        auto bindingDescription = getBindingDescription();
        auto attributeDescriptions = getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()
        };
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList
        };
        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1
        };

		vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f
        };
        switch (shader.cullMode)
        {
        case tr::Data::CullMode::None:
            rasterizer.cullMode = vk::CullModeFlagBits::eNone;
            break;
        case tr::Data::CullMode::Front:
            rasterizer.cullMode = vk::CullModeFlagBits::eFront;
            break;
        case tr::Data::CullMode::Back:
            rasterizer.cullMode = vk::CullModeFlagBits::eBack;
            break;
        case tr::Data::CullMode::Both:
            rasterizer.cullMode = vk::CullModeFlagBits::eFrontAndBack;
            break;
        
        default:
            rasterizer.cullMode = vk::CullModeFlagBits::eBack;
            break;
        }

		vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = _msaaSamples, 
            .sampleShadingEnable = vk::False
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        if (shader.blendMode == Data::BlendMode::Transparent) {
            colorBlendAttachment = {
                .blendEnable = vk::True,
                .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                .colorBlendOp = vk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                .alphaBlendOp = vk::BlendOp::eAdd,
                .colorWriteMask =
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA
            };
            depthStencil = {
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::False,
                .depthCompareOp = vk::CompareOp::eLess,
            };
        }
        else {
            colorBlendAttachment = {
                .blendEnable = vk::False,
                .colorWriteMask =
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA
            };
            depthStencil = {
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::True,
                .depthCompareOp = vk::CompareOp::eLess,
            };
        }

		vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy, 
            .attachmentCount = 1, 
            .pAttachments = &colorBlendAttachment
        };

		std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport, 
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
             .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
             .pDynamicStates = dynamicStates.data() 
        };

        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(glm::mat4) 
        };

        std::vector<vk::DescriptorSetLayoutBinding> shaderBindings;
        shaderBindings.push_back(vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        });
        uint32_t textureBinding = 1;
        for (const auto& param : shader.params) {
            if (shader.isTextureParam(param.defaultValue)) {
                shaderBindings.push_back(vk::DescriptorSetLayoutBinding{
                    .binding = textureBinding++,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eFragment
                });
            }
        }
        vk::raii::DescriptorSetLayout shaderDescriptorSetLayout(
            _device,
            vk::DescriptorSetLayoutCreateInfo {
                .bindingCount = static_cast<uint32_t>(shaderBindings.size()),
                .pBindings = shaderBindings.data()
            }
        );

        std::array setLayouts = { *_descriptorSetLayout, *shaderDescriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
            .pSetLayouts = setLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };
        vk::raii::PipelineLayout pipelineLayout = vk::raii::PipelineLayout(_device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {
                    .stageCount = 2,
                    .pStages = shaderStages,
                    .pVertexInputState = &vertexInputInfo,
                    .pInputAssemblyState = &inputAssembly,
                    .pViewportState = &viewportState,
                    .pRasterizationState = &rasterizer,
                    .pMultisampleState = &multisampling,
                    .pDepthStencilState  = &depthStencil,
                    .pColorBlendState = &colorBlending,
                    .pDynamicState = &dynamicState,
                    .layout = pipelineLayout,
                    .renderPass = nullptr,
                },
                {
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &_swapchainImageFormat,
                    .depthAttachmentFormat = _depthFormat
                }
        };

        vk::raii::Pipeline graphicsPipeline = vk::raii::Pipeline(_device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

        _shadersMap[handle] = {
            handle,
            std::move(pipelineLayout),
            std::move(graphicsPipeline),
            std::move(shaderDescriptorSetLayout)
        };
    }
    
    void VulkanRenderer::createTexture(
        tr::Resources::Handle<tr::Data::Texture> handle,
        const tr::Data::Texture& texture
    ) {
		vk::DeviceSize imageSize = texture.width * texture.height * texture.channels;
        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            imageSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

		void *data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, texture.pixels.data(), imageSize);
        stagingBufferMemory.unmapMemory();
        
        VulkanTexture result;

        result.mipLevels = static_cast<uint32_t>(
                std::floor(std::log2(std::max(texture.width, texture.height)))
            ) + 1;

        std::tie(result.textureImage, result.textureImageMemory) = createImage(
            texture.width, texture.height,
            vk::Format::eR8G8B8A8Srgb,
            result.mipLevels,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        transitionImageLayout(
            result.textureImage,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            result.mipLevels
        );
        copyBufferToImage(
            stagingBuffer,
            result.textureImage,
            static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height)
        );

        generateMipmaps(
            result.textureImage,
            vk::Format::eR8G8B8A8Srgb,
            texture.width, texture.height,
            result.mipLevels
        );

        result.textureImageView = createImageView(
            result.textureImage,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageAspectFlagBits::eColor,
            result.mipLevels
        );
        
        vk::PhysicalDeviceProperties properties = _physicalDevice.getProperties();
        
        vk::SamplerCreateInfo samplerInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways
        };
        result.textureSampler = vk::raii::Sampler(_device, samplerInfo);

        _texturesMap[handle] = std::move(result);
    }
    
    void VulkanRenderer::createMaterial(
        tr::Resources::Handle<tr::Data::Material> handle,
        const tr::Data::Material& material,
        const tr::Data::Shader& shader
    ) {
        VulkanMaterial result{
            .source = handle,
            .shader = material.shader,
        };
        auto& vulkanShader = _shadersMap[result.shader];

        std::vector<uint8_t> uboData;
        for (const auto& desc : shader.params) {
            const auto& value = material.get(desc.name, shader);
            auto info = shader.getParamTypeInfo(value);
            if (info.size == 0) continue;

            uint32_t offset = (static_cast<uint32_t>(uboData.size()) + info.alignment - 1) & ~(info.alignment - 1);
            uboData.resize(offset + info.size);
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (!std::is_same_v<T, Resources::Handle<Data::Texture>>) {
                    memcpy(uboData.data() + offset, &val, sizeof(val));
                }
            }, value);
        }
        if (uboData.empty()) uboData.resize(4, 0);

        vk::DeviceSize uboSize = uboData.size();
        auto [stagingBuffer, stagingMemory] = createBuffer(
            uboSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        void* mapped = stagingMemory.mapMemory(0, uboSize);
        memcpy(mapped, uboData.data(), uboSize);
        stagingMemory.unmapMemory();
        std::tie(result.paramsBuffer, result.paramsMemory) = createBuffer(
            uboSize,
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        copyBuffer(stagingBuffer, result.paramsBuffer, uboSize);
        
        result.descriptorSet = std::move(_device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo {
                .descriptorPool = _descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &*vulkanShader.descriptorSetLayout
            }
        ).front());

        std::vector<vk::DescriptorImageInfo> imageInfos;
        std::vector<vk::WriteDescriptorSet> writes;

        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *result.paramsBuffer,
            .offset = 0,
            .range = uboSize
        };
        writes.push_back(vk::WriteDescriptorSet{
            .dstSet = *result.descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo
        });

        uint32_t textureBinding = 1;
        for (const auto& desc : shader.params) {
            const auto& value = material.get(desc.name, shader);
            if (!shader.isTextureParam(value)) continue;
            auto texHandle = std::get<Resources::Handle<Data::Texture>>(value);
            if (texHandle.isValid()) {
                auto& tex = _texturesMap[texHandle];
                imageInfos.push_back({
                    .sampler = *tex.textureSampler,
                    .imageView = *tex.textureImageView,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                });
            }
            else {
                imageInfos.push_back({
                    .sampler = *_fallbackSampler,
                    .imageView = *_fallbackImageView,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                });
            }

            writes.push_back(vk::WriteDescriptorSet{
                .dstSet = *result.descriptorSet,
                .dstBinding = textureBinding++,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &imageInfos.back()
            });
        }
        _device.updateDescriptorSets(writes, {});

        _materialsMap[handle] = std::move(result);
    }
    
    void VulkanRenderer::createMesh(
        tr::Resources::Handle<tr::Data::Mesh> handle,
        const tr::Data::Mesh& mesh
    ) {
        VulkanMesh result{
            .source = handle
        };

        { // vertex buffer
            vk::DeviceSize bufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();
            auto [stagingBuffer, stagingBufferMemory] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );
            void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(dataStaging, mesh.vertices.data(), bufferSize);
            stagingBufferMemory.unmapMemory();
            std::tie(result.vertexBuffer, result.vertexBufferMemory) = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
            copyBuffer(stagingBuffer, result.vertexBuffer, bufferSize);
        }
        { // index buffer
            vk::DeviceSize bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();

            auto [stagingBuffer, stagingBufferMemory] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

            void* data = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(data, mesh.indices.data(), (size_t)bufferSize);
            stagingBufferMemory.unmapMemory();
            std::tie(result.indexBuffer, result.indexBufferMemory) = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );

            copyBuffer(stagingBuffer, result.indexBuffer, bufferSize);
        }

        uint32_t start = 0;
        for (auto subMeshSize : mesh.subMeshData) {
            result.subMeshesLayouts.push_back({ start, subMeshSize });
            start += subMeshSize;
        }

        _meshesMap[handle] = std::move(result);
    }

#pragma endregion

#pragma region Frame

    void VulkanRenderer::startFrame(const tr::Rendering::SceneData& sceneData) {
        if (_frameStarted
            || _application.window->width() == 0
            || _application.window->height() == 0
        ) {
            return;
        }

        if (_swapchainDirty) {
            recreateSwapchain();
            if (_swapchainDirty) {
                return;
            }
        }

        auto& fence = _inFlightFences[_currentFrame];
        
        while (
            _device.waitForFences(
                { *fence },
                vk::True,
                std::numeric_limits<uint64_t>::max()
            ) != vk::Result::eSuccess
        ) {
        }

        try {
            auto result = _swapchain.acquireNextImage(
                std::numeric_limits<uint64_t>::max(),
                *_imageAvailableSemaphores[_currentFrame],
                nullptr
            );
            _currentImageIndex = result.value;
            if (result.result == vk::Result::eSuboptimalKHR) {
                _swapchainDirty = true;
            }
        }
        catch (const vk::OutOfDateKHRError&) {
            recreateSwapchain();
            return;
        }

        _device.resetFences({ *fence });

        auto& commandBuffer = _commandBuffers[_currentFrame];
        commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);

        _lastSceneData = sceneData;
        // Flip Y axis for Vulkan clip space
        _lastSceneData.proj[1][1] *= -1;
        memcpy(_uniformBuffersMapped[_currentFrame], &_lastSceneData, sizeof(_lastSceneData));

        _frameStarted = true;
    }

    void VulkanRenderer::startShadowPass() {
        if (!_frameStarted) {
            return;
        }

        transitionImageLayout(
            *_shadowImage,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
            {}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        vk::RenderingAttachmentInfo depthAttachment{
            .imageView = *_shadowImageView,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearDepthStencilValue(1.0f, 0)
        };
        vk::RenderingInfo renderingInfo{
            .renderArea = { {0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} },
            .layerCount = 1,
            .pDepthAttachment = &depthAttachment
        };

        auto& commandBuffer = _commandBuffers[_currentFrame];
        
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Shadow Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D({0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}));
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_shadowPipeline);
    }

    void VulkanRenderer::drawShadows(const DrawCommand& command) {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];
        auto meshIt = _meshesMap.find(command.mesh);
        if (meshIt == _meshesMap.end()) {
            throw std::runtime_error("Can't use not registered mesh #" + command.mesh.index);
        }
        const auto& meshData = meshIt->second;
        commandBuffer.bindVertexBuffers(0, *meshData.vertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(*meshData.indexBuffer, 0, vk::IndexType::eUint32);
        for (int i = 0; i < command.materials.size(); ++i) {
            commandBuffer.pushConstants(
                _shadowPipelineLayout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                vk::ArrayProxy<const glm::mat4>({ _lastSceneData.lightViewProj, command.modelMatrix })
            );
            auto subMeshLayout = meshData.subMeshesLayouts[i];
            commandBuffer.drawIndexed(subMeshLayout.indexCount, 1, subMeshLayout.firstIndex, 0, 0);
        }
    }

    void VulkanRenderer::endShadowPass() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];
        commandBuffer.endRendering();
        commandBuffer.endDebugUtilsLabelEXT();
        transitionImageLayout(
            *_shadowImage,
            vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests, vk::PipelineStageFlagBits2::eFragmentShader,
            vk::ImageAspectFlagBits::eDepth
        );
    }

    void VulkanRenderer::startColorPass() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Final Color Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);

        transitionImageLayout(
            _swapchainImages[_currentImageIndex],
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
            {},  vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );
        transitionImageLayout(
            *_colorImage,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );
		transitionImageLayout(
		    *_depthImage,
		    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
		    vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        vk::ClearValue clearValue;
        clearValue.color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *_colorImageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode = vk::ResolveModeFlagBits::eAverage,
            .resolveImageView = *_swapchainImageViews[_currentImageIndex],
            .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = clearValue
        };
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
        vk::RenderingAttachmentInfo depthAttachment = {
            .imageView = _depthImageView,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = clearDepth
        };
        vk::RenderingInfo renderingInfo{
            .renderArea = {
                .offset = {0, 0},
                .extent = _swapchainExtent
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachment
        };

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.setViewport(0, vk::Viewport(
            0.0f, 0.0f,
            static_cast<float>(_swapchainExtent.width), static_cast<float>(_swapchainExtent.height),
            0.0f, 1.0f
        ));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapchainExtent));
    }

    void VulkanRenderer::draw(const DrawCommand& command) {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];
        auto meshIt = _meshesMap.find(command.mesh);
        if (meshIt == _meshesMap.end()) {
            throw std::runtime_error("Can't use not registered mesh #" + command.mesh.index);
        }
        const auto& meshData = meshIt->second;
        commandBuffer.bindVertexBuffers(0, *meshData.vertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(*meshData.indexBuffer, 0, vk::IndexType::eUint32);
        for (int i = 0; i < command.materials.size(); ++i) {
            const auto& material = command.materials[i];
            auto materialIt = _materialsMap.find(material);
            if (materialIt == _materialsMap.end()) {
                throw std::runtime_error("Can't use not registered material #" + material.index);
            }
            auto shaderIt = _shadersMap.find(materialIt->second.shader);
            if (shaderIt == _shadersMap.end()) {
                throw std::runtime_error("Can't use not registered shader #" + materialIt->second.shader.index);
            }
            const auto& pipeline = shaderIt->second;
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.graphicsPipeline);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipeline.pipelineLayout,
                0,
                { *_descriptorSets[_currentFrame], *materialIt->second.descriptorSet },
                nullptr
            );
            commandBuffer.pushConstants(
                pipeline.pipelineLayout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                vk::ArrayProxy<const glm::mat4>(command.modelMatrix)
            );
            auto subMeshLayout = meshData.subMeshesLayouts[i];
            commandBuffer.drawIndexed(subMeshLayout.indexCount, 1, subMeshLayout.firstIndex, 0, 0);
        }
    }

    void VulkanRenderer::endColorPass() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];
        commandBuffer.endRendering();
        commandBuffer.endDebugUtilsLabelEXT();
        transitionImageLayout(
            _swapchainImages[_currentImageIndex],
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite, {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::ImageAspectFlagBits::eColor
        );
        commandBuffer.end();
    }
    
    void VulkanRenderer::endFrame() {
        if (!_frameStarted) {
            return;
        }

        vk::CommandBufferSubmitInfo commandBufferInfo{
            .commandBuffer = _commandBuffers[_currentFrame]
        };
        vk::SemaphoreSubmitInfo waitSemaphoreInfo{
            .semaphore = *_imageAvailableSemaphores[_currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };
        vk::SemaphoreSubmitInfo signalSemaphoreInfo{
            .semaphore = *_renderFinishedSemaphores[_currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };
        vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo
        };

        _queue.submit2({ submitInfo }, *_inFlightFences[_currentFrame]);

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*_renderFinishedSemaphores[_currentFrame],
            .swapchainCount = 1,
            .pSwapchains = &*_swapchain,
            .pImageIndices = &_currentImageIndex
        };

        try {
            auto result = _queue.presentKHR(presentInfo);
            if (result == vk::Result::eSuboptimalKHR) {
                _swapchainDirty = true;
            }
        } catch (const vk::OutOfDateKHRError&) {
            _swapchainDirty = true;
        }

        _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        _frameStarted = false;
    }

#pragma endregion
}
