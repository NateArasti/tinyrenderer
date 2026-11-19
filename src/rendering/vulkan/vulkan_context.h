#pragma once

#include <functional>
#include <string_view>
#include <type_traits>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "application.h"

namespace tr::Rendering::Vulkan {
    struct ValidationOptions {
#ifdef NDEBUG
        bool enabled = false;
#else
        bool enabled = true;
#endif
        bool synchronizationValidation = false;
        std::function<void(vk::DebugUtilsMessageSeverityFlagBitsEXT, std::string_view)> onMessage;
    };

    class VulkanContext {
    private:
        ValidationOptions _validation;

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
        void setDebugName(vk::ObjectType type, uint64_t handle, std::string_view name);

        bool _debugUtilsEnabled = false;
        bool _memoryBudgetSupported = false;

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

        explicit VulkanContext(const tr::App::Application& app, ValidationOptions validation = {});

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        bool debugUtilsEnabled() const { return _debugUtilsEnabled; }
        bool memoryBudgetSupported() const { return _memoryBudgetSupported; }
        bool validationEnabled() const { return _validation.enabled && static_cast<bool>(*debugMessenger); }

        template<typename Handle>
        void setDebugName(Handle handle, std::string_view name) {
            if (!_debugUtilsEnabled || name.empty()) {
                return;
            }

            using NativeHandle = typename Handle::CType;
            const auto nativeHandle = static_cast<NativeHandle>(handle);
            const uint64_t value = [&]() {
                if constexpr (std::is_pointer_v<NativeHandle>) {
                    return reinterpret_cast<uint64_t>(nativeHandle);
                }
                else {
                    return static_cast<uint64_t>(nativeHandle);
                }
            }();
            setDebugName(Handle::objectType, value, name);
        }
        
        vk::Format findSupportedFormat(
            const std::vector<vk::Format>& candidates,
            vk::ImageTiling tiling,
            vk::FormatFeatureFlags features
        );
    };
}
