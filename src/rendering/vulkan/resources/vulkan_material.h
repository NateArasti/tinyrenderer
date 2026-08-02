#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "material.h"
#include "vulkan_shader.h"

namespace tr::Rendering::Vulkan {
    struct VulkanMaterial {
        tr::Resources::Handle<tr::Data::Material> handle;
        VulkanShader* shader = nullptr;
        vk::raii::Buffer paramsBuffer = nullptr;
        vk::raii::DeviceMemory paramsMemory = nullptr;
        vk::raii::DescriptorSet descriptorSet = nullptr;
    };
}
