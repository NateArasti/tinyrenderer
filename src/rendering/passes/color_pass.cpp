#include "color_pass.h"

#include <algorithm>
#include <cstring>

namespace tr::Rendering {
    namespace {
        std::vector<std::byte> packMaterialParameters(const Material& material, const Shader& shader) {
            std::vector<std::byte> bytes;
            for (const auto& desc : shader.params) {
                const auto& value = material.get(desc.name, shader);
                const auto info = shader.getParamTypeInfo(value);
                if (!info.size) {
                    continue;
                }
                const auto offset = (bytes.size() + info.alignment - 1) & ~(size_t(info.alignment) - 1);
                bytes.resize(offset + info.size);
                std::visit(
                    [&](const auto& parameter) {
                        using T = std::decay_t<decltype(parameter)>;
                        if constexpr (!std::is_same_v<T, Handle<Texture>>) {
                            std::memcpy(bytes.data() + offset, &parameter, sizeof(parameter));
                        }
                    },
                    value);
            }
            bytes.resize(std::max(size_t(16), (bytes.size() + 15) & ~size_t(15)));
            return bytes;
        }
    }

    ColorPass::ColorPass(RenderingResources& resources) : RenderPass("Color", resources) {
        const auto stages = ShaderStages::Vertex | ShaderStages::Fragment;
        const std::array cameraBindings{
            BindingLayoutDesc{ 0, BindingType::UniformBuffer, stages },
            BindingLayoutDesc{ 1, BindingType::CombinedImageSampler, ShaderStages::Fragment },
        };
        _cameraLayout = resources.createBindingLayout(cameraBindings);
        _environmentLayout = resources.createBindingLayout(cameraBindings);
        std::vector<BindingLayoutDesc> materialBindings{ { 0, BindingType::UniformBuffer, stages } };
        for (const auto& param : _shader.params) {
            if (_shader.isTextureParam(param.defaultValue)) {
                materialBindings.push_back({
                    .binding = static_cast<uint32_t>(materialBindings.size()),
                    .type = BindingType::CombinedImageSampler,
                    .stages = ShaderStages::Fragment
                });
            }
        }
        _materialLayout = resources.createBindingLayout(materialBindings);
        _pipelineLayout = resources.createPipelineLayout({
            .sets = { _cameraLayout, _materialLayout, _environmentLayout },
            .pushConstants = { { ShaderStages::Vertex, 0, sizeof(glm::mat4) } },
        });
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _cameraSets[i] = resources.createBindingSet(_cameraLayout);
            _environmentSets[i] = resources.createBindingSet(_environmentLayout);
        }
    }

    ColorPass::~ColorPass() {
        clearMaterials();
        _resources.destroyPipeline(_opaquePipeline);
        _resources.destroyPipeline(_transparentPipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
        for (auto set : _cameraSets) {
            _resources.destroyBindingSet(set);
        }
        for (auto set : _environmentSets) {
            _resources.destroyBindingSet(set);
        }
        _resources.destroyBindingLayout(_cameraLayout);
        _resources.destroyBindingLayout(_environmentLayout);
        _resources.destroyBindingLayout(_materialLayout);
    }

    void ColorPass::clearMaterials() {
        for (const auto& [handle, material] : _materials) {
            _resources.destroyBindingSet(material.set);
            _resources.destroyBuffer(material.buffer);
        }
        _materials.clear();
    }

    Handle<BindingSet> ColorPass::materialBindings(Handle<Material> handle) {
        if (auto found = _materials.find(handle); found != _materials.end()) {
            return found->second.set;
        }

        const auto& material = _resources.material(handle);
        const auto bytes = packMaterialParameters(material, _shader);
        MaterialBindings bindings{
            .set = _resources.createBindingSet(_materialLayout),
            .buffer = _resources.createBuffer({ bytes, BufferUsageFlags::UniformBuffer }),
        };
        std::vector<BindingDesc> descriptors{ { 0, BufferBinding{ bindings.buffer } } };
        for (const auto& desc : _shader.params) {
            const auto& value = material.get(desc.name, _shader);
            if (const auto* texture = std::get_if<Handle<Texture>>(&value)) {
                descriptors.push_back({
                    static_cast<uint32_t>(descriptors.size()),
                    ImageSamplerBinding{ _resources.texture(*texture) },
                });
            }
        }
        _resources.updateBindingSet(bindings.set, descriptors);
        _materials.emplace(handle, bindings);
        return bindings.set;
    }

    void ColorPass::createPipelines(Format colorFormat, Format depthFormat, uint32_t samples) {
        _resources.destroyPipeline(_opaquePipeline);
        _resources.destroyPipeline(_transparentPipeline);
        PipelineDesc desc{
            .shader = _shader,
            .vertexBinding = { sizeof(Data::Mesh::Vertex) },
            .vertexAttributes = meshAttributes(),
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

        builder.read(target, TextureState::ColorAttachment);
        builder.write(target, TextureState::ColorAttachment);
    }

    void ColorPass::updateBindings(RenderGraphContext& context, const FrameContext& frame) {
        const std::array cameraBindings{
            BindingDesc{ 0, BufferBinding{ frame.cameraDataBuffer } },
            BindingDesc{ 1, ImageSamplerBinding{ context.texture(shadow) } },
        };
        const std::array environmentBindings{
            BindingDesc{ 0, BufferBinding{ frame.environmentDataBuffer } },
            BindingDesc{ 1, ImageSamplerBinding{ context.texture(irradiance) } },
        };
        _resources.updateBindingSet(_cameraSets[frame.slot], cameraBindings);
        _resources.updateBindingSet(_environmentSets[frame.slot], environmentBindings);
    }

    void ColorPass::draw(
        CommandBuffer& cmd,
        const FrameContext& frame,
        std::span<const DrawCommand> draws,
        Handle<Pipeline> pipeline
    ) {
        cmd.bindPipeline(pipeline);
        cmd.bindBindings(0, _cameraSets[frame.slot]);
        cmd.bindBindings(2, _environmentSets[frame.slot]);
        for (const auto& draw : draws) {
            const auto& mesh = _resources.mesh(draw.mesh);
            const auto& submesh = mesh.subMeshes.at(draw.subMeshIndex);
            cmd.bindVertexBuffer(mesh.vertices);
            cmd.bindIndexBuffer(mesh.indices);
            cmd.setFrontFace(
                glm::determinant(glm::mat3(draw.modelMatrix)) < 0.0f
                ? WindingOrder::Clockwise
                : WindingOrder::CounterClockwise
            );
            cmd.bindBindings(1, materialBindings(draw.material));
            cmd.pushConstants({ ShaderStages::Vertex, 0, sizeof(glm::mat4) }, &draw.modelMatrix);
            cmd.drawIndexed(submesh.indexCount, submesh.firstIndex, 0);
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
