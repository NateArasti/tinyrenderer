#pragma once

#include "command_buffer.h"
#include "resources/vulkan_resources.h"

namespace tr::Rendering::Vulkan {
    class VulkanCommandBuffer : public CommandBuffer {
    private:
        VulkanContext& _context;
        VulkanResources& _resources;
        vk::raii::CommandBuffer* _commandBuffer = nullptr;
        vk::PipelineLayout _pipelineLayout;

        vk::RenderingAttachmentInfo createColorAttachment(const ColorAttachmentDesc& desc) const;
        vk::RenderingAttachmentInfo createDepthAttachment(const DepthAttachmentDesc& desc) const;

    public:
        VulkanCommandBuffer(VulkanContext& context, VulkanResources& resources);

        void reset(vk::raii::CommandBuffer& commandBuffer);
        const vk::raii::CommandBuffer& handle() const { return *_commandBuffer; }

        void beginDebugLabel(std::string_view name, const glm::vec4& color) override;
        void endDebugLabel() override;

        void beginRendering(RenderingDesc desc) override;
        void endRendering() override;
        void transition(Handle<GPUTexture> texture, TextureState current, TextureState target) override;

        void bindPipeline(Handle<Pipeline> pipeline) override;
        void setFrontFace(WindingOrder winding) override;
        void bindBindings(uint32_t setIndex, Handle<BindingSet> set) override;
        void pushConstants(PushConstantRange range, const void* bytes) override;
        void bindVertexBuffer(Handle<GPUBuffer> buffer) override;
        void bindIndexBuffer(Handle<GPUBuffer> buffer) override;

        void draw(uint32_t vertexCount, uint32_t firstVertex) override;
        void drawIndexed(
            uint32_t indexCount,
            uint32_t firstIndex,
            int32_t vertexOffset,
            uint32_t firstInstance,
            uint32_t instanceCount
        ) override;
    };
}
