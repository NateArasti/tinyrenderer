#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "shader.h"

namespace tr::Rendering::Vulkan {
    struct VulkanShader {
        const tr::Data::Shader* source = nullptr;
        vk::raii::PipelineLayout pipelineLayout = nullptr;
        vk::raii::Pipeline graphicsPipeline = nullptr;
        vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    };
}
