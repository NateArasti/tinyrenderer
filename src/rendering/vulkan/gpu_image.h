#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace tr::Rendering::Vulkan {
    struct GPUImage {
        vk::raii::Image image = nullptr;
        vk::raii::DeviceMemory memory = nullptr;
        vk::raii::ImageView view = nullptr;
        vk::Format format = vk::Format::eUndefined;
        vk::Extent2D extent{};
        uint32_t mipLevels = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    };
}
