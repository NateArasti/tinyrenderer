#include "shadow_pass.h"

#include <stdexcept>

namespace tr::Rendering::Vulkan {
    ShadowPass::ShadowPass(
        VulkanContext& context,
        ResourceFactory& resourceFactory,
        VulkanResources& resources
    ) : RenderPass(resources), _vulkanContext(context)
    {
        _image = resourceFactory.createImage(
            SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
            _format,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eDepth
        );

        _vulkanContext.setDebugName(
            vk::ObjectType::eImage,
            uint64_t(static_cast<VkImage>(*_image.image)),
            "ShadowMap"
        );
        _vulkanContext.setDebugName(
            vk::ObjectType::eImageView,
            uint64_t(static_cast<VkImageView>(*_image.view)),
            "ShadowMapView"
        );

        vk::SamplerCreateInfo samplerInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .addressModeU = vk::SamplerAddressMode::eClampToBorder,
            .addressModeV = vk::SamplerAddressMode::eClampToBorder,
            .addressModeW = vk::SamplerAddressMode::eClampToBorder,
            .compareEnable = vk::True,
            .compareOp = vk::CompareOp::eLess,
            .borderColor = vk::BorderColor::eFloatOpaqueWhite,
        };
        _sampler = vk::raii::Sampler(_vulkanContext.device, samplerInfo);
    }

    void ShadowPass::createPipeline(
        const Data::Shader& shader,
        vk::VertexInputBindingDescription bindingDescription,
        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions
    ) {
        const auto& code = shader.getCode();
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule{ _vulkanContext.device, createInfo };

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = shader.vertName.c_str()
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo };

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()
        };
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList
        };
        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1
        };
		
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::True,
            .depthBiasConstantFactor = 1.25f,
            .depthBiasSlopeFactor = 1.75f,
            .lineWidth = 1.0f
        };

		vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1, 
            .sampleShadingEnable = vk::False
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
        };

		std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport, 
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
             .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
             .pDynamicStates = dynamicStates.data() 
        };

        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = 2 * sizeof(glm::mat4)
        };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };
        _pipelineLayout = vk::raii::PipelineLayout(_vulkanContext.device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {
                    .stageCount = 1,
                    .pStages = shaderStages,
                    .pVertexInputState = &vertexInputInfo,
                    .pInputAssemblyState = &inputAssembly,
                    .pViewportState = &viewportState,
                    .pRasterizationState = &rasterizer,
                    .pMultisampleState = &multisampling,
                    .pDepthStencilState  = &depthStencil,
                    .pDynamicState = &dynamicState,
                    .layout = _pipelineLayout,
                    .renderPass = nullptr,
                },
                {
                    .depthAttachmentFormat = _format
                }
        };

        _pipeline = vk::raii::Pipeline(
            _vulkanContext.device,
            nullptr,
            pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
        );
    }

    vk::DescriptorImageInfo ShadowPass::descriptorInfo() const {
        return {
            .sampler = *_sampler,
            .imageView = *_image.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
    }
    
    void ShadowPass::begin(vk::raii::CommandBuffer& commandBuffer) {
        transitionImageLayout(
            commandBuffer,
            *_image.image,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
            {}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        vk::RenderingAttachmentInfo depthAttachment{
            .imageView = *_image.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearDepthStencilValue(1.0f, 0)
        };
        vk::RenderingInfo renderingInfo{
            .renderArea = { {0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} },
            .layerCount = 1,
            .pDepthAttachment = &depthAttachment
        };
        
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Shadow Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D({0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}));
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);
    }

    void ShadowPass::draw(vk::raii::CommandBuffer& commandBuffer, const DrawCommand& command) {
        const auto& mesh = _resources.mesh(command.mesh);
        commandBuffer.bindVertexBuffers(0, *mesh.vertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(*mesh.indexBuffer, 0, vk::IndexType::eUint32);
        if (command.subMeshIndex >= mesh.subMeshesLayouts.size()) {
            throw std::runtime_error("Draw command references a missing submesh");
        }
        commandBuffer.pushConstants(
            _pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            vk::ArrayProxy<const glm::mat4>({ _context->sceneData->lightViewProj, command.modelMatrix })
        );
        const auto& subMesh = mesh.subMeshesLayouts[command.subMeshIndex];
        commandBuffer.drawIndexed(subMesh.indexCount, 1, subMesh.firstIndex, 0, 0);
    }

    void ShadowPass::end(vk::raii::CommandBuffer& commandBuffer) {
        commandBuffer.endRendering();
        commandBuffer.endDebugUtilsLabelEXT();
        transitionImageLayout(
            commandBuffer,
            *_image.image,
            vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests, vk::PipelineStageFlagBits2::eFragmentShader,
            vk::ImageAspectFlagBits::eDepth
        );
    }
}
