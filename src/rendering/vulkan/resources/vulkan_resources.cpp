#include "vulkan_resources.h"

namespace tr::Rendering::Vulkan {
    VulkanResources::VulkanResources(VulkanContext& context, VulkanAllocator& allocator)
        : _context(context),
          _allocator(allocator),
          _cubemapConverter(context, allocator),
          _descriptorAllocator(context.device) {
        createFallbackTextures();
    }

    VulkanResources::~VulkanResources() {
        _context.device.waitIdle();
    }

    void VulkanResources::beginFrame(uint32_t slot) {
        _frameSlot = slot;
        _retiredTextures[slot].clear();
    }
}
