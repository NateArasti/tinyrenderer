#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_shader.h"
#include "vulkan_material.h"
#include "vulkan_mesh.h"

#include "application.h"
#include "rhi.h"

namespace tr::Rendering::Vulkan {
    class VulkanRenderer : public RHI {
    private:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

        const std::vector<const char*> _requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName
        };
        const std::vector<char const*> _validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
        
        const tr::App::Application& _application;

        vk::raii::Context _context;
        vk::raii::Instance _instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
        vk::raii::SurfaceKHR _surface = nullptr;

	    vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::PhysicalDevice _physicalDevice = nullptr;
        vk::raii::Device _device = nullptr;

        uint32_t _queueIndex = ~0;
        vk::raii::Queue _queue = nullptr;

        std::vector<vk::raii::Buffer> _uniformBuffers;
        std::vector<vk::raii::DeviceMemory> _uniformBuffersMemory;
        std::vector<void *> _uniformBuffersMapped;

        vk::raii::DescriptorPool _descriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet> _descriptorSets;

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

        std::unordered_map<tr::Resources::Handle<tr::Data::Shader>, VulkanShader> _shadersMap;
        std::unordered_map<tr::Resources::Handle<tr::Data::Material>, VulkanMaterial> _materialsMap;
        std::unordered_map<tr::Resources::Handle<tr::Data::Mesh>, VulkanMesh> _meshesMap;

        uint32_t _currentFrame = 0;
        uint32_t _currentImageIndex = 0;
        bool _frameStarted = false;
        bool _swapchainDirty = false;
        bool _initialized = false;

        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapchain();
        void createImageViews();
		void createDescriptorSetLayout();
        void createCommandPool();
        void createCommandBuffers();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();
        void createSyncObjects();
        void cleanupSwapchain();
        void recreateSwapchain();

        void transitionImageLayout(
            uint32_t imageIndex,
            vk::ImageLayout old_layout,
            vk::ImageLayout new_layout,
            vk::AccessFlags2 src_access_mask,
            vk::AccessFlags2 dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask,
            vk::PipelineStageFlags2 dst_stage_mask
        );
        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(
            vk::DeviceSize size,
            vk::BufferUsageFlags usage,
            vk::MemoryPropertyFlags properties
        );
        void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        void recordFrameStartCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
        void recordFrameEndCommands(vk::CommandBuffer commandBuffer, uint32_t imageIndex);

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
