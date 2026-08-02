#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace tr::Rendering::Vulkan {
    struct GPUBuffer {
        vk::raii::Buffer buffer;
        vk::raii::DeviceMemory memory;
        vk::DeviceSize size;
    };
}
