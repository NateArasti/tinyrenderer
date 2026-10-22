#include "vulkan_command_buffer.h"

#include "vulkan_utility.h"

namespace tr::Rendering::Vulkan {
    VulkanCommandBuffer::VulkanCommandBuffer(VulkanContext& context, VulkanResources& resources)
        : _context(context), _resources(resources) {}

    void VulkanCommandBuffer::reset(vk::raii::CommandBuffer& commandBuffer) {
        _commandBuffer = &commandBuffer;
        _pipelineLayout = nullptr;
    }

    void VulkanCommandBuffer::beginDebugLabel(std::string_view name, const glm::vec4& color) {
        if (!_context.debugUtilsEnabled()) {
            return;
        }

        const std::string labelName(name);
        vk::DebugUtilsLabelEXT label{
            .pLabelName = labelName.c_str(),
        };
        label.color[0] = color.r;
        label.color[1] = color.g;
        label.color[2] = color.b;
        label.color[3] = color.a;
        _commandBuffer->beginDebugUtilsLabelEXT(label);
    }

    void VulkanCommandBuffer::endDebugLabel() {
        if (_context.debugUtilsEnabled()) {
            _commandBuffer->endDebugUtilsLabelEXT();
        }
    }

    void VulkanCommandBuffer::transition(Handle<GPUTexture> handle, TextureState current, TextureState target) {
        const auto& texture = _resources.texture(handle);
        const auto before = toVulkanTextureState(current);
        const auto after = toVulkanTextureState(target);
        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = before.stages,
            .srcAccessMask = before.access,
            .dstStageMask = after.stages,
            .dstAccessMask = after.access,
            .oldLayout = before.layout,
            .newLayout = after.layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = texture.image,
            .subresourceRange = {
                .aspectMask = texture.aspectMask,
                .levelCount = texture.mipLevels,
                .layerCount = texture.arrayLayers,
            },
        };
        _commandBuffer->pipelineBarrier2({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        });
    }

    vk::RenderingAttachmentInfo VulkanCommandBuffer::createColorAttachment(const ColorAttachmentDesc& desc) const {
        if (!desc.target.isValid()) {
            return {};
        }

        vk::RenderingAttachmentInfo attachment{
            .imageView = _resources.texture(desc.target).view,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = desc.load ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue(
                std::array{ desc.clearValue.r, desc.clearValue.g, desc.clearValue.b, desc.clearValue.a }
            ),
        };
        if (desc.resolve.isValid()) {
            attachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
            attachment.resolveImageView = _resources.texture(desc.resolve).view;
            attachment.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }
        return attachment;
    }

    vk::RenderingAttachmentInfo VulkanCommandBuffer::createDepthAttachment(const DepthAttachmentDesc& desc) const {
        if (!desc.target.isValid()) {
            return {};
        }

        return {
            .imageView = _resources.texture(desc.target).view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = desc.load ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearDepthStencilValue(1.0f, 0),
        };
    }

    void VulkanCommandBuffer::beginRendering(RenderingDesc desc) {
        const auto colorAttachment = createColorAttachment(desc.colorAttachment);
        const auto depthAttachment = createDepthAttachment(desc.depthAttachment);
        const bool hasColor = desc.colorAttachment.target.isValid();
        const bool hasDepth = desc.depthAttachment.target.isValid();
        const vk::Rect2D renderArea{
            .offset = { static_cast<int32_t>(desc.renderArea.offset.x), static_cast<int32_t>(desc.renderArea.offset.y) },
            .extent = { static_cast<uint32_t>(desc.renderArea.extent.x),
                static_cast<uint32_t>(desc.renderArea.extent.y) },
        };
        _commandBuffer->beginRendering({
            .renderArea = renderArea,
            .layerCount = 1,
            .colorAttachmentCount = hasColor ? 1u : 0u,
            .pColorAttachments = hasColor ? &colorAttachment : nullptr,
            .pDepthAttachment = hasDepth ? &depthAttachment : nullptr,
        });
        _commandBuffer->setViewport(
            0,
            vk::Viewport(
                desc.renderArea.offset.x,
                desc.renderArea.offset.y,
                desc.renderArea.extent.x,
                desc.renderArea.extent.y,
                0.0f,
                1.0f
            )
        );
        _commandBuffer->setScissor(0, renderArea);
    }

    void VulkanCommandBuffer::endRendering() {
        _commandBuffer->endRendering();
    }

    void VulkanCommandBuffer::bindPipeline(Handle<Pipeline> handle) {
        const auto& pipeline = _resources.pipeline(handle);
        _pipelineLayout = _resources.pipelineLayout(pipeline.layout);
        _commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.pipeline);
        setFrontFace(pipeline.frontFace);
    }

    void VulkanCommandBuffer::setFrontFace(WindingOrder winding) {
        _commandBuffer->setFrontFace(toVulkanFrontFace(winding));
    }

    void VulkanCommandBuffer::bindBindings(uint32_t setIndex, Handle<BindingSet> set) {
        _commandBuffer->bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            _pipelineLayout,
            setIndex,
            { _resources.bindingSet(set) },
            {}
        );
    }

    void VulkanCommandBuffer::pushConstants(PushConstantRange range, const void* bytes) {
        _commandBuffer->pushConstants<uint8_t>(
            _pipelineLayout,
            toVulkanStages(range.stages),
            range.offset,
            vk::ArrayProxy<const uint8_t>(range.size, static_cast<const uint8_t*>(bytes))
        );
    }

    void VulkanCommandBuffer::bindVertexBuffer(Handle<GPUBuffer> buffer) {
        _commandBuffer->bindVertexBuffers(0, { *_resources.buffer(buffer).buffer }, { 0 });
    }

    void VulkanCommandBuffer::bindIndexBuffer(Handle<GPUBuffer> buffer) {
        _commandBuffer->bindIndexBuffer(*_resources.buffer(buffer).buffer, 0, vk::IndexType::eUint32);
    }

    void VulkanCommandBuffer::draw(uint32_t vertexCount, uint32_t firstVertex) {
        _commandBuffer->draw(vertexCount, 1, firstVertex, 0);
    }

    void VulkanCommandBuffer::drawIndexed(
        uint32_t indexCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance,
        uint32_t instanceCount
    ) {
        _commandBuffer->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }
}
