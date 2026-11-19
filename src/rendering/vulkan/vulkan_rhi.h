#pragma once

#include <array>
#include <memory>

#include "vulkan_context.h"
#include "swapchain.h"
#include "resources/vulkan_allocator.h"
#include "resources/vulkan_resources.h"
#include "vulkan_command_buffer.h"
#include "vulkan_ui_renderer.h"
#include "rhi.h"
#include "defines.h"

namespace tr::Rendering::Vulkan {
    class VulkanRHI : public RHI {
    private:
        const tr::App::Application& _application;
        VulkanContext _vulkanContext;
        VulkanAllocator _allocator;
        Swapchain _swapchain;

        std::unique_ptr<VulkanResources> _resources;
        std::unique_ptr<VulkanCommandBuffer> _commandAdapter;
        std::unique_ptr<VulkanUIRenderer> _uiRenderer;

        vk::raii::CommandPool _commandPool = nullptr;
        std::vector<vk::raii::CommandBuffer> _commandBuffers;
        vk::raii::QueryPool _timestampQueries = nullptr;
        std::array<bool, MAX_FRAMES_IN_FLIGHT> _timestampPending{};
        uint32_t _timestampValidBits = 0;
        double _timestampPeriod = 0.0;
        double _frameWaitMilliseconds = 0.0;
        std::optional<double> _gpuFrameMilliseconds;
        
        std::vector<Handle<GPUTexture>> _backbuffers;
        std::vector<Handle<GPUTexture>> _hdrTargets;
        std::vector<Handle<GPUTexture>> _msaaTargets;
        std::vector<Handle<GPUTexture>> _depthTargets;
        std::vector<TextureState> _backbufferStates;

        std::array<vk::raii::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores{
            nullptr,
            nullptr,
        };
        std::array<vk::raii::Fence, MAX_FRAMES_IN_FLIGHT> _inFlightFences{
            nullptr,
            nullptr,
        };

        uint32_t _currentFrame = 0;
        uint32_t _currentImageIndex = 0;
        bool _frameStarted = false;
        bool _swapchainDirty = false;
        bool _vsyncEnabled = true;

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void createTimestampQueries();
        void recreateSwapchain();
        void importBackbuffers();
        void createFrameTargets();
        void destroyFrameTargets();

    public:
        explicit VulkanRHI(const tr::App::Application& application, ValidationOptions validation = {});
        ~VulkanRHI() override;

        std::string getDeviceName() const override;
        std::optional<App::MemoryUsage> getMemoryUsage() const override;
        bool isDeviceMemoryShared() const override;
        double getFrameWaitMilliseconds() const override { return _frameWaitMilliseconds; }
        std::optional<double> getGpuFrameMilliseconds() const override { return _gpuFrameMilliseconds; }
        bool validationEnabled() const { return _vulkanContext.validationEnabled(); }
        void resize(uint32_t width, uint32_t height) override;
        uint32_t getMsaaSamples() const override;
        std::vector<uint32_t> getSupportedMsaaSamples() const override;
        void setMsaaSamples(uint32_t samples) override;
        bool getVsyncEnabled() const override { return _vsyncEnabled; }
        bool supportsVsyncToggle() const override { return _swapchain.supportsVsyncToggle(); }
        void setVsyncEnabled(bool enabled) override;
        float getProjectionYSign() const override { return -1.0f; }
        Format getDepthFormat() const override;
        void waitIdle() override;

        RenderingResources& resources() override { return *_resources; }
        UIRenderer& uiRenderer() override;

        std::optional<FrameContext> startFrame() override;
        void endFrame() override;
    };
}
