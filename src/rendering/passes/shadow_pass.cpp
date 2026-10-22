#include "shadow_pass.h"

#include <array>

#include "shadow.h"

namespace tr::Rendering {
    ShadowPass::ShadowPass(RenderingResources& resources) : RenderPass("Shadow", resources) {
        const std::array bindings{
            BindingLayoutDesc{ 0, BindingType::StorageBuffer, ShaderStages::Vertex | ShaderStages::Fragment },
        };
        _bindingLayout = resources.createBindingLayout(bindings);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _sets[i] = resources.createBindingSet(_bindingLayout);
        }
        _pipelineLayout = resources.createPipelineLayout({
            .sets = { _bindingLayout },
            .pushConstants = { { ShaderStages::Vertex, 0, 2 * sizeof(glm::mat4) } },
        });
        Data::EmbeddedShaders::Shadow shader;
        _pipeline = resources.createPipeline(_pipelineLayout, {
            .shader = shader,
            .vertexBinding = { sizeof(Data::Mesh::Vertex) },
            .vertexAttributes = {
                {
                    0,
                    R32G32B32SFloat,
                    offsetof(Data::Mesh::Vertex, position)
                }
            },
            .rasterization = {
                .cullMode = CullMode::None,
                .depthBiasEnabled = true,
                .depthBiasConstant = 1.25f,
                .depthBiasSlope = 1.75f,
            },
            .depth = {
                .testEnabled = true,
                .writeEnabled = true,
            },
            .depthFormat = D32SFloat,
        });
    }

    ShadowPass::~ShadowPass() {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _resources.destroyBindingSet(_sets[i]);
        }
        _resources.destroyBindingLayout(_bindingLayout);
        _resources.destroyPipeline(_pipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
    }

    void ShadowPass::setup(RenderGraphBuilder& builder) {
        target = builder.create("Shadow map", {
            .width = SHADOW_MAP_SIZE,
            .height = SHADOW_MAP_SIZE,
            .format = D32SFloat,
            .sampled = true,
            .comparisonSampler = true,
        });
        builder.write(target, TextureState::DepthAttachment);
    }

    void ShadowPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
        const std::array bindings{
            BindingDesc{ 0, BufferBinding{ frame.instanceBuffer, BindingType::StorageBuffer } },
        };
        _resources.updateBindingSet(_sets[frame.slot], bindings);
        cmd.beginRendering({
            .renderArea = {
                .extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE },
            },
            .depthAttachment = { .target = context.texture(target) },
        });
        cmd.bindPipeline(_pipeline);
        cmd.bindBindings(0, _sets[frame.slot]);
        for (const auto& draw : draws) {
            const auto& mesh = _resources.mesh(draw.mesh);
            const auto& submesh = mesh.subMeshes.at(draw.subMeshIndex);
            cmd.bindVertexBuffer(mesh.vertices);
            cmd.bindIndexBuffer(mesh.indices);
            cmd.setFrontFace(WindingOrder::CounterClockwise);
            cmd.pushConstants(
                PushConstantRange{
                    ShaderStages::Vertex,
                    0,
                    sizeof(frame.environmentData.lightViewProj)
                },
                &frame.environmentData.lightViewProj
            );
            cmd.drawIndexed(submesh.indexCount, submesh.firstIndex, 0, draw.firstInstance, draw.instanceCount);
        }
        cmd.endRendering();
    }
}
