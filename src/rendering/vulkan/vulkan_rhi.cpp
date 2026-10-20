#include "vulkan_rhi.h"

#include "vulkan_utility.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    VulkanRHI::VulkanRHI(const tr::App::Application& application) :
        _application(application),
        _vulkanContext(application),
        _allocator(_vulkanContext),
        _swapchain(_vulkanContext, application.window->width(), application.window->height())
    {
        _resources = std::make_unique<VulkanResources>(_vulkanContext, _allocator);
        _commandAdapter = std::make_unique<VulkanCommandBuffer>(_vulkanContext, *_resources);
        _uiRenderer = std::make_unique<VulkanUIRenderer>(_vulkanContext, _swapchain);
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
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
        _swapchain.recreate(_application.window->width(), _application.window->height());
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

    std::optional<FrameContext> VulkanRHI::startFrame() {
        if (_frameStarted || _application.window->width() == 0 || _application.window->height() == 0) {
            return std::nullopt;
        }

        if (_swapchainDirty) {
            recreateSwapchain();
            return std::nullopt;
        }

        auto& fence = _inFlightFences[_currentFrame];

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
            _swapchainDirty = true;
            return std::nullopt;
        }

        _resources->beginFrame(_currentFrame);

        auto& commandBuffer = _commandBuffers[_currentFrame];
        commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);

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

        commandBuffer.end();
        _vulkanContext.device.resetFences({ *_inFlightFences[_currentFrame] });

        vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = commandBuffer };
        vk::SemaphoreSubmitInfo waitSemaphoreInfo{
            .semaphore = *_imageAvailableSemaphores[_currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };
        vk::SemaphoreSubmitInfo signalSemaphoreInfo{
            .semaphore = *_swapchain.renderFinished(_currentImageIndex),
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
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

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*_swapchain.renderFinished(_currentImageIndex),
            .swapchainCount = 1,
            .pSwapchains = &*_swapchain.handle(),
            .pImageIndices = &_currentImageIndex,
        };

        try {
            auto result = _vulkanContext.queue.presentKHR(presentInfo);
            if (result == vk::Result::eSuboptimalKHR) {
                _swapchainDirty = true;
            }
        }
        catch (const vk::OutOfDateKHRError&) {
            _swapchainDirty = true;
        }

        _backbufferStates[_currentImageIndex] = TextureState::Present;
        _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        _frameStarted = false;
    }

}
