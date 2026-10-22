#include "vulkan_descriptor_allocator.h"

#include "defines.h"

namespace tr::Rendering::Vulkan {
    void VulkanDescriptorAllocator::createPool() {
        const std::array sizes{
            vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, _nextPoolSize },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, _nextPoolSize },
            vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURES + _nextPoolSize * 4 },
        };
        _pools.push_back({
            .handle = vk::raii::DescriptorPool(_device, {
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = _nextPoolSize,
                .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                .pPoolSizes = sizes.data(),
            }),
        });
        _nextPoolSize *= 2;
    }

    VulkanDescriptorAllocator::Allocation VulkanDescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
        while (true) {
            for (uint32_t i = 0; i < _pools.size(); ++i) {
                auto& pool = _pools[i];
                if (pool.exhausted) {
                    continue;
                }

                try {
                    auto sets = _device.allocateDescriptorSets({
                        .descriptorPool = *pool.handle,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &layout,
                    });
                    return {
                        .set = std::move(sets.front()),
                        .poolIndex = i,
                    };
                }
                catch (const vk::OutOfPoolMemoryError&) {
                    pool.exhausted = true;
                }
                catch (const vk::FragmentedPoolError&) {
                    pool.exhausted = true;
                }
            }

            createPool();
        }
    }

    void VulkanDescriptorAllocator::free(vk::raii::DescriptorSet& set, uint32_t poolIndex) {
        set.clear();
        _pools.at(poolIndex).exhausted = false;
    }
}
