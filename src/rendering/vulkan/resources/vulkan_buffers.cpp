#include "vulkan_resources.h"

#include <cstring>
#include <stdexcept>

#include "vulkan_utility.h"

namespace tr::Rendering::Vulkan {
    Handle<GPUBuffer> VulkanResources::createBuffer(const BufferDesc& desc) {
        auto buffer = _allocator.createBuffer(
            desc.data.size(),
            toVulkanBufferUsage(desc.usageFlags),
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        Handle<GPUBuffer> handle{ _nextHandle++ };
        _buffers.emplace(handle.index, std::move(buffer));
        updateBuffer(handle, desc.data);
        return handle;
    }

    void VulkanResources::updateBuffer(Handle<GPUBuffer> handle, std::span<const std::byte> bytes) {
        auto& buffer = _buffers.at(handle.index);
        if (bytes.size() > buffer.size) {
            throw std::invalid_argument("Buffer update exceeds allocation");
        }
        auto* data = buffer.memory.mapMemory(0, bytes.size());
        std::memcpy(data, bytes.data(), bytes.size());
        buffer.memory.unmapMemory();
    }

    void VulkanResources::destroyBuffer(Handle<GPUBuffer> handle) {
        _buffers.erase(handle.index);
    }

    const VulkanBuffer& VulkanResources::buffer(Handle<GPUBuffer> handle) const {
        return _buffers.at(handle.index);
    }
}
