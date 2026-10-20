#include "post_processing_pass.h"

#include "post_processing.h"

namespace tr::Rendering {
    PostProcessingPass::PostProcessingPass(RenderingResources& resources) : RenderPass("PostProcessing", resources) {
        const std::array<float, 6> vertices{
            -1.0f, -1.0f,
            -1.0f,  3.0f,
             3.0f, -1.0f
        };

        _vertexBuffer = _resources.createBuffer({
            std::as_bytes(std::span(vertices)),
            BufferUsageFlags::VertexBuffer
        });

        const std::array<const BindingLayoutDesc, 1> bindings = {
            BindingLayoutDesc{
                .binding = 0,
                .type = BindingType::CombinedImageSampler,
                .stages = ShaderStages::Fragment,
            }
        };

        _bindingsLayout = resources.createBindingLayout(bindings);
        for (int i = 0; i < _bindingSets.size(); ++i) {
            _bindingSets[i] = _resources.createBindingSet(_bindingsLayout);
        }

        _pipelineLayout = resources.createPipelineLayout({
            .sets = { _bindingsLayout },
            .pushConstants = { { .stages = ShaderStages::Fragment, .offset = 0, .size = sizeof(PostProcessingData) } }
        });
    }

    PostProcessingPass::~PostProcessingPass() {
        _resources.destroyPipeline(_pipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
        for (auto set : _bindingSets) {
            _resources.destroyBindingSet(set);
        }
        _resources.destroyBindingLayout(_bindingsLayout);
        _resources.destroyBuffer(_vertexBuffer);
    }

    void PostProcessingPass::createPipeline(Format format) {
        _resources.destroyPipeline(_pipeline);
        EmbeddedShaders::PostProcessing shader;
        _pipeline = _resources.createPipeline(
            _pipelineLayout,
            PipelineDesc{
                .shader = shader,
                .vertexBinding = { 2 * sizeof(float) },
                .vertexAttributes = { { 0, Format::R32G32SFloat, 0 } },
                .rasterization = { .cullMode = CullMode::None },
                .colorFormat = format,
            }
        );
    }

    void PostProcessingPass::setup(RenderGraphBuilder& builder) {
        builder.read(source, TextureState::ShaderRead);
        builder.write(target, TextureState::ColorAttachment);
    }

    void PostProcessingPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
        cmd.beginRendering({
            .renderArea{
                .offset = glm::vec2(0, 0),
                .extent = frame.extent,
            },
            .colorAttachment{
                .target = context.texture(target),
                .load = false,
            },
        });
        auto set = _bindingSets[frame.slot];
        const std::array bindings{
            BindingDesc{ 0, ImageSamplerBinding{ context.texture(source) } },
        };
        _resources.updateBindingSet(set, bindings);

        cmd.bindPipeline(_pipeline);
        cmd.bindBindings(0, set);
        cmd.pushConstants({ ShaderStages::Fragment, 0, sizeof(PostProcessingData) }, &postProcessingData);
        cmd.bindVertexBuffer(_vertexBuffer);
        cmd.draw(3, 0);

        cmd.endRendering();
    }
}
