#include "vulkan_renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    const static std::vector<const char*> _requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName
    };

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
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        _initialized = true;
    }

    #pragma region Vulkan initialization

    void VulkanRenderer::createInstance()
    {
        uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

        vk::ApplicationInfo appInfo{
            .pApplicationName = _application.name.data(),
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: parse application version?
            .pEngineName = _application.name.data(),
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = extensionCount,
            .ppEnabledExtensionNames = extensions
        };

        _instance = vk::raii::Instance(_context, createInfo);
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

    void VulkanRenderer::recordClearCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);

        vk::ImageMemoryBarrier2 toColorAttachment{
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = _swapchainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::DependencyInfo toColorAttachmentDependency{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toColorAttachment
        };
        commandBuffer.pipelineBarrier2(toColorAttachmentDependency);

        vk::ClearValue clearValue;
        clearValue.color = vk::ClearColorValue(std::array<float, 4>{0.03f, 0.05f, 0.08f, 1.0f});
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
        commandBuffer.endRendering();

        vk::ImageMemoryBarrier2 toPresent{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eNone,
            .dstAccessMask = vk::AccessFlagBits2::eNone,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = _swapchainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::DependencyInfo toPresentDependency{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toPresent
        };
        commandBuffer.pipelineBarrier2(toPresentDependency);

        commandBuffer.end();
    }

    #pragma endregion

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
        _queue = nullptr;
        _device = nullptr;
        _physicalDevice = nullptr;
        _surface = nullptr;
        _instance = nullptr;
        _initialized = false;
    }

    void VulkanRenderer::clearResources() {
    }

    void VulkanRenderer::resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            _swapchainDirty = true;
            return;
        }

        recreateSwapchain();
    }

    void VulkanRenderer::createShader(
        tr::Resources::Handle<tr::Data::Shader> handle,
        const tr::Data::Shader& shader
    ) {
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
    }
    
    void VulkanRenderer::createMesh(
        tr::Resources::Handle<tr::Data::Mesh> handle,
        const tr::Data::Mesh& mesh
    ) {
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
        while (_device.waitForFences({*fence}, vk::True, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
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

        _device.resetFences({*fence});
        _commandBuffers[_currentFrame].reset();
        recordClearCommands(*_commandBuffers[_currentFrame], _currentImageIndex);
        _frameStarted = true;
    }

    void VulkanRenderer::draw(const DrawCommand& command) {
    }
    
    void VulkanRenderer::endFrame() {
        if (!_frameStarted) {
            return;
        }

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

        _queue.submit2({submitInfo}, *_inFlightFences[_currentFrame]);

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
}
