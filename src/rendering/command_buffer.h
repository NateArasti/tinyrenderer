#pragma once

#include <string_view>

#include "handle.h"
#include "rendering_structs.h"

using namespace tr::Data;

namespace tr::Rendering {
    class CommandBuffer {
    public:
        virtual ~CommandBuffer() = default;

        virtual void beginDebugLabel(
            std::string_view name,
            const glm::vec4& color = glm::vec4(0.25f, 0.5f, 0.9f, 1.0f)
        ) = 0;
        virtual void endDebugLabel() = 0;

        virtual void beginRendering(RenderingDesc desc) = 0;
        virtual void endRendering() = 0;

        virtual void transition(Handle<GPUTexture> texture, TextureState current, TextureState target) = 0;

        virtual void bindPipeline(Handle<Pipeline> pipeline) = 0;
        virtual void setFrontFace(WindingOrder winding) = 0;
        virtual void bindBindings(uint32_t setIndex, Handle<BindingSet> set) = 0;
        virtual void pushConstants(PushConstantRange pushConstantRange, const void* bytes) = 0;
        virtual void bindVertexBuffer(Handle<GPUBuffer> buffer) = 0;
        virtual void bindIndexBuffer(Handle<GPUBuffer> buffer) = 0;

        virtual void draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
        virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) = 0;
    };
}
