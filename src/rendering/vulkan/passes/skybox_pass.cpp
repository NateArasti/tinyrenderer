#include "skybox_pass.h"

#include <array>
#include <span>

#include "skybox.h"

namespace tr::Rendering::Vulkan {
    SkyboxPass::SkyboxPass(
        VulkanContext& context,
        VulkanResources& resources,
        GBuffer& gBuffer,
        ResourceFactory& factory,
        vk::DescriptorSetLayout sceneLayout,
        vk::Format colorFormat
    ) :
        _vulkanContext(context),
        _gBuffer(gBuffer),
        _resources(resources)
    {
        const std::array<float, 6> vertices{
            -1.0f, -1.0f,
             3.0f, -1.0f,
            -1.0f,  3.0f
        };
        _vertexBuffer = factory.uploadBuffer(
            std::as_bytes(std::span(vertices)),
            vk::BufferUsageFlagBits::eVertexBuffer
        );

        const vk::DescriptorSetLayoutBinding cubemapBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };
        _cubemapLayout = vk::raii::DescriptorSetLayout(
            context.device,
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = 1,
                .pBindings = &cubemapBinding
            }
        );

        const vk::DescriptorPoolSize poolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1
        };
        _descriptorPool = vk::raii::DescriptorPool(
            context.device,
            vk::DescriptorPoolCreateInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize
            }
        );
        _cubemapSet = std::move(context.device.allocateDescriptorSets({
            .descriptorPool = *_descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*_cubemapLayout
        }).front());

        const std::array setLayouts{ sceneLayout, *_cubemapLayout };
        _pipelineLayout = vk::raii::PipelineLayout(
            context.device,
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
                .pSetLayouts = setLayouts.data()
            }
        );
        createPipeline(colorFormat);
    }

    void SkyboxPass::createPipeline(vk::Format colorFormat) {
        const Data::EmbeddedShaders::Skybox shader;
        const auto& code = static_cast<const Data::Shader&>(shader).getCode();
        const vk::raii::ShaderModule shaderModule(
            _vulkanContext.device,
            vk::ShaderModuleCreateInfo{
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(code.data())
            }
        );

        const std::array shaderStages{
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *shaderModule,
                .pName = shader.vertName.c_str()
            },
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *shaderModule,
                .pName = shader.fragName.c_str()
            }
        };
        const vk::VertexInputBindingDescription vertexBinding{
            .binding = 0,
            .stride = 2 * sizeof(float),
            .inputRate = vk::VertexInputRate::eVertex
        };
        const vk::VertexInputAttributeDescription vertexAttribute{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = 0
        };
        const vk::PipelineVertexInputStateCreateInfo vertexInput{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexAttribute
        };
        const vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList
        };
        const vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1
        };
        const vk::PipelineRasterizationStateCreateInfo rasterizer{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .lineWidth = 1.0f
        };
        const vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = _vulkanContext.msaaSamples
        };
        const vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .colorWriteMask =
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA
        };
        const vk::PipelineColorBlendStateCreateInfo colorBlending{
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };
        const std::array dynamicStates{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        const vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        const vk::PipelineRenderingCreateInfo renderingInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat
        };
        vk::StructureChain<
            vk::GraphicsPipelineCreateInfo,
            vk::PipelineRenderingCreateInfo
        > pipelineInfo{
            {
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = *_pipelineLayout
            },
            renderingInfo
        };
        _pipeline = vk::raii::Pipeline(
            _vulkanContext.device,
            nullptr,
            pipelineInfo.get<vk::GraphicsPipelineCreateInfo>()
        );
    }

    void SkyboxPass::recreate(vk::Format colorFormat) {
        createPipeline(colorFormat);
    }

    void SkyboxPass::bindCubemap(const VulkanCubemap& cubemap) {
        const vk::DescriptorImageInfo imageInfo{
            .sampler = *cubemap.sampler,
            .imageView = *cubemap.image.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        _vulkanContext.device.updateDescriptorSets({{
            .dstSet = *_cubemapSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo
        }}, {});
    }

    void SkyboxPass::record(
        vk::raii::CommandBuffer& commandBuffer,
        const FrameContext& frame,
        const Data::Environment& environment
    ) {
#ifndef NDEBUG
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.setPLabelName("Skybox Pass");
        commandBuffer.beginDebugUtilsLabelEXT(labelInfo);
#endif

        const bool multisampled = _vulkanContext.msaaSamples != vk::SampleCountFlagBits::e1;
        if (multisampled) {
            imageBarrier(
                commandBuffer,
                *_gBuffer.colorImage.image,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                {},
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::ImageAspectFlagBits::eColor
            );
        }

        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = multisampled ? *_gBuffer.colorImage.view : frame.targetImageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue(std::array{
                environment.clearColor.r,
                environment.clearColor.g,
                environment.clearColor.b,
                1.0f
            })
        };
        const vk::RenderingInfo renderingInfo{
            .renderArea = { .offset = { 0, 0 }, .extent = frame.extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
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

        const VulkanCubemap* cubemap = _resources.cubemap(environment.skyboxHandle);
        if (cubemap != nullptr) {
            if (_boundCubemap != cubemap->handle) {
                bindCubemap(*cubemap);
                _boundCubemap = cubemap->handle;
            }
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);
            commandBuffer.bindVertexBuffers(0, *_vertexBuffer.buffer, { 0 });
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *_pipelineLayout,
                0,
                { frame.sceneDescriptorSet, *_cubemapSet },
                {}
            );
            commandBuffer.draw(3, 1, 0, 0);
        }

        commandBuffer.endRendering();
#ifndef NDEBUG
        commandBuffer.endDebugUtilsLabelEXT();
#endif
    }
}
