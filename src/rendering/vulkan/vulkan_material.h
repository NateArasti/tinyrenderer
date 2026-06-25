#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "material.h"

namespace tr::Rendering::Vulkan {
    struct VulkanMaterial {
        tr::Resources::Handle<tr::Data::Material> source;
        tr::Resources::Handle<tr::Data::Shader> shader;
    };
}
