#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "cubemap.h"
#include "../utility/gpu_image.h"

namespace tr::Rendering::Vulkan {
    struct VulkanCubemap {
        tr::Resources::Handle<tr::Data::Cubemap> handle;
        GPUImage image;
        vk::raii::Sampler sampler = nullptr;
    };
}
