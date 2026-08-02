#include "vulkan_renderer.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "scene_data.h"

namespace tr::Rendering::Vulkan {
    namespace {
        vk::SampleCountFlagBits toSampleCount(uint32_t samples) {
            switch (samples) {
            case 1: return vk::SampleCountFlagBits::e1;
            case 2: return vk::SampleCountFlagBits::e2;
            case 4: return vk::SampleCountFlagBits::e4;
            case 8: return vk::SampleCountFlagBits::e8;
            default: throw std::invalid_argument("Unsupported MSAA sample count");
            }
        }

        uint32_t fromSampleCount(vk::SampleCountFlagBits samples) {
            return static_cast<uint32_t>(samples);
        }
    }

    VulkanRenderer::VulkanRenderer(const tr::App::Application& application) :
        _application(application),
        _vulkanContext(application),
        _resourceFactory(_vulkanContext),
        _swapchain(_vulkanContext, application.window->width(), application.window->height())
    {
        createDescriptorSetLayout();
        _resources = std::make_unique<VulkanResources>(
            _vulkanContext,
            _resourceFactory,
            _descriptorSetLayout,
            _swapchain.format()
        );
        _shadowPass = std::make_unique<ShadowPass>(_vulkanContext, _resourceFactory, *_resources);
        _colorPass = std::make_unique<ColorPass>(_vulkanContext, _resourceFactory, _swapchain, *_resources);
        _uiPass = std::make_unique<UIPass>(_vulkanContext, _swapchain);
        createCommandPool();
        createCommandBuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
        createSyncObjects();
    }

    VulkanRenderer::~VulkanRenderer() {
        _vulkanContext.device.waitIdle();
        _commandBuffers.clear();
        _descriptorSets.clear();
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
        _uniformBuffers.clear();
    }

    void VulkanRenderer::resize(uint32_t, uint32_t) {
        _swapchainDirty = true;
    }

    uint32_t VulkanRenderer::getMsaaSamples() const {
        return fromSampleCount(_vulkanContext.msaaSamples);
    }

    std::vector<uint32_t> VulkanRenderer::getSupportedMsaaSamples() const {
        const auto limits = _vulkanContext.physicalDevice.getProperties().limits;
        const auto supported =
            limits.framebufferColorSampleCounts
            & limits.framebufferDepthSampleCounts;

        std::vector<uint32_t> result;
        for (const auto samples : {
            vk::SampleCountFlagBits::e1,
            vk::SampleCountFlagBits::e2,
            vk::SampleCountFlagBits::e4,
            vk::SampleCountFlagBits::e8
        }) {
            if (supported & samples) {
                result.push_back(fromSampleCount(samples));
            }
        }
        return result;
    }

    void VulkanRenderer::setMsaaSamples(uint32_t samples) {
        const auto sampleCount = toSampleCount(samples);
        const auto supported = getSupportedMsaaSamples();
        if (std::ranges::find(supported, samples) == supported.end()) {
            throw std::invalid_argument("MSAA sample count is not supported by this device");
        }
        if (_vulkanContext.msaaSamples == sampleCount) {
            return;
        }

        _vulkanContext.device.waitIdle();
        _vulkanContext.msaaSamples = sampleCount;
        _colorPass->recreate();
        _resources->recreateBaseShaders(_swapchain.format());
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


#pragma endregion

#pragma region Vulkan initialization

    void VulkanRenderer::createShadowShader(const tr::Data::Shader& shader) {
        _shadowPass->createPipeline(shader, getBindingDescription(), getAttributeDescriptions());
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
        _descriptorSetLayout = vk::raii::DescriptorSetLayout(_vulkanContext.device, layoutInfo);
    }

    void VulkanRenderer::createCommandPool() {
        vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = _vulkanContext.queueIndex
        };
        _commandPool = vk::raii::CommandPool(_vulkanContext.device, createInfo);
    }

