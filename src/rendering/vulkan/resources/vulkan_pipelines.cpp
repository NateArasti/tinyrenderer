#include "vulkan_resources.h"

#include "vulkan_utility.h"

namespace tr::Rendering::Vulkan {
    Handle<Pipeline> VulkanResources::createPipeline(Handle<PipelineLayout> layout, const PipelineDesc& desc) {
        const auto& code = desc.shader.getCode();
        vk::raii::ShaderModule module(_context.device, {
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        });
        std::vector<vk::PipelineShaderStageCreateInfo> stages{ {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *module,
            .pName = desc.shader.vertName.c_str(),
        } };
        if (!desc.shader.fragName.empty()) {
            stages.push_back({
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *module,
                .pName = desc.shader.fragName.c_str(),
            });
        }
        vk::VertexInputBindingDescription binding{ 0, desc.vertexBinding.size, vk::VertexInputRate::eVertex };
        std::vector<vk::VertexInputAttributeDescription> attributes;
        for (auto attribute : desc.vertexAttributes) {
            attributes.push_back({ attribute.location, 0, toVulkanFormat(attribute.format), attribute.offset });
        }
        vk::PipelineVertexInputStateCreateInfo vertexInput{
            .vertexBindingDescriptionCount = desc.vertexBinding.size ? 1u : 0u,
            .pVertexBindingDescriptions = &binding,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
            .pVertexAttributeDescriptions = attributes.data(),
        };
        vk::PipelineInputAssemblyStateCreateInfo assembly{ .topology = vk::PrimitiveTopology::eTriangleList };
        vk::PipelineViewportStateCreateInfo viewport{
            .viewportCount = 1,
            .scissorCount = 1,
        };
        vk::PipelineRasterizationStateCreateInfo rasterization{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = toVulkanCullMode(desc.rasterization.cullMode),
            .frontFace = toVulkanFrontFace(desc.rasterization.frontFace),
            .depthBiasEnable = desc.rasterization.depthBiasEnabled,
            .depthBiasConstantFactor = desc.rasterization.depthBiasConstant,
            .depthBiasSlopeFactor = desc.rasterization.depthBiasSlope,
            .lineWidth = 1.0f,
        };
        vk::PipelineMultisampleStateCreateInfo multisample{
            .rasterizationSamples = static_cast<vk::SampleCountFlagBits>(desc.multisample)
        };
        vk::PipelineDepthStencilStateCreateInfo depth{
            .depthTestEnable = desc.depth.testEnabled,
            .depthWriteEnable = desc.depth.writeEnabled,
            .depthCompareOp = toVulkanCompareOp(desc.depth.compareOp),
        };
        vk::PipelineColorBlendAttachmentState attachment{
            .blendEnable = desc.blendEnabled,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };
        const bool hasColor = desc.colorFormat != Undefined;
        vk::PipelineColorBlendStateCreateInfo blend{
            .attachmentCount = hasColor ? 1u : 0u,
            .pAttachments = &attachment,
        };
        const std::array dynamic{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
            vk::DynamicState::eFrontFace,
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamic.size()),
            .pDynamicStates = dynamic.data(),
        };
        const auto colorFormat = toVulkanFormat(desc.colorFormat);
        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> info{
            {
                .stageCount = static_cast<uint32_t>(stages.size()),
                .pStages = stages.data(),
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &assembly,
                .pViewportState = &viewport,
                .pRasterizationState = &rasterization,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth,
                .pColorBlendState = &blend,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout(layout),
            },
            {
                .colorAttachmentCount = hasColor ? 1u : 0u,
                .pColorAttachmentFormats = &colorFormat,
                .depthAttachmentFormat = toVulkanFormat(desc.depthFormat),
            }
        };
        Handle<Pipeline> handle{ _nextHandle++ };
        _pipelines.emplace(handle.index,
            VulkanPipeline{
                .pipeline = vk::raii::Pipeline(
                    _context.device,
                    nullptr,
                    info.get<vk::GraphicsPipelineCreateInfo>()),
                .layout = layout,
                .frontFace = desc.rasterization.frontFace,
            });
        return handle;
    }

    Handle<PipelineLayout> VulkanResources::createPipelineLayout(const PipelineLayoutDesc& desc) {
        std::vector<vk::DescriptorSetLayout> sets;
        for (auto set : desc.sets) {
            sets.push_back(*_bindingLayouts.at(set.index).layout);
        }
        std::vector<vk::PushConstantRange> ranges;
        for (auto range : desc.pushConstants) {
            ranges.push_back({ toVulkanStages(range.stages), range.offset, range.size });
        }
        Handle<PipelineLayout> handle{ _nextHandle++ };
        _pipelineLayouts.emplace(handle.index,
            VulkanPipelineLayout{
                .layout = vk::raii::PipelineLayout(_context.device, {
                    .setLayoutCount = static_cast<uint32_t>(sets.size()),
                    .pSetLayouts = sets.data(),
                    .pushConstantRangeCount = static_cast<uint32_t>(ranges.size()),
                    .pPushConstantRanges = ranges.data(),
                }),
            });
        return handle;
    }

    void VulkanResources::destroyPipelineLayout(Handle<PipelineLayout> handle) {
        _pipelineLayouts.erase(handle.index);
    }

    void VulkanResources::destroyPipeline(Handle<Pipeline> handle) {
        _pipelines.erase(handle.index);
    }

    const VulkanPipeline& VulkanResources::pipeline(Handle<Pipeline> handle) const {
        return _pipelines.at(handle.index);
    }

    vk::PipelineLayout VulkanResources::pipelineLayout(Handle<PipelineLayout> handle) const {
        return *_pipelineLayouts.at(handle.index).layout;
    }
}
