#pragma once

#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "handle.h"
#include "mesh.h"

namespace tr::Rendering::Vulkan {
    struct VulkanMesh {
        struct SubmeshLayout {
            uint32_t firstIndex;
            uint32_t indexCount;
        };

        tr::Resources::Handle<tr::Data::Mesh> source;
        std::vector<SubmeshLayout> subMeshesLayouts;

        vk::raii::Buffer vertexBuffer = nullptr;
        vk::raii::DeviceMemory vertexBufferMemory = nullptr;
        vk::raii::Buffer indexBuffer = nullptr;
        vk::raii::DeviceMemory indexBufferMemory = nullptr;
    };
}
