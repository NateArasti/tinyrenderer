#include "vulkan_renderer.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "scene_data.h"
#include "vulkan_utility.h"

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
            _environmentDescriptorSetLayout,
            _swapchain.format()
        );
        _gBuffer = std::make_unique<GBuffer>(
            _vulkanContext,
            _resourceFactory,
            _swapchain
        );
        _skyboxPass = std::make_unique<SkyboxPass>(
            _vulkanContext,
            *_resources,
            *_gBuffer,
            _resourceFactory,
            MAX_FRAMES_IN_FLIGHT,
            _swapchain.format()
        );
        _shadowPass = std::make_unique<ShadowPass>(_vulkanContext, _resourceFactory, *_resources);
        _colorPass = std::make_unique<ColorPass>(_vulkanContext, *_gBuffer, *_resources);
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
        _environmentDescriptorSets.clear();
        _skyboxPass.reset();
        for (auto& buffer : _environmentBuffers) {
            buffer.memory.unmapMemory();
            buffer.buffer.clear();
        }
        _uniformBuffers.clear();
        _environmentBuffers.clear();
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
        _gBuffer->recreate();
        _skyboxPass->recreate(_swapchain.format());
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

#pragma endregion

#pragma region Vulkan initialization
	void VulkanRenderer::createDescriptorSetLayout() {
		const std::array sceneBindings = {
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
        _descriptorSetLayout = vk::raii::DescriptorSetLayout(_vulkanContext.device, {
            .bindingCount = static_cast<uint32_t>(sceneBindings.size()),
            .pBindings = sceneBindings.data()
        });

		const std::array environmentBindings = {
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
        _environmentDescriptorSetLayout = vk::raii::DescriptorSetLayout(_vulkanContext.device, {
            .bindingCount = static_cast<uint32_t>(environmentBindings.size()),
            .pBindings = environmentBindings.data()
        });


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

	void VulkanRenderer::createDescriptorPool() {
		std::array poolSize {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2 * MAX_FRAMES_IN_FLIGHT),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2 * MAX_FRAMES_IN_FLIGHT)
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 2 * MAX_FRAMES_IN_FLIGHT,
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
            _uniformBuffers.push_back(_resourceFactory.createBuffer(
                8 * sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            ));
            _uniformBuffersMapped.push_back(
                _uniformBuffers.back().memory.mapMemory(0, _environmentBuffers.back().size)
            );
            vk::DescriptorBufferInfo bufferInfo{
                .buffer = _uniformBuffers[i].buffer,
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
            vk::DescriptorImageInfo shadowMapInfo = _shadowPass->shadowMap();
            vk::WriteDescriptorSet shadowMapDescriptorWrite{
                .dstSet = _descriptorSets[i],
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &shadowMapInfo
            };
            _vulkanContext.device.updateDescriptorSets({ sceneDataDescriptorWrite, shadowMapDescriptorWrite }, {});
		}

        std::fill(layouts.begin(), layouts.end(), *_environmentDescriptorSetLayout);
        _environmentDescriptorSets = _vulkanContext.device.allocateDescriptorSets(allocInfo);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _environmentBuffers.push_back(_resourceFactory.createBuffer(
                8 * sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            ));
            _environmentBuffersMapped.push_back(
                _environmentBuffers.back().memory.mapMemory(0, _environmentBuffers.back().size)
            );
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
        _gBuffer->recreate();
        if (_swapchain.format() != oldFormat) {
            _skyboxPass->recreate(_swapchain.format());
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

        imageBarrier(
            commandBuffer,
            _swapchain.image(_currentImageIndex),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );

        _lastSceneData = sceneData;
        // Flip Y axis for Vulkan clip space
        _lastSceneData.proj[1][1] *= -1;
        memcpy(_uniformBuffersMapped[_currentFrame], &_lastSceneData, sizeof(_lastSceneData));

        _frameStarted = true;
    }

    void VulkanRenderer::updateEnvironment(const Data::Environment& environment) {
        const auto* cubemap = _resources->cubemap(environment.skyboxHandle);
        const std::array<float, 12> data{
            environment.clearColor.x, environment.clearColor.y, environment.clearColor.z,
            environment.skyboxRotation,
            environment.skyboxTint.x, environment.skyboxTint.y, environment.skyboxTint.z,
            environment.skyboxExposure,
            0.0f, 0.0f, 0.0f,
            cubemap != nullptr ? 1.0f : 0.0f,
        };
        if (cubemap == nullptr) {
            cubemap = _resources->fallbackCubemap();
        }
        memcpy(_environmentBuffersMapped[_currentFrame], data.data(), sizeof(data));
        const vk::DescriptorBufferInfo environmentInfo {
            .buffer = *_environmentBuffers[_currentFrame].buffer,
            .offset = 0,
            .range = sizeof(data)
        };
        vk::DescriptorImageInfo irradianceInfo{
            .sampler = *cubemap->sampler,
            .imageView = *cubemap->irradiance.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        const auto environmentSet = *_environmentDescriptorSets[_currentFrame];
        _vulkanContext.device.updateDescriptorSets(
            {
                {
                    .dstSet = environmentSet,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &environmentInfo
                },
                {
                    .dstSet = environmentSet,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &irradianceInfo
                }
            },
            {}
        );
    }

    void VulkanRenderer::renderSkybox(const tr::Data::Environment& environment) {
        if (!_frameStarted) return;

        updateEnvironment(environment);
        _skyboxPass->record(
            _commandBuffers[_currentFrame],
            FrameContext{
                .sceneDescriptorSet = *_descriptorSets[_currentFrame],
                .environmentDescriptorSet = *_environmentDescriptorSets[_currentFrame],
                .targetImage = _swapchain.image(_currentImageIndex),
                .targetImageView = *_swapchain.imageView(_currentImageIndex),
                .extent = _swapchain.extent()
            },
            environment,
            _currentFrame,
            vk::DescriptorBufferInfo{
                .buffer = *_uniformBuffers[_currentFrame].buffer, .offset = 0, .range = sizeof(SceneData)
            },
            vk::DescriptorBufferInfo{
                .buffer = *_environmentBuffers[_currentFrame].buffer,
                .offset = 0,
                .range = _environmentBuffers[_currentFrame].size
            }
        );
    }

    void VulkanRenderer::renderShadowPass(std::span<const DrawCommand> commands) {
        if (!_frameStarted) return;
        _shadowPass->record(
            _commandBuffers[_currentFrame],
            _lastSceneData,
            commands
        );
    }

    void VulkanRenderer::renderColorPass(std::span<const DrawCommand> commands) {
        if (!_frameStarted) return;
        _colorPass->record(
            _commandBuffers[_currentFrame],
            FrameContext{
                .sceneDescriptorSet = *_descriptorSets[_currentFrame],
                .environmentDescriptorSet = *_environmentDescriptorSets[_currentFrame],
                .targetImage = _swapchain.image(_currentImageIndex),
                .targetImageView = *_swapchain.imageView(_currentImageIndex),
                .extent = _swapchain.extent()
            },
            commands
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

        imageBarrier(
            _commandBuffers[_currentFrame],
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
