#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "rendering_structs.h"

namespace tr::Rendering::Vulkan {
    struct TextureAccess {
        vk::ImageLayout layout;
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;
    };

    TextureAccess toVulkanTextureState(TextureState state);
    vk::Format toVulkanFormat(Format format);
    Format fromVulkanFormat(vk::Format format);
    vk::DescriptorType toVulkanBindingType(BindingType bindingType);
    vk::ShaderStageFlags toVulkanStages(ShaderStages stages);
    vk::CompareOp toVulkanCompareOp(CompareOp compareOp);
    vk::CullModeFlags toVulkanCullMode(Data::CullMode cullMode);
    vk::FrontFace toVulkanFrontFace(WindingOrder winding);
    vk::BufferUsageFlags toVulkanBufferUsage(BufferUsageFlags usage);
    bool isDepthFormat(Format format);
}