    void VulkanRenderer::createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = *_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };
        _commandBuffers = vk::raii::CommandBuffers(_vulkanContext.device, allocateInfo);
    }

    void VulkanRenderer::createUniformBuffers() {
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
        _uniformBuffersMapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DeviceSize bufferSize = sizeof(SceneData);
			GPUBuffer buffer = _resourceFactory.createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );
            _uniformBuffers.emplace_back(std::move(buffer.buffer));
			_uniformBuffersMemory.emplace_back(std::move(buffer.memory));
			_uniformBuffersMapped.emplace_back(_uniformBuffersMemory.back().mapMemory(0, buffer.size));
		}
    }

	void VulkanRenderer::createDescriptorPool() {
		std::array poolSize {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()
        };
        _descriptorPool = vk::raii::DescriptorPool(_vulkanContext.device, poolInfo);
    }

    void VulkanRenderer::createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *_descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

		_descriptorSets = _vulkanContext.device.allocateDescriptorSets(allocInfo);

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
            vk::DescriptorImageInfo shadowMapInfo = _shadowPass->descriptorInfo();
            vk::WriteDescriptorSet shadowMapDescriptorWrite{
                .dstSet = _descriptorSets[i],
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &shadowMapInfo
            };
            _vulkanContext.device.updateDescriptorSets({ sceneDataDescriptorWrite, shadowMapDescriptorWrite }, {});
		}
    }

    void VulkanRenderer::createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _imageAvailableSemaphores[i] = vk::raii::Semaphore(_vulkanContext.device, semaphoreInfo);
            _inFlightFences[i] = vk::raii::Fence(_vulkanContext.device, fenceInfo);
        }
    }

    void VulkanRenderer::recreateSwapchain() {
        if (_application.window->width() == 0 || _application.window->height() == 0) {
            _swapchainDirty = true;
            return;
        }

        _vulkanContext.device.waitIdle();
        const vk::Format oldFormat = _swapchain.format();
        _swapchain.recreate(
            _application.window->width(),
            _application.window->height()
        );
        _colorPass->recreate();
        if (_swapchain.format() != oldFormat) {
            _resources->recreateBaseShaders(_swapchain.format());
        }
        _uiPass->recreate(_swapchain);
        _swapchainDirty = false;
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
            _vulkanContext.device.waitForFences(
                { *fence },
                vk::True,
                std::numeric_limits<uint64_t>::max()
            ) != vk::Result::eSuccess
        ) {
        }

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
            return;
        }

        _vulkanContext.device.resetFences({ *fence });

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


    void VulkanRenderer::renderShadowPass(std::span<const DrawCommand> commands) {
        if (!_frameStarted) return;
        _shadowPass->render(
            _commandBuffers[_currentFrame],
            commands,
            RenderPass::Context{ .sceneData = &_lastSceneData }
        );
    }

    void VulkanRenderer::renderColorPass(std::span<const DrawCommand> commands) {
        if (!_frameStarted) return;
        _colorPass->render(
            _commandBuffers[_currentFrame],
            commands,
            RenderPass::Context{
                .sceneData = &_lastSceneData,
                .sceneDescriptorSet = *_descriptorSets[_currentFrame],
                .targetImage = _swapchain.image(_currentImageIndex),
                .targetImageView = *_swapchain.imageView(_currentImageIndex),
                .extent = _swapchain.extent()
            }
        );
    }

    void VulkanRenderer::prepareUI() {
        if (_swapchainDirty) {
            recreateSwapchain();
            if (_swapchainDirty) {
                return;
            }
        }
        _uiPass->beginFrame();
    }

    void VulkanRenderer::drawUI(ImDrawData* drawData) {
        if (!_frameStarted || drawData == nullptr) {
            return;
        }

        _uiPass->draw(
            _commandBuffers[_currentFrame],
            *_swapchain.imageView(_currentImageIndex),
            _swapchain.extent(),
            drawData
        );
    }
    
    void VulkanRenderer::endFrame() {
        if (!_frameStarted) {
            return;
        }

        auto& commandBuffer = _commandBuffers[_currentFrame];

        transitionImageLayout(
            _swapchain.image(_currentImageIndex),
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite, {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::ImageAspectFlagBits::eColor
        );
        commandBuffer.end();

        vk::CommandBufferSubmitInfo commandBufferInfo{
            .commandBuffer = commandBuffer
        };
        vk::SemaphoreSubmitInfo waitSemaphoreInfo{
            .semaphore = *_imageAvailableSemaphores[_currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };
        vk::SemaphoreSubmitInfo signalSemaphoreInfo{
            .semaphore = *_swapchain.renderFinished(_currentImageIndex),
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

        _vulkanContext.queue.submit2({ submitInfo }, *_inFlightFences[_currentFrame]);

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*_swapchain.renderFinished(_currentImageIndex),
            .swapchainCount = 1,
            .pSwapchains = &*_swapchain.handle(),
            .pImageIndices = &_currentImageIndex
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

        _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        _frameStarted = false;
    }

#pragma endregion
}
