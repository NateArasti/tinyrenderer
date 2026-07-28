#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_shader.h"
#include "vulkan_texture.h"
#include "vulkan_material.h"
#include "vulkan_mesh.h"

#include "application.h"
#include "rhi.h"

namespace tr::Rendering::Vulkan {
    class VulkanRenderer : public RHI {
    private:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        static constexpr uint32_t MAX_MATERIALS = 256;
        static constexpr uint32_t MAX_TEXTURES_PER_MATERIAL = 8;
        static constexpr uint32_t SHADOW_MAP_SIZE = 4096;

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

        vk::raii::PhysicalDevice _physicalDevice = nullptr;
        vk::raii::Device _device = nullptr;
        
        vk::raii::DescriptorPool _descriptorPool = nullptr;
        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        std::vector<vk::raii::DescriptorSet> _descriptorSets;

        uint32_t _queueIndex = ~0;
        vk::raii::Queue _queue = nullptr;

        std::vector<vk::raii::Buffer> _uniformBuffers;
        std::vector<vk::raii::DeviceMemory> _uniformBuffersMemory;
        std::vector<void*> _uniformBuffersMapped;

        vk::raii::SwapchainKHR _swapchain = nullptr;
        vk::Format _swapchainImageFormat = vk::Format::eUndefined;
        vk::Extent2D _swapchainExtent = {};
        std::vector<vk::Image> _swapchainImages;
        std::vector<vk::raii::ImageView> _swapchainImageViews;

        vk::raii::CommandPool _commandPool = nullptr;
        std::vector<vk::raii::CommandBuffer> _commandBuffers;

        vk::SampleCountFlagBits _msaaSamples = vk::SampleCountFlagBits::e1;

        vk::raii::Image _colorImage = nullptr;
        vk::raii::DeviceMemory _colorImageMemory = nullptr;
        vk::raii::ImageView _colorImageView = nullptr;

        vk::Format _depthFormat;
        vk::raii::Image _depthImage = nullptr;
        vk::raii::DeviceMemory _depthImageMemory = nullptr;
        vk::raii::ImageView _depthImageView = nullptr;

        vk::Format _shadowFormat = vk::Format::eD32Sfloat;
        vk::raii::PipelineLayout _shadowPipelineLayout = nullptr;
        vk::raii::Pipeline _shadowPipeline = nullptr;
        vk::raii::Image _shadowImage = nullptr;
        vk::raii::DeviceMemory _shadowImageMemory = nullptr;
        vk::raii::ImageView _shadowImageView = nullptr;
        vk::raii::Sampler _shadowSampler = nullptr;

        vk::raii::Image _fallbackImage = nullptr;
        vk::raii::DeviceMemory _fallbackImageMemory = nullptr;
        vk::raii::ImageView _fallbackImageView = nullptr;
        vk::raii::Sampler _fallbackSampler = nullptr;

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
        std::unordered_map<tr::Resources::Handle<tr::Data::Texture>, VulkanTexture> _texturesMap;
        std::unordered_map<tr::Resources::Handle<tr::Data::Material>, VulkanMaterial> _materialsMap;
        std::unordered_map<tr::Resources::Handle<tr::Data::Mesh>, VulkanMesh> _meshesMap;

        uint32_t _currentFrame = 0;
        uint32_t _currentImageIndex = 0;
        bool _frameStarted = false;
        bool _swapchainDirty = false;
        bool _initialized = false;

        SceneData _lastSceneData;

        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapchain();
        void createImageViews();
		void createDescriptorSetLayout();
        void createCommandPool();
        void createFallbackTexture();
        void createColorResources();
        void createDepthResources();
        void createShadowResources();
        void createCommandBuffers();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();
        void createSyncObjects();
        void cleanupSwapchain();
        void recreateSwapchain();
        void createUIObjects();

        void setDebugName(vk::ObjectType type, uint64_t handle, const char* name);

        std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();
        void endSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer) const;
        std::tuple<vk::raii::Image, vk::raii::DeviceMemory> createImage(
            uint32_t width, uint32_t height,
            vk::Format format,
            uint32_t mipLevels,
            vk::SampleCountFlagBits samples,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties
        );
        vk::raii::ImageView createImageView(
            vk::Image const& image, 
            vk::Format format, 
            vk::ImageAspectFlags aspectFlags,
            uint32_t mipLevels
        );
        void transitionImageLayout(
            vk::Image image,
            vk::ImageLayout old_layout, vk::ImageLayout new_layout,
            vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask,
            vk::ImageAspectFlags image_aspect_flags
        );
        void transitionImageLayout(
            const vk::raii::Image& image,
            const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout,
            uint32_t mipLevels
        );
        void copyBufferToImage(
            const vk::raii::Buffer& buffer,
            const vk::raii::Image& image,
            uint32_t width, uint32_t height
        );
        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(
            vk::DeviceSize size,
            vk::BufferUsageFlags usage,
            vk::MemoryPropertyFlags properties
        );
        void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
        vk::Format findSupportedFormat(
                const std::vector<vk::Format>& candidates,
            vk::ImageTiling tiling,
            vk::FormatFeatureFlags features
        );
        vk::SampleCountFlagBits getMaxSampleCount();
        void generateMipmaps(
            vk::raii::Image& image,
            vk::Format imageFormat,
            int32_t texWidth, int32_t texHeight,
            uint32_t mipLevels
        );

    public:
        explicit VulkanRenderer(const tr::App::Application& application);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        std::string getDeviceName() const override {
            const auto properties = _physicalDevice.getProperties();
            return properties.deviceName.data();
        }
        void clearResources() override;
        void resize(uint32_t width, uint32_t height) override;

        void createShadowShader(const tr::Data::Shader& shader) override;

        void createShader(
            tr::Resources::Handle<tr::Data::Shader> handle,
            const tr::Data::Shader& shader) override;
        void createTexture(
            tr::Resources::Handle<tr::Data::Texture> handle,
            const tr::Data::Texture& texture) override;
        void createMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material,
            const tr::Data::Shader& shader) override;
        void createMesh(
            tr::Resources::Handle<tr::Data::Mesh> handle,
            const tr::Data::Mesh& mesh) override;
        
        void startFrame(const tr::Rendering::SceneData& sceneData) override;
        void startShadowPass() override;
        void drawShadows(const DrawCommand& command) override;
        void endShadowPass() override;
        void startColorPass() override;
        void draw(const DrawCommand& command) override;
        void endColorPass() override;
        void prepareUI() override;
        void drawUI(ImDrawData* drawData) override;
        void endFrame() override;
    };
}
