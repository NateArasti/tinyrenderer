#include "color_pass.h"

#include <array>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    ColorPass::ColorPass(VulkanContext& context, GBuffer& gBuffer, VulkanResources& resources)
        : _vulkanContext(context), _gBuffer(gBuffer), _resources(resources)
    { }

    void ColorPass::record(vk::raii::CommandBuffer& commandBuffer, const FrameContext& frame, std::span<const DrawCommand> drawCalls) {
        begin(commandBuffer, frame);
        for (const auto& drawCall : drawCalls) {
            draw(commandBuffer, drawCall, frame);
        }
        end(commandBuffer);
    }

    void ColorPass::begin(vk::raii::CommandBuffer& commandBuffer, const FrameContext& frame) {
#ifndef NDEBUG
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Color Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);
#endif

        const bool multisampled = _vulkanContext.msaaSamples != vk::SampleCountFlagBits::e1;
        imageBarrier(
            commandBuffer,
            multisampled ? *_gBuffer.colorImage.image : frame.targetImage,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );
        imageBarrier(
            commandBuffer,
            *_gBuffer.depthImage.image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = multisampled ? *_gBuffer.colorImage.view : frame.targetImageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode = multisampled
                ? vk::ResolveModeFlagBits::eAverage
                : vk::ResolveModeFlagBits::eNone,
            .resolveImageView = multisampled ? frame.targetImageView : vk::ImageView{},
            .resolveImageLayout = multisampled
                ? vk::ImageLayout::eColorAttachmentOptimal
                : vk::ImageLayout::eUndefined,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = multisampled
                ? vk::AttachmentStoreOp::eDontCare
                : vk::AttachmentStoreOp::eStore,
        };
        const vk::RenderingAttachmentInfo depthAttachment{
            .imageView = *_gBuffer.depthImage.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = vk::ClearDepthStencilValue(1.0f, 0)
        };
        const vk::RenderingInfo renderingInfo{
            .renderArea = { .offset = { 0, 0 }, .extent = frame.extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachment
        };
        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.setViewport(0, vk::Viewport(
            0.0f,
            0.0f,
            static_cast<float>(frame.extent.width),
            static_cast<float>(frame.extent.height),
            0.0f,
            1.0f
        ));
        commandBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, frame.extent));
    }

    void ColorPass::draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command, const FrameContext& frame) {
        const auto& mesh = _resources.mesh(command.mesh);
        if (command.subMeshIndex >= mesh.subMeshesLayouts.size()) {
            throw std::runtime_error("Draw command references a missing submesh");
        }
        commandBuffer.bindVertexBuffers(0, *mesh.vertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(*mesh.indexBuffer, 0, vk::IndexType::eUint32);
        const auto& material = _resources.material(command.material);
        if (material.shader == nullptr) throw std::runtime_error("Can't use material with no shader");
        const auto& shader = *material.shader;
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *shader.graphicsPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            shader.pipelineLayout,
            0,
            { frame.sceneDescriptorSet, *material.descriptorSet },
            nullptr
        );
        commandBuffer.pushConstants(
            shader.pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            vk::ArrayProxy<const glm::mat4>(command.modelMatrix)
        );
        const auto& subMesh = mesh.subMeshesLayouts[command.subMeshIndex];
        commandBuffer.drawIndexed(subMesh.indexCount, 1, subMesh.firstIndex, 0, 0);
    }

    void ColorPass::end(vk::raii::CommandBuffer& commandBuffer) {
        commandBuffer.endRendering();
#ifndef NDEBUG
        commandBuffer.endDebugUtilsLabelEXT();
#endif
    }
}
