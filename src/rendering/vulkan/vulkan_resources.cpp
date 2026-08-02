#include "vulkan_resources.h"

#include <array>
#include <cstring>
#include <ranges>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    namespace {
        vk::VertexInputBindingDescription bindingDescription() {
            return {
                .binding = 0,
                .stride = sizeof(tr::Data::Mesh::Vertex),
                .inputRate = vk::VertexInputRate::eVertex
            };
        }

        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions() {
            using Vertex = tr::Data::Mesh::Vertex;
            return {{
                { .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, position) },
                { .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, normal) },
                { .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, uv) },
                { .location = 3, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Vertex, color) }
            }};
        }
    }

    VulkanResources::VulkanResources(
        VulkanContext& context,
        ResourceFactory& factory,
        const vk::raii::DescriptorSetLayout& sceneDescriptorSetLayout,
        vk::Format colorFormat
    ) :
        _context(context),
        _factory(factory),
        _sceneDescriptorSetLayout(sceneDescriptorSetLayout),
        _colorFormat(colorFormat)
    {
        createDescriptorPool();
        createFallbackTexture();
    }

    void VulkanResources::createDescriptorPool() {
        std::array poolSizes {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_MATERIALS),
            vk::DescriptorPoolSize(
                vk::DescriptorType::eCombinedImageSampler,
                MAX_MATERIALS * MAX_TEXTURES_PER_MATERIAL
            )
        };
        _materialDescriptorPool = vk::raii::DescriptorPool(
            _context.device,
            vk::DescriptorPoolCreateInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = MAX_MATERIALS,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()
            }
        );
    }

    void VulkanResources::createFallbackTexture() {
        constexpr std::array<uint8_t, 4> white = { 255, 255, 255, 255 };
        _fallbackTexture.image = _factory.uploadTexture(
            std::as_bytes(std::span(white)),
            1,
            1,
            vk::Format::eR8G8B8A8Unorm,
            false
        );
        _fallbackTexture.sampler = vk::raii::Sampler(
            _context.device,
            vk::SamplerCreateInfo{
                .magFilter = vk::Filter::eNearest,
                .minFilter = vk::Filter::eNearest,
                .mipmapMode = vk::SamplerMipmapMode::eNearest,
                .addressModeU = vk::SamplerAddressMode::eRepeat,
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat
            }
        );
    }

    void VulkanResources::clear() {
        _context.device.waitIdle();
        _materials.clear();
        _textures.clear();
        _meshes.clear();
        ++_generation;
        _textureIndex = 0;
        _meshIndex = 0;
    }

    VulkanShader VulkanResources::createShader(
        const tr::Data::Shader& shader,
        tr::Data::BlendMode blendMode
    ) {
        const auto& code = shader.getCode();
        vk::raii::ShaderModule shaderModule(
            _context.device,
            vk::ShaderModuleCreateInfo{
                .codeSize = code.size() * sizeof(char),
                .pCode = reinterpret_cast<const uint32_t*>(code.data())
            }
        );
        std::array shaderStages = {
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = shaderModule,
                .pName = shader.vertName.c_str()
            },
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = shaderModule,
                .pName = shader.fragName.c_str()
            }
        };

        const auto vertexBinding = bindingDescription();
        const auto vertexAttributes = attributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInput{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.data()
        };
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList
        };
        vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f
        };
        switch (shader.cullMode) {
            case tr::Data::CullMode::None: rasterizer.cullMode = vk::CullModeFlagBits::eNone; break;
            case tr::Data::CullMode::Front: rasterizer.cullMode = vk::CullModeFlagBits::eFront; break;
            case tr::Data::CullMode::Back: rasterizer.cullMode = vk::CullModeFlagBits::eBack; break;
            case tr::Data::CullMode::Both: rasterizer.cullMode = vk::CullModeFlagBits::eFrontAndBack; break;
            default: rasterizer.cullMode = vk::CullModeFlagBits::eBack; break;
        }

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = _context.msaaSamples,
            .sampleShadingEnable = vk::False
        };
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        if (blendMode == tr::Data::BlendMode::Transparent) {
            colorBlendAttachment = {
                .blendEnable = vk::True,
                .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                .colorBlendOp = vk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                .alphaBlendOp = vk::BlendOp::eAdd,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
            };
            depthStencil = {
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::False,
                .depthCompareOp = vk::CompareOp::eLess
            };
        }
        else {
            colorBlendAttachment = {
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
            };
            depthStencil = {
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::True,
                .depthCompareOp = vk::CompareOp::eLess
            };
        }
        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };
        const std::array dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };
        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(glm::mat4)
        };

        std::vector<vk::DescriptorSetLayoutBinding> shaderBindings = {{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        }};
        uint32_t textureBinding = 1;
        for (const auto& param : shader.params) {
            if (shader.isTextureParam(param.defaultValue)) {
                shaderBindings.push_back({
                    .binding = textureBinding++,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eFragment
                });
            }
        }
        vk::raii::DescriptorSetLayout materialLayout(
            _context.device,
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(shaderBindings.size()),
                .pBindings = shaderBindings.data()
            }
        );
        const std::array setLayouts = { *_sceneDescriptorSetLayout, *materialLayout };
        vk::raii::PipelineLayout pipelineLayout(
            _context.device,
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
                .pSetLayouts = setLayouts.data(),
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange
            }
        );

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineInfo = {
            {
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = nullptr
            },
            {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &_colorFormat,
                .depthAttachmentFormat = _context.depthFormat
            }
        };
        vk::raii::Pipeline pipeline(
            _context.device,
            nullptr,
            pipelineInfo.get<vk::GraphicsPipelineCreateInfo>()
        );
        return {
            .source = &shader,
            .pipelineLayout = std::move(pipelineLayout),
            .graphicsPipeline = std::move(pipeline),
            .descriptorSetLayout = std::move(materialLayout)
        };
    }

    void VulkanResources::createBaseShaders(tr::Data::Shader& shader) {
        _opaqueShader = createShader(shader, tr::Data::BlendMode::Opaque);
        _transparentShader = createShader(shader, tr::Data::BlendMode::Transparent);
    }

    void VulkanResources::recreateBaseShaders(vk::Format colorFormat) {
        _colorFormat = colorFormat;
        if (_opaqueShader.source != nullptr) {
            auto* source = _opaqueShader.source;
            _opaqueShader = createShader(*source, tr::Data::BlendMode::Opaque);
            _transparentShader = createShader(*source, tr::Data::BlendMode::Transparent);
        }
    }

    tr::Resources::Handle<tr::Data::Texture> VulkanResources::createTexture(
        const tr::Data::Texture& texture
    ) {
        const vk::Format format = texture.colorSpace == tr::Data::TextureColorSpace::SRGB
            ? vk::Format::eR8G8B8A8Srgb
            : vk::Format::eR8G8B8A8Unorm;
        VulkanTexture result{ .handle = { _textureIndex++, _generation } };
        result.image = _factory.uploadTexture(
            std::as_bytes(std::span(texture.pixels)),
            texture.width,
            texture.height,
            format,
            true
        );
        const auto properties = _context.physicalDevice.getProperties();
        result.sampler = vk::raii::Sampler(_context.device, vk::SamplerCreateInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways
        });
        const auto handle = result.handle;
        _textures[handle] = std::move(result);
        return handle;
    }

    tr::Resources::Handle<tr::Data::Mesh> VulkanResources::createMesh(const tr::Data::Mesh& mesh) {
        VulkanMesh result{ .handle = { _meshIndex++, _generation } };
        auto vertices = _factory.uploadBuffer(
            std::as_bytes(std::span(mesh.vertices)),
            vk::BufferUsageFlagBits::eVertexBuffer
        );
        result.vertexBuffer = std::move(vertices.buffer);
        result.vertexBufferMemory = std::move(vertices.memory);
        auto indices = _factory.uploadBuffer(
            std::as_bytes(std::span(mesh.indices)),
            vk::BufferUsageFlagBits::eIndexBuffer
        );
        result.indexBuffer = std::move(indices.buffer);
        result.indexBufferMemory = std::move(indices.memory);
        uint32_t firstIndex = 0;
        for (const auto indexCount : mesh.subMeshData) {
            result.subMeshesLayouts.push_back({ firstIndex, indexCount });
            firstIndex += indexCount;
        }
        const auto handle = result.handle;
        _meshes[handle] = std::move(result);
        return handle;
    }

    void VulkanResources::registerMaterial(
        tr::Resources::Handle<tr::Data::Material> handle,
        const tr::Data::Material& material
    ) {
        auto& shader = material.blendMode == tr::Data::BlendMode::Opaque
            ? _opaqueShader
            : _transparentShader;
        VulkanMaterial result{ .handle = handle, .shader = &shader };

        std::vector<uint8_t> uniformData;
        for (const auto& desc : shader.source->params) {
            const auto& value = material.get(desc.name, *shader.source);
            const auto info = shader.source->getParamTypeInfo(value);
            if (info.size == 0) continue;
            const uint32_t offset = (static_cast<uint32_t>(uniformData.size()) + info.alignment - 1) & ~(info.alignment - 1);
            uniformData.resize(offset + info.size);
            std::visit([&](const auto& parameter) {
                using T = std::decay_t<decltype(parameter)>;
                if constexpr (!std::is_same_v<T, tr::Resources::Handle<tr::Data::Texture>>) {
                    std::memcpy(uniformData.data() + offset, &parameter, sizeof(parameter));
                }
            }, value);
        }
        if (uniformData.empty()) uniformData.resize(4, 0);
        auto params = _factory.uploadBuffer(
            std::as_bytes(std::span(uniformData)),
            vk::BufferUsageFlagBits::eUniformBuffer
        );
        const auto uniformSize = params.size;
        result.paramsBuffer = std::move(params.buffer);
        result.paramsMemory = std::move(params.memory);
        result.descriptorSet = std::move(_context.device.allocateDescriptorSets({
            .descriptorPool = _materialDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*shader.descriptorSetLayout
        }).front());

        const size_t textureCount = std::ranges::count_if(
            shader.source->params,
            [&shader](const auto& desc) {
                return shader.source->isTextureParam(desc.defaultValue);
            }
        );
        std::vector<vk::DescriptorImageInfo> imageInfos;
        imageInfos.reserve(textureCount);
        std::vector<vk::WriteDescriptorSet> writes;
        writes.reserve(textureCount + 1);
        const vk::DescriptorBufferInfo bufferInfo{
            .buffer = *result.paramsBuffer,
            .offset = 0,
            .range = uniformSize
        };
        writes.push_back({
            .dstSet = *result.descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo
        });

        uint32_t binding = 1;
        for (const auto& desc : shader.source->params) {
            const auto& value = material.get(desc.name, *shader.source);
            if (!shader.source->isTextureParam(value)) continue;
            const auto textureHandle = std::get<tr::Resources::Handle<tr::Data::Texture>>(value);
            const VulkanTexture* texture = &_fallbackTexture;
            if (textureHandle.isValid()) {
                const auto found = _textures.find(textureHandle);
                if (found == _textures.end()) {
                    throw std::runtime_error("Can't use unregistered texture");
                }
                texture = &found->second;
            }
            imageInfos.push_back({
                .sampler = *texture->sampler,
                .imageView = *texture->image.view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
            writes.push_back({
                .dstSet = *result.descriptorSet,
                .dstBinding = binding++,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &imageInfos.back()
            });
        }
        _context.device.updateDescriptorSets(writes, {});
        _materials[handle] = std::move(result);
    }

    const VulkanMesh& VulkanResources::mesh(tr::Resources::Handle<tr::Data::Mesh> handle) const {
        const auto found = _meshes.find(handle);
        if (found == _meshes.end()) throw std::runtime_error("Can't use unregistered mesh");
        return found->second;
    }

    const VulkanMaterial& VulkanResources::material(
        tr::Resources::Handle<tr::Data::Material> handle
    ) const {
        const auto found = _materials.find(handle);
        if (found == _materials.end()) throw std::runtime_error("Can't use unregistered material");
        return found->second;
    }
}
