#pragma once

#include <array>
#include <vector>
#include <memory>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"
#include "swapchain.h"
#include "gbuffer.h"

#include "utility/gpu_buffer.h"
#include "utility/gpu_image.h"

#include "resources/resource_factory.h"
#include "resources/vulkan_resources.h"

#include "passes/skybox_pass.h"
#include "passes/shadow_pass.h"
#include "passes/color_pass.h"
#include "passes/ui_pass.h"

#include "application.h"
#include "rhi.h"

namespace tr::Rendering::Vulkan {
    class VulkanRenderer : public RHI {
    private:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        
        const tr::App::Application& _application;

        VulkanContext _vulkanContext;
        ResourceFactory _resourceFactory;
        Swapchain _swapchain;

        std::unique_ptr<VulkanResources> _resources;
        std::unique_ptr<GBuffer> _gBuffer;
        std::unique_ptr<SkyboxPass> _skyboxPass;
        std::unique_ptr<ShadowPass> _shadowPass;
        std::unique_ptr<ColorPass> _colorPass;
        std::unique_ptr<UIPass> _uiPass;

        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        std::vector<vk::raii::DescriptorSet> _descriptorSets;

        std::vector<vk::raii::Buffer> _uniformBuffers;
        std::vector<vk::raii::DeviceMemory> _uniformBuffersMemory;
        std::vector<void*> _uniformBuffersMapped;

        vk::raii::CommandPool _commandPool = nullptr;
        std::vector<vk::raii::CommandBuffer> _commandBuffers;

        std::array<vk::raii::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores = {
            nullptr,
            nullptr
        };
        std::array<vk::raii::Fence, MAX_FRAMES_IN_FLIGHT> _inFlightFences = {
            nullptr,
            nullptr
        };

        uint32_t _currentFrame = 0;
        uint32_t _currentImageIndex = 0;
        bool _frameStarted = false;
        bool _swapchainDirty = false;

        SceneData _lastSceneData;

		void createDescriptorSetLayout();
        void createCommandPool();
        void createCommandBuffers();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();
        void createSyncObjects();
        void recreateSwapchain();

    public:
        explicit VulkanRenderer(const tr::App::Application& application);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        std::string getDeviceName() const override {
            const auto properties = _vulkanContext.physicalDevice.getProperties();
            return properties.deviceName.data();
        }
        void resize(uint32_t width, uint32_t height) override;
        uint32_t getMsaaSamples() const override;
        std::vector<uint32_t> getSupportedMsaaSamples() const override;
        void setMsaaSamples(uint32_t samples) override;

        RenderingResources& resources() override { return *_resources; }
        
        void startFrame(const tr::Rendering::SceneData& sceneData) override;
        void renderSkybox(const tr::Data::Environment& environment) override;
        void renderShadowPass(std::span<const tr::Rendering::DrawCommand> commands) override;
        void renderColorPass(std::span<const tr::Rendering::DrawCommand> commands) override;
        void prepareUI() override;
        void drawUI(ImDrawData* drawData) override;
        void endFrame() override;
    };
}
