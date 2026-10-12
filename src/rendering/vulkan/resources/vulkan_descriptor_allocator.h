#pragma once

#include <vector>

#include "vulkan_context.h"

namespace tr::Rendering::Vulkan {
    class VulkanDescriptorAllocator {
    private:
        struct Pool {
            vk::raii::DescriptorPool handle = nullptr;
            bool exhausted = false;
        };

        vk::raii::Device& _device;
        std::vector<Pool> _pools;
        uint32_t _nextPoolSize = 32;

        void createPool();

    public:
        struct Allocation {
            vk::raii::DescriptorSet set = nullptr;
            uint32_t poolIndex = 0;
        };

        explicit VulkanDescriptorAllocator(vk::raii::Device& device) : _device(device) {}

        Allocation allocate(vk::DescriptorSetLayout layout);
        void free(vk::raii::DescriptorSet& set, uint32_t poolIndex);
    };
}
