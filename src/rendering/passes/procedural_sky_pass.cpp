#include "procedural_sky_pass.h"

#include "procedural_sky.h"

#include <array>
#include <span>

namespace tr::Rendering {
    ProceduralSkyPass::ProceduralSkyPass(RenderingResources& resources) : RenderPass("Procedural Sky", resources) {
        const std::array<float, 6> vertices{
            -1.0f, -1.0f,
            -1.0f,  3.0f,
             3.0f, -1.0f
        };

        _vertexBuffer = _resources.createBuffer({
            std::as_bytes(std::span(vertices)),
            BufferUsageFlags::VertexBuffer
        });

        const std::array bindings = {
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
                .type = BindingType::UniformBuffer,
                .stages = ShaderStages::Fragment
            },
        };

        _bindingsLayout = resources.createBindingLayout(bindings);
        for (int i = 0; i < _bindingSets.size(); ++i) {
            _bindingSets[i] = _resources.createBindingSet(_bindingsLayout);
            _dataBuffers[i] = _resources.createBuffer({
                .data = std::as_bytes(std::span(&proceduralSkyData, 1)),
                .usageFlags = BufferUsageFlags::UniformBuffer,
            });
        }

        _pipelineLayout = resources.createPipelineLayout({ .sets = { _bindingsLayout } });
    }

    ProceduralSkyPass::~ProceduralSkyPass() {
        _resources.destroyPipeline(_pipeline);
        _resources.destroyPipelineLayout(_pipelineLayout);
        for (auto set : _bindingSets) {
            _resources.destroyBindingSet(set);
        }
        for (auto buffer : _dataBuffers) {
            _resources.destroyBuffer(buffer);
        }
        _resources.destroyBindingLayout(_bindingsLayout);
        _resources.destroyBuffer(_vertexBuffer);
    }

    void ProceduralSkyPass::createPipeline(Format format, uint32_t samples) {
        _resources.destroyPipeline(_pipeline);
        EmbeddedShaders::ProceduralSky shader;
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

    void ProceduralSkyPass::setup(RenderGraphBuilder& builder) {
        builder.write(target, TextureState::ColorAttachment);
    }

    void ProceduralSkyPass::execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) {
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
        
        auto set = _bindingSets[frame.slot];
        auto dataBuffer = _dataBuffers[frame.slot];
        _resources.updateBuffer(dataBuffer, std::as_bytes(std::span(&proceduralSkyData, 1)));
        const std::array bindings{
            BindingDesc{ 0, BufferBinding{ frame.environmentDataBuffer, BindingType::UniformBuffer } },
            BindingDesc{ 1, BufferBinding{ frame.cameraDataBuffer, BindingType::UniformBuffer } },
            BindingDesc{ 2, BufferBinding{ dataBuffer, BindingType::UniformBuffer } },
        };
        _resources.updateBindingSet(set, bindings);

        cmd.bindPipeline(_pipeline);
        cmd.bindBindings(0, set);
        cmd.bindVertexBuffer(_vertexBuffer);
        cmd.draw(3, 0);

        cmd.endRendering();
    }
}
