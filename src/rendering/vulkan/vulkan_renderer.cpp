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

#include "uniform_buffer_object.h"

namespace tr::Rendering::Vulkan {
    VulkanRenderer::VulkanRenderer(const tr::App::Application& application)
        : _application(application) {
    }

    VulkanRenderer::~VulkanRenderer() {
    }

    void VulkanRenderer::init() {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createDescriptorSetLayout();
        createCommandPool();
        createCommandBuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
        createSyncObjects();
        _initialized = true;
    }

    void VulkanRenderer::shutdown() {
        if (!_initialized) {
            return;
        }

        _device.waitIdle();
        cleanupSwapchain();
        _commandBuffers.clear();
        _commandPool = nullptr;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _imageAvailableSemaphores[i] = nullptr;
            _renderFinishedSemaphores[i] = nullptr;
            _inFlightFences[i] = nullptr;
        }
        clearResources();

        _descriptorSetLayout = nullptr;
        _descriptorPool = nullptr;
        _descriptorSets.clear();
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
        _uniformBuffers.clear();
        _queue = nullptr;
        _device = nullptr;
        _physicalDevice = nullptr;
        _surface = nullptr;
        _instance = nullptr;
        _initialized = false;
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

#pragma region Vulkan initialization
        
#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

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
            vk::ImageViewCreateInfo createInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = _swapchainImageFormat,
                .components = {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity
                },
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            _swapchainImageViews.emplace_back(_device, createInfo);
        }
    }

	void VulkanRenderer::createDescriptorSetLayout() {
		vk::DescriptorSetLayoutBinding uboLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex
        };
        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            .bindingCount = 1,
            .pBindings = &uboLayoutBinding
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
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
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
        vk::DescriptorPoolSize poolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
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
			vk::DescriptorBufferInfo bufferInfo{.buffer = _uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
            vk::WriteDescriptorSet   descriptorWrite{
                .dstSet = _descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &bufferInfo
            };
            _device.updateDescriptorSets(descriptorWrite, {});
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
        _swapchainDirty = false;
    }

#pragma endregion

#pragma region Helpers

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0, 
            .stride = sizeof(tr::Data::Mesh::Vertex), 
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions()
    {
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

    void VulkanRenderer::transitionImageLayout(
	    uint32_t imageIndex,
	    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	    vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask
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
            .image = _swapchainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
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
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f
        };

		vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1, 
            .sampleShadingEnable = vk::False
        };

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		    .blendEnable = vk::False,
            .colorWriteMask =
                  vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA 
        };

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

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &*_descriptorSetLayout,
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
                    .pColorBlendState = &colorBlending,
                    .pDynamicState = &dynamicState,
                    .layout = pipelineLayout,
                    .renderPass = nullptr
                },
                {
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &_swapchainImageFormat
                }
        };

        vk::raii::Pipeline graphicsPipeline = vk::raii::Pipeline(_device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

        _shadersMap[handle] = { handle, std::move(pipelineLayout), std::move(graphicsPipeline) };
    }
    
    void VulkanRenderer::createTexture(
        tr::Resources::Handle<tr::Data::Texture> handle,
        const tr::Data::Texture& texture
    ) {
    }
    
    void VulkanRenderer::createMaterial(
        tr::Resources::Handle<tr::Data::Material> handle,
        const tr::Data::Material& material
    ) {
        _materialsMap[handle] = { handle, material.getShader() };
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

    void VulkanRenderer::recordFrameStartCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);

        transitionImageLayout(
            imageIndex,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearValue;
        clearValue.color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *_swapchainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearValue
        };
        vk::RenderingInfo renderingInfo{
            .renderArea = {
                .offset = {0, 0},
                .extent = _swapchainExtent
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.setViewport(0, vk::Viewport(
            0.0f, 0.0f,
            static_cast<float>(_swapchainExtent.width), static_cast<float>(_swapchainExtent.height),
            0.0f, 1.0f
        ));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapchainExtent));
    }

    void VulkanRenderer::recordFrameEndCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        commandBuffer.endRendering();
        transitionImageLayout(
            _currentImageIndex,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eNone
        );
        commandBuffer.end();
    }

    void VulkanRenderer::startFrame(const tr::Data::Camera& camera) {
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
        while (_device.waitForFences({ *fence }, vk::True, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
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
        } catch (const vk::OutOfDateKHRError&) {
            recreateSwapchain();
            return;
        }
        
        UniformBufferObject ubo{
            .view = glm::inverse(camera.transform.getMatrix()),
            .proj = glm::perspective(
                glm::radians(camera.fov),
                static_cast<float>(_swapchainExtent.width) / static_cast<float>(_swapchainExtent.height),
                camera.near, camera.far)
        };
        ubo.proj[1][1] *= -1; // Flip Y axis for Vulkan clip space
        memcpy(_uniformBuffersMapped[_currentImageIndex], &ubo, sizeof(ubo));

        _device.resetFences({ *fence });
        
        auto& commandBuffer = *_commandBuffers[_currentFrame];
        commandBuffer.reset();
        recordFrameStartCommands(commandBuffer, _currentImageIndex);
        _frameStarted = true;
    }

    void VulkanRenderer::draw(const DrawCommand& command) {
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
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, *_descriptorSets[_currentFrame], nullptr);
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
    
    void VulkanRenderer::endFrame() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = *_commandBuffers[_currentFrame];
        recordFrameEndCommands(commandBuffer, _currentImageIndex);

        vk::CommandBufferSubmitInfo commandBufferInfo{
            .commandBuffer = *_commandBuffers[_currentFrame]
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
