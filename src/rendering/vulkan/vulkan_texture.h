#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "texture.h"

namespace tr::Rendering::Vulkan {
    struct VulkanTexture {
        tr::Resources::Handle<tr::Data::Texture> source;
        vk::raii::Image textureImage = nullptr;
        vk::raii::DeviceMemory textureImageMemory = nullptr;
        vk::raii::ImageView textureImageView = nullptr;
        vk::raii::Sampler textureSampler = nullptr;
    };
}
