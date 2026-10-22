#include "color_pass.h"

#include <algorithm>
#include <cstring>

namespace tr::Rendering {
    ColorPass::ColorPass(Data::Shader shader, RenderingResources& resources)
        : RenderPass("Color", resources), _shader(shader)
    {
        const std::array sceneBindings{
            BindingLayoutDesc{ 0, BindingType::UniformBuffer, ShaderStages::Vertex | ShaderStages::Fragment },
            BindingLayoutDesc{ 1, BindingType::CombinedImageSampler, ShaderStages::Fragment },
            BindingLayoutDesc{ 2, BindingType::StorageBuffer, ShaderStages::Vertex | ShaderStages::Fragment },
        };
        _sceneLayout = resources.createBindingLayout(sceneBindings);
        const std::array environmentBindings{
            BindingLayoutDesc{ 0, BindingType::UniformBuffer, ShaderStages::Vertex | ShaderStages::Fragment },
            BindingLayoutDesc{ 1, BindingType::CombinedImageSampler, ShaderStages::Fragment },
            BindingLayoutDesc{ 2, BindingType::CombinedImageSampler, ShaderStages::Fragment },
            BindingLayoutDesc{ 3, BindingType::CombinedImageSampler, ShaderStages::Fragment },
        };
        _environmentLayout = resources.createBindingLayout(environmentBindings);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _sceneSets[i] = resources.createBindingSet(_sceneLayout);
            _environmentSets[i] = resources.createBindingSet(_environmentLayout);
        }

        std::vector<BindingLayoutDesc> materialBindings{
            { 0, BindingType::StorageBuffer, ShaderStages::Vertex | ShaderStages::Fragment },
            { 1, BindingType::CombinedImageSampler, ShaderStages::Fragment, MAX_TEXTURES },
        };
        _materialLayout = resources.createBindingLayout(materialBindings);
        _materialSet = resources.createBindingSet(_materialLayout);

        _pipelineLayout = resources.createPipelineLayout({
            .sets = { _sceneLayout, _materialLayout, _environmentLayout },
        });
    }

    ColorPass::~ColorPass() {
        _resources.destroyPipeline(_opaquePipeline);
        _resources.destroyPipeline(_transparentPipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
        for (auto set : _sceneSets) {
            _resources.destroyBindingSet(set);
        }
        for (auto set : _environmentSets) {
            _resources.destroyBindingSet(set);
        }
        _resources.destroyBindingSet(_materialSet);
        _resources.destroyBindingLayout(_sceneLayout);
        _resources.destroyBindingLayout(_materialLayout);
        _resources.destroyBindingLayout(_environmentLayout);
    }

    void ColorPass::setMaterialTable(Handle<GPUBuffer> materials, std::span<const Handle<GPUTexture>> textures) {
        std::vector<BindingDesc> materialsBindings{
            BindingDesc{ 0, BufferBinding{ materials, BindingType::StorageBuffer } },
        };
        for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
            materialsBindings.push_back(BindingDesc{ 1, ImageSamplerBinding{ textures[i] }, i });
        }
        _resources.updateBindingSet(_materialSet, materialsBindings);
    }

    void ColorPass::createPipelines(Format colorFormat, Format depthFormat, uint32_t samples) {
        _resources.destroyPipeline(_opaquePipeline);
        _resources.destroyPipeline(_transparentPipeline);
        PipelineDesc desc{
            .shader = _shader,
            .vertexBinding = { sizeof(Data::Mesh::Vertex) },
            .vertexAttributes = {
                { 0, R32G32B32SFloat, offsetof(Data::Mesh::Vertex, position) },
                { 1, R32G32B32SFloat, offsetof(Data::Mesh::Vertex, normal) },
                { 2, R32G32SFloat, offsetof(Data::Mesh::Vertex, uv) },
                { 3, R32G32B32A32SFloat, offsetof(Data::Mesh::Vertex, color) }
            },
            .rasterization = { .cullMode = _shader.cullMode },
            .depth = {
                .testEnabled = true,
                .writeEnabled = true,
            },
            .multisample = static_cast<Multisample>(samples),
            .colorFormat = colorFormat,
            .depthFormat = depthFormat,
        };
        _opaquePipeline = _resources.createPipeline(_pipelineLayout, desc);
        desc.depth.writeEnabled = false;
        desc.blendEnabled = true;
        _transparentPipeline = _resources.createPipeline(_pipelineLayout, desc);
    }

    void ColorPass::setup(RenderGraphBuilder& builder) {
        builder.write(depth, TextureState::DepthAttachment);
        builder.write(resolve, TextureState::ColorAttachment);
        builder.read(shadow, TextureState::ShaderRead);
        builder.read(irradiance, TextureState::ShaderRead);
        builder.read(prefiltered, TextureState::ShaderRead);
        builder.read(brdf, TextureState::ShaderRead);

        builder.read(target, TextureState::ColorAttachment);
        builder.write(target, TextureState::ColorAttachment);
    }

    void ColorPass::updateBindings(RenderGraphContext& context, const FrameContext& frame) {
        const std::array sceneBindings{
            BindingDesc{ 0, BufferBinding{ frame.cameraDataBuffer, BindingType::UniformBuffer } },
            BindingDesc{ 1, ImageSamplerBinding{ context.texture(shadow) } },
            BindingDesc{ 2, BufferBinding{ frame.instanceBuffer, BindingType::StorageBuffer } },
        };
        const std::array environmentBindings{
            BindingDesc{ 0, BufferBinding{ frame.environmentDataBuffer, BindingType::UniformBuffer } },
            BindingDesc{ 1, ImageSamplerBinding{ context.texture(irradiance) } },
            BindingDesc{ 2, ImageSamplerBinding{ context.texture(prefiltered) } },
            BindingDesc{ 3, ImageSamplerBinding{ context.texture(brdf) } },
        };
        _resources.updateBindingSet(_sceneSets[frame.slot], sceneBindings);
        _resources.updateBindingSet(_environmentSets[frame.slot], environmentBindings);
    }

    void ColorPass::draw(
        CommandBuffer& cmd,
        const FrameContext& frame,
        std::span<const DrawCommand> draws,
        Handle<Pipeline> pipeline
    ) {
        cmd.bindPipeline(pipeline);
        cmd.bindBindings(0, _sceneSets[frame.slot]);
        cmd.bindBindings(1, _materialSet);
        cmd.bindBindings(2, _environmentSets[frame.slot]);
        for (const auto& draw : draws) {
            const auto& mesh = _resources.mesh(draw.mesh);
            const auto& submesh = mesh.subMeshes.at(draw.subMeshIndex);
            cmd.bindVertexBuffer(mesh.vertices);
            cmd.bindIndexBuffer(mesh.indices);
            cmd.setFrontFace(WindingOrder::CounterClockwise);
            cmd.drawIndexed(submesh.indexCount, submesh.firstIndex, 0, draw.firstInstance, draw.instanceCount);
        }
    }

    void ColorPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
        updateBindings(context, frame);
        cmd.beginRendering({
            .renderArea = { .extent = frame.extent },
            .colorAttachment = {
                .target = context.texture(target),
                .load = true,
                .resolve = context.texture(resolve),
            },
            .depthAttachment = { .target = context.texture(depth) },
        });
        draw(cmd, frame, opaque, _opaquePipeline);
        draw(cmd, frame, transparent, _transparentPipeline);
        cmd.endRendering();
    }
}
