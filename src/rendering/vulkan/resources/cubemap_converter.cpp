#include "cubemap_converter.h"

#include <array>
#include <stdexcept>
#include <vector>

#include "equirectangular_conversion.h"
#include "import_error.h"

namespace tr::Rendering::Vulkan {
    CubemapConverter::CubemapConverter(VulkanContext& context, ResourceFactory& factory)
        : _context(context), _factory(factory)
    {
        _commandPool = vk::raii::CommandPool(
            context.device,
            vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = context.queueIndex
            }
        );

        const std::array<float, 6> vertices{
            -1.0f, -1.0f,
             3.0f, -1.0f,
            -1.0f,  3.0f
        };
        _vertexBuffer = factory.uploadBuffer(
            std::as_bytes(std::span(vertices)),
            vk::BufferUsageFlagBits::eVertexBuffer
        );

        const vk::DescriptorSetLayoutBinding sourceBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };
        _descriptorSetLayout = vk::raii::DescriptorSetLayout(
            context.device,
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = 1,
                .pBindings = &sourceBinding
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
        _descriptorSet = std::move(context.device.allocateDescriptorSets({
            .descriptorPool = *_descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*_descriptorSetLayout
        }).front());

        _sourceSampler = vk::raii::Sampler(
            context.device,
            vk::SamplerCreateInfo{
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eNearest,
                .addressModeU = vk::SamplerAddressMode::eRepeat,
                .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                .maxLod = 0.0f
            }
        );
    }

    const CubemapConverter::ConversionPipeline& CubemapConverter::pipeline(vk::Format destinationFormat) {
        if (_pipelines.contains(destinationFormat)) {
            return _pipelines[destinationFormat];
        }

        const auto formatProperties = _context.physicalDevice.getFormatProperties(destinationFormat);
        if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)) {
            throw std::runtime_error(
                "Cubemap destination format cannot be used as a color attachment"
            );
        }

        tr::Data::EmbeddedShaders::EquirectangularConversion shader;
        const auto& code = static_cast<const tr::Data::Shader&>(shader).getCode();
        vk::raii::ShaderModule shaderModule(
            _context.device,
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
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f
        };
        const vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False
        };
        const vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = vk::False,
            .colorWriteMask =
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA
        };
        const vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False,
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
        const vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
            .offset = 0,
            .size = sizeof(uint32_t)
        };

        ConversionPipeline result;

        result.layout = vk::raii::PipelineLayout(
            _context.device,
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &*_descriptorSetLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange
            }
        );

        const vk::PipelineRenderingCreateInfo renderingInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &destinationFormat
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
                .layout = *result.layout
            },
            renderingInfo
        };
        result.pipeline = vk::raii::Pipeline(
            _context.device,
            nullptr,
            pipelineInfo.get<vk::GraphicsPipelineCreateInfo>()
        );
        _pipelines[destinationFormat] = std::move(result);

        return _pipelines[destinationFormat];
    }

    vk::raii::CommandBuffer CubemapConverter::beginCommands() {
        vk::raii::CommandBuffer commandBuffer = std::move(
            _context.device.allocateCommandBuffers({
                .commandPool = *_commandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            }).front()
        );
        commandBuffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });
        return commandBuffer;
    }

    void CubemapConverter::submitCommands(
        const vk::raii::CommandBuffer& commandBuffer
    ) const {
        commandBuffer.end();
        const vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        _context.queue.submit(submitInfo, nullptr);
        _context.queue.waitIdle();
    }

    GPUImage CubemapConverter::convertEquirectangular(
        const GPUImage& source,
        vk::Format destinationFormat
    ) {
        if (
            source.extent.width < 4 ||
            source.extent.height < 2 ||
            source.extent.width != source.extent.height * 2
        ) {
            throw tr::Loading::ImportError(
                "Equirectangular image must have a 2:1 aspect ratio"
            );
        }

        const ConversionPipeline& conversionPipeline = pipeline(destinationFormat);

        const uint32_t faceSize = source.extent.width / 4;
        GPUImage target = _factory.createImage({
            .flags = vk::ImageCreateFlagBits::eCubeCompatible,
            .imageType = vk::ImageType::e2D,
            .viewType = vk::ImageViewType::eCube,
            .format = destinationFormat,
            .extent = { faceSize, faceSize, 1 },
            .mipLevels = 1,
            .arrayLayers = 6,
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eSampled
        });

        std::vector<vk::raii::ImageView> faceViews;
        faceViews.reserve(6);
        for (uint32_t face = 0; face < 6; ++face) {
            faceViews.push_back(_factory.createImageView(target.image, {
                .viewType = vk::ImageViewType::e2D,
                .format = destinationFormat,
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .mipLevels = 1,
                .baseArrayLayer = face,
                .arrayLayers = 1
            }));
        }

        const vk::DescriptorImageInfo sourceInfo{
            .sampler = *_sourceSampler,
            .imageView = *source.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        _context.device.updateDescriptorSets({{
            .dstSet = *_descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &sourceInfo
        }}, {});

        auto commandBuffer = beginCommands();

        const vk::ImageMemoryBarrier2 beginBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = *target.image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6
            }
        };
        commandBuffer.pipelineBarrier2({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &beginBarrier
        });

        commandBuffer.setViewport(0, vk::Viewport(
            0.0f,
            0.0f,
            static_cast<float>(faceSize),
            static_cast<float>(faceSize),
            0.0f,
            1.0f
        ));
        commandBuffer.setScissor(
            0,
            vk::Rect2D({ 0, 0 }, { faceSize, faceSize })
        );
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            *conversionPipeline.pipeline
        );
        commandBuffer.bindVertexBuffers(0, *_vertexBuffer.buffer, { 0 });
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *conversionPipeline.layout,
            0,
            { *_descriptorSet },
            {}
        );

        for (uint32_t face = 0; face < 6; ++face) {
            const vk::RenderingAttachmentInfo colorAttachment{
                .imageView = *faceViews[face],
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eDontCare,
                .storeOp = vk::AttachmentStoreOp::eStore
            };
            commandBuffer.beginRendering({
                .renderArea = { { 0, 0 }, { faceSize, faceSize } },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment
            });
            commandBuffer.pushConstants(
                *conversionPipeline.layout,
                vk::ShaderStageFlagBits::eFragment,
                0,
                vk::ArrayProxy<const uint32_t>(face)
            );
            commandBuffer.draw(3, 1, 0, 0);
            commandBuffer.endRendering();
        }

        const vk::ImageMemoryBarrier2 endBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = *target.image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6
            }
        };
        commandBuffer.pipelineBarrier2({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &endBarrier
        });

        submitCommands(commandBuffer);
        return target;
    }
}
