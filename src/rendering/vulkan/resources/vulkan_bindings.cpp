#include "vulkan_resources.h"


#include "vulkan_utility.h"

namespace tr::Rendering::Vulkan {
    Handle<BindingLayout> VulkanResources::createBindingLayout(std::span<const BindingLayoutDesc> bindings) {
        std::vector<vk::DescriptorSetLayoutBinding> native;
        for (const auto& binding : bindings) {
            native.push_back({
                .binding = binding.binding,
                .descriptorType =
                    binding.type == BindingType::UniformBuffer
                    ? vk::DescriptorType::eUniformBuffer
                    : vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = toVulkanStages(binding.stages),
            });
        }
        Handle<BindingLayout> handle{ _nextHandle++ };
        _bindingLayouts.emplace(handle.index,
            VulkanBindingLayout{
                .layout = vk::raii::DescriptorSetLayout(_context.device, {
                    .bindingCount = static_cast<uint32_t>(native.size()),
                    .pBindings = native.data(),
                }),
            });
        return handle;
    }

    void VulkanResources::destroyBindingLayout(Handle<BindingLayout> handle) {
        _bindingLayouts.erase(handle.index);
    }

    Handle<BindingSet> VulkanResources::createBindingSet(Handle<BindingLayout> layout) {
        Handle<BindingSet> handle{ _nextHandle++ };
        auto allocation = _descriptorAllocator.allocate(*_bindingLayouts.at(layout.index).layout);
        _bindingSets.emplace(handle.index, VulkanBindingSet{
            .set = std::move(allocation.set),
            .poolIndex = allocation.poolIndex,
        });
        return handle;
    }

    void VulkanResources::destroyBindingSet(Handle<BindingSet> handle) {
        auto entry = _bindingSets.extract(handle.index);
        if (!entry.empty()) {
            _descriptorAllocator.free(entry.mapped().set, entry.mapped().poolIndex);
        }
    }

    void VulkanResources::updateBindingSet(Handle<BindingSet> set, std::span<const BindingDesc> bindings) {
        for (const auto& binding : bindings) {
            vk::DescriptorBufferInfo bufferInfo;
            vk::DescriptorImageInfo imageInfo;
            vk::WriteDescriptorSet write{
                .dstSet = bindingSet(set),
                .dstBinding = binding.binding,
                .descriptorCount = 1,
            };
            if (const auto* value = std::get_if<BufferBinding>(&binding.resource)) {
                const auto& buffer = _buffers.at(value->buffer.index);
                bufferInfo = {
                    .buffer = *buffer.buffer,
                    .range = buffer.size,
                };
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.pBufferInfo = &bufferInfo;
            }
            else {
                const auto& image = texture(std::get<ImageSamplerBinding>(binding.resource).texture);
                imageInfo = {
                    .sampler = image.sampler,
                    .imageView = image.view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                };
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.pImageInfo = &imageInfo;
            }
            _context.device.updateDescriptorSets(write, {});
        }
    }

    vk::DescriptorSet VulkanResources::bindingSet(Handle<BindingSet> handle) const {
        return *_bindingSets.at(handle.index).set;
    }
}
