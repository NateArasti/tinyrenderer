#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "rendering_structs.h"

namespace tr::Rendering::Vulkan {
    struct VulkanImage {
        vk::raii::DeviceMemory memory = nullptr;
        vk::raii::Image image = nullptr;
        vk::raii::ImageView view = nullptr;
        vk::Format format = vk::Format::eUndefined;
        vk::Extent3D extent{};
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    };

    struct VulkanBuffer final : GPUBuffer {
        vk::raii::DeviceMemory memory = nullptr;
        vk::raii::Buffer buffer = nullptr;
        vk::DeviceSize size = 0;
    };

    struct VulkanTexture final : GPUTexture {
        VulkanImage allocation;
        vk::raii::Sampler ownedSampler = nullptr;
        vk::Image image;
        vk::ImageView view;
        vk::Sampler sampler;
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
    };

    struct VulkanBindingLayout final : BindingLayout {
        vk::raii::DescriptorSetLayout layout = nullptr;
    };

    struct VulkanBindingSet final : BindingSet {
        vk::raii::DescriptorSet set = nullptr;
        uint32_t poolIndex = 0;
    };

    struct VulkanPipelineLayout final : PipelineLayout {
        vk::raii::PipelineLayout layout = nullptr;
    };

    struct VulkanPipeline final : Pipeline {
        vk::raii::Pipeline pipeline = nullptr;
        Handle<PipelineLayout> layout;
        WindingOrder frontFace = CounterClockwise;
    };
}
