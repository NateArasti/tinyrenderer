#include "vulkan_rhi.h"

#include "vulkan_utility.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tr::Rendering::Vulkan {
    VulkanRHI::VulkanRHI(const tr::App::Application& application, ValidationOptions validation) :
        _application(application),
        _vulkanContext(application, std::move(validation)),
        _allocator(_vulkanContext),
        _swapchain(_vulkanContext, application.window->width(), application.window->height())
    {
        _resources = std::make_unique<VulkanResources>(_vulkanContext, _allocator);
        _commandAdapter = std::make_unique<VulkanCommandBuffer>(_vulkanContext, *_resources);
        _uiRenderer = std::make_unique<VulkanUIRenderer>(_vulkanContext, _swapchain);
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        createTimestampQueries();
        importBackbuffers();
        createFrameTargets();
    }

    VulkanRHI::~VulkanRHI() {
        _vulkanContext.device.waitIdle();
        destroyFrameTargets();
        for (const auto handle : _backbuffers) {
            _resources->forgetTexture(handle);
        }
    }

    std::string VulkanRHI::getDeviceName() const {
        return _vulkanContext.physicalDevice.getProperties().deviceName.data();
    }

    std::optional<App::MemoryUsage> VulkanRHI::getMemoryUsage() const {
        if (!_vulkanContext.memoryBudgetSupported()) {
            return std::nullopt;
        }

        const auto properties = _vulkanContext.physicalDevice.getMemoryProperties2<
            vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT
        >();
        const auto& memory = properties.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;
        const auto& budget = properties.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();

        App::MemoryUsage usage;
        for (uint32_t i = 0; i < memory.memoryHeapCount; ++i) {
            if (memory.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
                usage.usedBytes += budget.heapUsage[i];
                usage.totalBytes += memory.memoryHeaps[i].size;
            }
        }
        if (usage.totalBytes == 0) {
            return std::nullopt;
        }
        return usage;
    }

    bool VulkanRHI::isDeviceMemoryShared() const {
        const auto type = _vulkanContext.physicalDevice.getProperties().deviceType;
        return type == vk::PhysicalDeviceType::eIntegratedGpu || type == vk::PhysicalDeviceType::eCpu;
    }

    Format VulkanRHI::getDepthFormat() const {
        return fromVulkanFormat(_vulkanContext.depthFormat);
    }

    void VulkanRHI::waitIdle() {
        _vulkanContext.device.waitIdle();
    }

    void VulkanRHI::resize(uint32_t, uint32_t) {
        _swapchainDirty = true;
        recreateSwapchain();
    }

    void VulkanRHI::importBackbuffers() {
        for (uint32_t i = 0; i < _swapchain.imageCount(); ++i) {
            _backbuffers.push_back(_resources->importTexture(
                "Backbuffer " + std::to_string(i),
                _swapchain.image(i),
                *_swapchain.imageView(i)
            ));
            _backbufferStates.push_back(TextureState::Undefined);
        }
    }

    void VulkanRHI::createFrameTargets() {
        const auto extent = _swapchain.extent();
        const auto samples = getMsaaSamples();
        for (uint32_t i = 0; i < _swapchain.imageCount(); ++i) {
            if (samples > 1) {
                _msaaTargets.push_back(_resources->createTexture({
                    .name = "MSAA color " + std::to_string(i),
                    .width = extent.width,
                    .height = extent.height,
                    .format = HDR_FORMAT,
                    .samples = samples,
                }));
            }
            _hdrTargets.push_back(_resources->createTexture({
                .name = "HDR " + std::to_string(i),
                .width = extent.width,
                .height = extent.height,
                .format = HDR_FORMAT,
                .samples = 1,
                .sampled = true,
            }));
            _depthTargets.push_back(_resources->createTexture({
                .name = "Depth " + std::to_string(i),
                .width = extent.width,
                .height = extent.height,
                .format = getDepthFormat(),
                .samples = samples,
            }));
        }
    }

    void VulkanRHI::destroyFrameTargets() {
        for (const auto handle : _hdrTargets) {
            _resources->destroyTexture(handle);
        }
        for (const auto handle : _msaaTargets) {
            _resources->destroyTexture(handle);
        }
        for (const auto handle : _depthTargets) {
            _resources->destroyTexture(handle);
        }
        _hdrTargets.clear();
        _msaaTargets.clear();
        _depthTargets.clear();
    }

    void VulkanRHI::recreateSwapchain() {
        if (!_application.window->width() || !_application.window->height()) {
            return;
        }
        _vulkanContext.device.waitIdle();
        destroyFrameTargets();
        for (auto handle : _backbuffers) {
            _resources->forgetTexture(handle);
        }
        _backbuffers.clear();
        _backbufferStates.clear();
        _swapchain.recreate(_application.window->width(), _application.window->height(), _vsyncEnabled);
        if (!supportsVsyncToggle()) {
            _vsyncEnabled = true;
        }
        importBackbuffers();
        createFrameTargets();
        _uiRenderer->recreate();
        _swapchainDirty = false;
    }
    
    UIRenderer& VulkanRHI::uiRenderer() {
        return *_uiRenderer;
    }

    uint32_t VulkanRHI::getMsaaSamples() const {
        return static_cast<uint32_t>(_vulkanContext.msaaSamples);
    }

    std::vector<uint32_t> VulkanRHI::getSupportedMsaaSamples() const {
        const auto limits = _vulkanContext.physicalDevice.getProperties().limits;
        const auto supported = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;

        std::vector<uint32_t> result;
        for (const auto samples :
            {
                vk::SampleCountFlagBits::e1,
                vk::SampleCountFlagBits::e2,
                vk::SampleCountFlagBits::e4,
                vk::SampleCountFlagBits::e8
            }
        ) {
            if (supported & samples) {
                result.push_back(static_cast<uint32_t>(samples));
            }
        }
        return result;
    }

    void VulkanRHI::setMsaaSamples(uint32_t samples) {
        const auto sampleCount = static_cast<vk::SampleCountFlagBits>(samples);
        const auto supported = getSupportedMsaaSamples();
        if (std::ranges::find(supported, samples) == supported.end()) {
            throw std::invalid_argument("MSAA sample count is not supported by this device");
        }
        if (_vulkanContext.msaaSamples == sampleCount) {
            return;
        }

        _vulkanContext.device.waitIdle();
        _vulkanContext.msaaSamples = sampleCount;
        destroyFrameTargets();
        createFrameTargets();
    }

    void VulkanRHI::setVsyncEnabled(bool enabled) {
        if (_vsyncEnabled == enabled || (!enabled && !supportsVsyncToggle())) {
            return;
        }

        _vsyncEnabled = enabled;
        _swapchainDirty = true;
    }

    void VulkanRHI::createCommandPool() {
        vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = _vulkanContext.queueIndex,
        };
        _commandPool = vk::raii::CommandPool(_vulkanContext.device, createInfo);
    }

    void VulkanRHI::createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = *_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };
        _commandBuffers = vk::raii::CommandBuffers(_vulkanContext.device, allocateInfo);
    }

    void VulkanRHI::createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _imageAvailableSemaphores[i] = vk::raii::Semaphore(_vulkanContext.device, semaphoreInfo);
            _inFlightFences[i] = vk::raii::Fence(_vulkanContext.device, fenceInfo);
        }
    }

    void VulkanRHI::createTimestampQueries() {
        const auto queueFamilies = _vulkanContext.physicalDevice.getQueueFamilyProperties();
        _timestampValidBits = queueFamilies[_vulkanContext.queueIndex].timestampValidBits;
        if (_timestampValidBits == 0) {
            return;
        }

        _timestampPeriod = _vulkanContext.physicalDevice.getProperties().limits.timestampPeriod;
        vk::QueryPoolCreateInfo createInfo{
            .queryType = vk::QueryType::eTimestamp,
            .queryCount = 2 * MAX_FRAMES_IN_FLIGHT,
        };
        _timestampQueries = vk::raii::QueryPool(_vulkanContext.device, createInfo);
    }

    std::optional<FrameContext> VulkanRHI::startFrame() {
        _frameWaitMilliseconds = 0.0;
        if (_frameStarted || _application.window->width() == 0 || _application.window->height() == 0) {
            return std::nullopt;
        }

        if (_swapchainDirty) {
            recreateSwapchain();
            return std::nullopt;
        }

        auto& fence = _inFlightFences[_currentFrame];

        const auto waitStart = std::chrono::steady_clock::now();
        while (_vulkanContext.device.waitForFences(
            { *fence },
            vk::True,
            std::numeric_limits<uint64_t>::max()
        ) != vk::Result::eSuccess) { }

        try {
            auto result = _swapchain.handle().acquireNextImage(
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
            _frameWaitMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - waitStart
            ).count();
            _swapchainDirty = true;
            return std::nullopt;
        }
        _frameWaitMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - waitStart
        ).count();

        const uint32_t firstQuery = 2 * _currentFrame;
        if (_timestampPending[_currentFrame]) {
            uint64_t timestamps[2]{};
            const auto result = _timestampQueries.getResults(
                firstQuery, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64
            );
            if (result == vk::Result::eSuccess) {
                const uint64_t mask = std::numeric_limits<uint64_t>::max() >> (64 - _timestampValidBits);
                const uint64_t ticks = (timestamps[1] - timestamps[0]) & mask;
                _gpuFrameMilliseconds = static_cast<double>(ticks) * _timestampPeriod / 1'000'000.0;
            }
            _timestampPending[_currentFrame] = false;
        }

        _resources->beginFrame(_currentFrame);

        auto& commandBuffer = _commandBuffers[_currentFrame];
        commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);
        if (_timestampValidBits != 0) {
            commandBuffer.resetQueryPool(*_timestampQueries, firstQuery, 2);
            commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *_timestampQueries, firstQuery);
        }

        _commandAdapter->reset(commandBuffer);
        _frameStarted = true;
        const auto extent = _swapchain.extent();
        return FrameContext{
            .cmd = *_commandAdapter,
            .slot = _currentFrame,
            .extent = { extent.width, extent.height },
            .backbuffer = _backbuffers.at(_currentImageIndex),
            .hdrTarget = _hdrTargets.at(_currentImageIndex),
            .colorTarget = _msaaTargets.empty()
                ? _hdrTargets.at(_currentImageIndex)
                : _msaaTargets.at(_currentImageIndex),
            .depthTarget = _depthTargets.at(_currentImageIndex),
            .colorFormat = fromVulkanFormat(_swapchain.format()),
            .backbufferInitialState = _backbufferStates.at(_currentImageIndex),
        };
    }

    void VulkanRHI::endFrame() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];

        if (_timestampValidBits != 0) {
            commandBuffer.writeTimestamp2(
                vk::PipelineStageFlagBits2::eAllCommands, *_timestampQueries, 2 * _currentFrame + 1
            );
        }
        commandBuffer.end();
        _vulkanContext.device.resetFences({ *_inFlightFences[_currentFrame] });

        vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = commandBuffer };
        // Include image layout transitions in the acquire and present semaphore scopes.
        vk::SemaphoreSubmitInfo waitSemaphoreInfo{
            .semaphore = *_imageAvailableSemaphores[_currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        };
        vk::SemaphoreSubmitInfo signalSemaphoreInfo{
            .semaphore = *_swapchain.renderFinished(_currentImageIndex),
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        };
        vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo,
        };

        _vulkanContext.queue.submit2({ submitInfo }, *_inFlightFences[_currentFrame]);
        _timestampPending[_currentFrame] = _timestampValidBits != 0;

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*_swapchain.renderFinished(_currentImageIndex),
            .swapchainCount = 1,
            .pSwapchains = &*_swapchain.handle(),
            .pImageIndices = &_currentImageIndex,
        };

        const auto presentStart = std::chrono::steady_clock::now();
        try {
            auto result = _vulkanContext.queue.presentKHR(presentInfo);
            if (result == vk::Result::eSuboptimalKHR) {
                _swapchainDirty = true;
            }
        }
        catch (const vk::OutOfDateKHRError&) {
            _swapchainDirty = true;
        }
        _frameWaitMilliseconds += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - presentStart
        ).count();

        _backbufferStates[_currentImageIndex] = TextureState::Present;
        _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        _frameStarted = false;
    }

}
