#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "texture.h"

namespace tr::Rendering::Vulkan {
    struct VulkanTexture {
        tr::Resources::Handle<tr::Data::Texture> handle;
        GPUImage image;
        vk::raii::Sampler sampler = nullptr;
    };
}
