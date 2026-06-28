#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "material.h"

namespace tr::Rendering::Vulkan {
    struct VulkanMaterial {
        tr::Resources::Handle<tr::Data::Material> source;
        tr::Resources::Handle<tr::Data::Shader> shader;
        vk::raii::Buffer paramsBuffer = nullptr;
        vk::raii::DeviceMemory paramsMemory = nullptr;
        vk::raii::DescriptorSet descriptorSet = nullptr;
    };
}
