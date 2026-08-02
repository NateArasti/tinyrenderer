#include "vulkan_context.h"

#include <iostream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "application.h"

namespace tr::Rendering::Vulkan {
#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif
        
    namespace {
        std::vector<const char*> getRequiredInstanceExtensions() {
            uint32_t glfwExtensionCount = 0;
            auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
            
            if (enableValidationLayers)
            {
                extensions.push_back(vk::EXTDebugUtilsExtensionName);
            }

            return extensions;
        }

        bool isDeviceSuitable(
            const vk::raii::PhysicalDevice& physicalDevice,
            const vk::raii::SurfaceKHR& surface,
            const std::vector<const char*>& requiredDeviceExtension
        ) {
            bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

            if (!supportsVulkan1_3) {
                return false;
            }

            auto queueFamilies = physicalDevice.getQueueFamilyProperties();
            bool supportsGraphicsAndPresent = false;
            for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    physicalDevice.getSurfaceSupportKHR(i, *surface)) {
                    supportsGraphicsAndPresent = true;
                    break;
                }
            }

            if (!supportsGraphicsAndPresent) {
                return false;
            }

            bool supportsAllRequiredExtensions = true;
            auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
            for (const auto& requiredDeviceExtension : requiredDeviceExtension) {
                bool isSupported = false;
                for (const auto& availableDeviceExtension : availableDeviceExtensions) {
                    if (strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0) {
                        isSupported = true;
                        break;
                    }
                }
                if (!isSupported) {
                    supportsAllRequiredExtensions = false;
                    break;
                }
            }

            if (!supportsAllRequiredExtensions) {
                return false;
            }

            auto features = physicalDevice.template getFeatures2
                <
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
                >();
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            if (!supportsRequiredFeatures) {
                return false;
            }

            if (physicalDevice.getSurfaceFormatsKHR(*surface).empty() ||
                physicalDevice.getSurfacePresentModesKHR(*surface).empty()) {
                return false;
            }

            return true;
        }
    }

    VulkanContext::VulkanContext(const tr::App::Application& app) {
        createInstance(app);
        createSurface(app);
        pickPhysicalDevice();
        createLogicalDevice();
        msaaSamples = getMaxSampleCount();
        depthFormat = findSupportedFormat(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    vk::Format VulkanContext::findSupportedFormat(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features
    ) {
        for (const auto format : candidates) {
            vk::FormatProperties props = physicalDevice.getFormatProperties(format);

            if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
                ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    void VulkanContext::setDebugName(vk::ObjectType type, uint64_t handle, const char* name) {
        if (!enableValidationLayers) return;

        vk::DebugUtilsObjectNameInfoEXT nameInfo{
            .objectType = type,
            .objectHandle = handle,
            .pObjectName = name
        };
        device.setDebugUtilsObjectNameEXT(nameInfo);
    }

    void VulkanContext::createInstance(const tr::App::Application& application) {
        vk::ApplicationInfo appInfo{
            .pApplicationName = application.name.data(),
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: parse application version?
            .pEngineName = application.name.data(),
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(_validationLayers.begin(), _validationLayers.end());
        }
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(
            requiredLayers,
            [&layerProperties](auto const& requiredLayer) {
                return std::ranges::none_of(
                    layerProperties,
                    [requiredLayer](auto const& layerProperty) {
                        return strcmp(layerProperty.layerName, requiredLayer) == 0;
                    }
                );
            }
        );
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        auto requiredExtensions = getRequiredInstanceExtensions();
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(
            requiredExtensions,
            [&extensionProperties](auto const& requiredExtension) {
                return std::ranges::none_of(
                    extensionProperties,
                    [requiredExtension](auto const& extensionProperty) {
                        return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                    }
                );
            }
        );
        if (unsupportedPropertyIt != requiredExtensions.end())
        {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };
        instance = vk::raii::Instance(context, createInfo);
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
        return vk::False;
    }

    void VulkanContext::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlags,
            .pfnUserCallback = &debugCallback
        };
        
        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void VulkanContext::createSurface(const tr::App::Application& application) {
        VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

        auto result = glfwCreateWindowSurface(
            static_cast<VkInstance>(*instance),
            static_cast<GLFWwindow*>(application.window->nativeHandle()),
            nullptr,
            &rawSurface
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("glfwCreateWindowSurface failed");
        }

        surface = vk::raii::SurfaceKHR(instance, rawSurface);
    }

    void VulkanContext::pickPhysicalDevice() {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

        for (const auto& checkPhysicalDevice : physicalDevices) {
            if (checkPhysicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
                isDeviceSuitable(checkPhysicalDevice, surface, _requiredDeviceExtension)
            ) {
                physicalDevice = checkPhysicalDevice;
                break;
            }
        }

        if (physicalDevice == nullptr)
        {
            for (const auto& checkPhysicalDevice : physicalDevices) {
                if (isDeviceSuitable(checkPhysicalDevice, surface, _requiredDeviceExtension)) {
                    physicalDevice = checkPhysicalDevice;
                    break;
                }
            }
        }

        if (physicalDevice == nullptr)
        {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
    }

    void VulkanContext::createLogicalDevice() {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        > featureChain = {
                {
                    .features = {
                        .samplerAnisotropy = true
                    }
                },
                {
                    .synchronization2 = true,
                    .dynamicRendering = true
                },
                {
                    .extendedDynamicState = true
                }
        };

        // create a Device with single queue
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = queueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(_requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = _requiredDeviceExtension.data()
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue = vk::raii::Queue(device, queueIndex, 0);
    }

    vk::SampleCountFlagBits VulkanContext::getMaxSampleCount() {
        auto limits = physicalDevice.getProperties().limits;
        auto counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        for (auto count : { vk::SampleCountFlagBits::e8, vk::SampleCountFlagBits::e4, vk::SampleCountFlagBits::e2 }) {
            if (counts & count) return count;
        }
        return vk::SampleCountFlagBits::e1;
    }
}
