#pragma once

#include <array>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "application.h"
#include "rhi.h"

namespace tr::Rendering::Vulkan {
    class VulkanRenderer : public RHI {
    private:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        
        const tr::App::Application& _application;

        vk::raii::Context _context;
        vk::raii::Instance _instance = nullptr;
        vk::raii::SurfaceKHR _surface = nullptr;

        vk::raii::PhysicalDevice _physicalDevice = nullptr;
        vk::raii::Device _device = nullptr;

        uint32_t _queueIndex = ~0;
        vk::raii::Queue _queue = nullptr;

        vk::raii::SwapchainKHR _swapchain = nullptr;
        vk::Format _swapchainImageFormat = vk::Format::eUndefined;
        vk::Extent2D _swapchainExtent = {};
        std::vector<vk::Image> _swapchainImages;
        std::vector<vk::raii::ImageView> _swapchainImageViews;

        vk::raii::CommandPool _commandPool = nullptr;
        std::vector<vk::raii::CommandBuffer> _commandBuffers;

        std::array<vk::raii::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores = {
            nullptr,
            nullptr
        };
        std::array<vk::raii::Semaphore, MAX_FRAMES_IN_FLIGHT> _renderFinishedSemaphores = {
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
        bool _initialized = false;

        void createInstance();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapchain();
        void createImageViews();
        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void cleanupSwapchain();
        void recreateSwapchain();

        void recordClearCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex);

    public:
        explicit VulkanRenderer(const tr::App::Application& window);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        void init() override;
        void shutdown() override;
        void clearResources() override;

        void resize(uint32_t width, uint32_t height) override;

        void createShader(
            tr::Resources::Handle<tr::Data::Shader> handle,
            const tr::Data::Shader& shader) override;
        void createTexture(
            tr::Resources::Handle<tr::Data::Texture> handle,
            const tr::Data::Texture& texture) override;
        void createMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material) override;
        void createMesh(
            tr::Resources::Handle<tr::Data::Mesh> handle,
            const tr::Data::Mesh& mesh) override;

        void startFrame(const tr::Data::Camera& camera) override;
        void draw(const DrawCommand& command) override;
        void endFrame() override;
    };
}
