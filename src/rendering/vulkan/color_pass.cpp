#include "color_pass.h"

#include <array>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    ColorPass::ColorPass(
        VulkanContext& context,
        ResourceFactory& factory,
        Swapchain& swapchain,
        VulkanResources& resources
    ) :
        RenderPass(resources),
        _vulkanContext(context),
        _factory(factory),
        _swapchain(swapchain)
    {
        createAttachments();
    }

    void ColorPass::createAttachments() {
        _colorImage = _factory.createImage(
            _swapchain.extent().width,
            _swapchain.extent().height,
            _swapchain.format(),
            1,
            _vulkanContext.msaaSamples,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor
        );
        _depthImage = _factory.createImage(
            _swapchain.extent().width,
            _swapchain.extent().height,
            _vulkanContext.depthFormat,
            1,
            _vulkanContext.msaaSamples,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eDepth
        );
    }

    void ColorPass::recreate() {
        createAttachments();
    }

    void ColorPass::begin(vk::raii::CommandBuffer& commandBuffer) {
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Color Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);

        transitionImageLayout(
            commandBuffer,
            _context->targetImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );
        transitionImageLayout(
            commandBuffer,
            *_colorImage.image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );
        transitionImageLayout(
            commandBuffer,
            *_depthImage.image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        vk::ClearValue clearColor;
        clearColor.color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *_colorImage.view,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode = vk::ResolveModeFlagBits::eAverage,
            .resolveImageView = _context->targetImageView,
            .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = clearColor
        };
        const vk::RenderingAttachmentInfo depthAttachment{
            .imageView = *_depthImage.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = vk::ClearDepthStencilValue(1.0f, 0)
        };
        const vk::RenderingInfo renderingInfo{
            .renderArea = { .offset = { 0, 0 }, .extent = _context->extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachment
        };
        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.setViewport(0, vk::Viewport(
            0.0f,
            0.0f,
            static_cast<float>(_context->extent.width),
            static_cast<float>(_context->extent.height),
            0.0f,
            1.0f
        ));
        commandBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, _context->extent));
    }

    void ColorPass::draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command) {
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
            { _context->sceneDescriptorSet, *material.descriptorSet },
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
        commandBuffer.endDebugUtilsLabelEXT();
    }
}
