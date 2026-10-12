#include "shadow_pass.h"

#include <array>

#include "shadow.h"

namespace tr::Rendering {
    ShadowPass::ShadowPass(RenderingResources& resources) : RenderPass("Shadow", resources) {
        _pipelineLayout = resources.createPipelineLayout({
            .pushConstants = { { ShaderStages::Vertex, 0, 2 * sizeof(glm::mat4) } },
        });
        Data::EmbeddedShaders::Shadow shader;
        _pipeline = resources.createPipeline(_pipelineLayout, {
            .shader = shader,
            .vertexBinding = { sizeof(Data::Mesh::Vertex) },
            .vertexAttributes = meshAttributes(),
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
        cmd.beginRendering({
            .renderArea = {
                .extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE },
            },
            .depthAttachment = { .target = context.texture(target) },
        });
        cmd.bindPipeline(_pipeline);
        for (const auto& draw : draws) {
            const auto& mesh = _resources.mesh(draw.mesh);
            const auto& submesh = mesh.subMeshes.at(draw.subMeshIndex);
            cmd.bindVertexBuffer(mesh.vertices);
            cmd.bindIndexBuffer(mesh.indices);
            cmd.setFrontFace(glm::determinant(glm::mat3(draw.modelMatrix)) < 0 ? Clockwise : CounterClockwise);
            const std::array matrices{
                frame.environmentData.lightViewProj,
                draw.modelMatrix,
            };
            cmd.pushConstants({ ShaderStages::Vertex, 0, sizeof(matrices) }, matrices.data());
            cmd.drawIndexed(submesh.indexCount, submesh.firstIndex, 0);
        }
        cmd.endRendering();
    }
}
