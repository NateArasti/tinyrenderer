#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "application.h"

namespace tr::Rendering::Vulkan {
    class VulkanContext {
    private:
        const std::vector<const char*> _requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName
        };
        const std::vector<char const*> _validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        void createInstance(const tr::App::Application& application);
        void setupDebugMessenger();
        void createSurface(const tr::App::Application& application);
        void pickPhysicalDevice();
        void createLogicalDevice();
        vk::SampleCountFlagBits getMaxSampleCount();

    public:
        vk::raii::Context context;
        vk::raii::Instance instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
        vk::raii::SurfaceKHR surface = nullptr;

        vk::raii::PhysicalDevice physicalDevice = nullptr;
        vk::raii::Device device = nullptr;

        uint32_t queueIndex = ~0;
        vk::raii::Queue queue = nullptr;

        vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;
        vk::Format depthFormat;

        explicit VulkanContext(const tr::App::Application& app);

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        void setDebugName(vk::ObjectType type, uint64_t handle, const char* name);
        
        vk::Format findSupportedFormat(
            const std::vector<vk::Format>& candidates,
            vk::ImageTiling tiling,
            vk::FormatFeatureFlags features
        );
    };
}
