#include "skybox_pass.h"

#include "skybox.h"

#include <array>
#include <span>

namespace tr::Rendering {
    SkyboxPass::SkyboxPass(RenderingResources& resources) : RenderPass("Skybox", resources) {
        const std::array<float, 6> vertices{
            -1.0f, -1.0f,
            -1.0f,  3.0f,
             3.0f, -1.0f
        };

        _vertexBuffer = _resources.createBuffer({
            std::as_bytes(std::span(vertices)),
            BufferUsageFlags::VertexBuffer
        });

        const std::array<const BindingLayoutDesc, 3> bindings = {
            BindingLayoutDesc{
                .binding = 0,
                .type = BindingType::UniformBuffer,
                .stages = ShaderStages::Fragment
            },
            BindingLayoutDesc{
                .binding = 1,
                .type = BindingType::UniformBuffer,
                .stages = ShaderStages::Fragment
            },
            BindingLayoutDesc{
                .binding = 2,
                .type = BindingType::CombinedImageSampler,
                .stages = ShaderStages::Fragment,
            }
        };

        _bindingsLayout = resources.createBindingLayout(bindings);
        for (int i = 0; i < _bindingSets.size(); ++i) {
            _bindingSets[i] = _resources.createBindingSet(_bindingsLayout);
        }

        _pipelineLayout = resources.createPipelineLayout({ .sets = { _bindingsLayout } });
    }

    SkyboxPass::~SkyboxPass() {
        _resources.destroyPipeline(_pipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
        for (auto set : _bindingSets) {
            _resources.destroyBindingSet(set);
        }
        _resources.destroyBindingLayout(_bindingsLayout);
        _resources.destroyBuffer(_vertexBuffer);
    }

    void SkyboxPass::createPipeline(Format format, uint32_t samples) {
        _resources.destroyPipeline(_pipeline);
        EmbeddedShaders::Skybox shader;
        _pipeline = _resources.createPipeline(
            _pipelineLayout,
            PipelineDesc{
                .shader = shader,
                .vertexBinding = { 2 * sizeof(float) },
                .vertexAttributes = { { 0, Format::R32G32SFloat, 0 } },
                .rasterization = { .cullMode = CullMode::None },
                .multisample = static_cast<Multisample>(samples),
                .colorFormat = format,
            }
        );
    }

    void SkyboxPass::setup(RenderGraphBuilder& builder) {
        builder.read(cubemap, TextureState::ShaderRead);
        builder.write(target, TextureState::ColorAttachment);
    }

    void SkyboxPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
        cmd.beginRendering({
            .renderArea{
                .offset = glm::vec2(0, 0),
                .extent = frame.extent,
            },
            .colorAttachment{
                .target = context.texture(target),
                .clearValue = glm::vec4(frame.environmentData.clearColor, 1.0f),
            },
        });

        if (frame.environmentData.hasSkybox > 0.5) {
            auto set = _bindingSets[frame.slot];
            const std::array bindings{
                BindingDesc{ 0, BufferBinding{ frame.environmentDataBuffer } },
                BindingDesc{ 1, BufferBinding{ frame.cameraDataBuffer } },
                BindingDesc{ 2, ImageSamplerBinding{ context.texture(cubemap) } },
            };
            _resources.updateBindingSet(set, bindings);

            cmd.bindPipeline(_pipeline);
            cmd.bindBindings(0, set);
            cmd.bindVertexBuffer(_vertexBuffer);
            cmd.draw(3, 0);
        }

        cmd.endRendering();
    }
}
