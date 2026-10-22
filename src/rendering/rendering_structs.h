#pragma once

#include "handle.h"
#include "shader.h"
#include "mesh.h"

#include <variant>
#include <string>
#include <vector>
#include <span>

namespace tr::Rendering {
    using Data::Handle;
    using Data::Shader;

    enum Format {
        Undefined,
        R8G8B8A8UNorm,
        R8G8B8A8SRGB,
        B8G8R8A8UNorm,
        B8G8R8A8SRGB,
        D32SFloat,
        D32SFloatS8,
        D24UNormS8,
        R16G16B16A16SFloat,
        R32G32B32A32SFloat,
        R32G32B32SFloat,
        R32G32SFloat,
        R32SFloat,
    };

    enum class TextureState {
        Undefined,
        ColorAttachment,
        DepthAttachment,
        ShaderRead,
        Present
    };

    struct TextureDesc {
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
        Format format = Format::Undefined;
        uint32_t samples = 1;
        bool sampled = false;
        bool comparisonSampler = false;
        bool operator==(const TextureDesc&) const = default;
    };

    struct GPUTexture {};

    enum BufferUsageFlags : uint8_t {
        UniformBuffer = 1,
        VertexBuffer = 2,
        IndexBuffer = 4,
        TransferSrc = 8,
        TransferDst = 16,
        Storage = 32,
    };

    struct BufferDesc {
        std::span<const std::byte> data;
        BufferUsageFlags usageFlags;
    };

    struct GPUBuffer {};

    enum class BindingType {
        UniformBuffer,
        StorageBuffer,
        CombinedImageSampler
    };

    enum class ShaderStages : uint8_t {
        Vertex = 1,
        Fragment = 2,
    };

    constexpr ShaderStages operator|(ShaderStages a, ShaderStages b) {
        return static_cast<ShaderStages>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    struct BindingLayoutDesc {
        uint32_t binding;
        BindingType type;
        ShaderStages stages;
        uint32_t count = 1;
    };

    struct BindingLayout {};

    struct BufferBinding {
        Handle<GPUBuffer> buffer;
        BindingType type;
    };

    struct ImageSamplerBinding {
        Handle<GPUTexture> texture;
    };

    struct BindingDesc {
        uint32_t binding;
        std::variant<BufferBinding, ImageSamplerBinding> resource;
        uint32_t arrayElement;
    };

    struct BindingSet {};

    struct PushConstantRange {
        ShaderStages stages;
        uint32_t offset;
        uint32_t size;
    };

    struct PipelineLayoutDesc {
        std::vector<Handle<BindingLayout>> sets;
        std::vector<PushConstantRange> pushConstants;
    };

    struct PipelineLayout {};

    enum WindingOrder { Clockwise, CounterClockwise };

    enum CompareOp {
        Always,
        Less,
        LessOrEqual,
        Equal,
        Greater,
        GreaterOrEqual,
    };

    enum Multisample {
        x1 = 1,
        x2 = 2,
        x4 = 4,
        x8 = 8,
    };

    struct VertexBindingDesc {
        uint32_t size;
    };

    struct VertexAttributeDesc {
        uint32_t location;
        Format format;
        uint32_t offset;
    };
    
    struct RasterizationDesc {
        tr::Data::CullMode cullMode = tr::Data::CullMode::Back;
        WindingOrder frontFace = WindingOrder::CounterClockwise;

        bool depthBiasEnabled = false;
        float depthBiasConstant = 0;
        float depthBiasSlope = 0;
    };

    struct DepthStateDesc {
        bool testEnabled = false;
        bool writeEnabled = false;
        CompareOp compareOp = CompareOp::Less;
    };

    struct PipelineDesc {
        const Shader& shader;

        VertexBindingDesc vertexBinding;
        std::vector<VertexAttributeDesc> vertexAttributes;

        RasterizationDesc rasterization;
        DepthStateDesc depth;

        Multisample multisample = Multisample::x1;
        Format colorFormat = Format::Undefined;
        Format depthFormat = Format::Undefined;
        bool blendEnabled = false;
    };

    struct Pipeline {};

    struct ColorAttachmentDesc {
        Handle<GPUTexture> target;
        glm::vec4 clearValue{ 0.0f };
        bool load = false;
        Handle<GPUTexture> resolve;
    };

    struct DepthAttachmentDesc {
        Handle<GPUTexture> target;
        bool load = false;
    };

    struct RenderArea {
        glm::vec2 offset{ 0.0f };
        glm::vec2 extent;
    };

    struct RenderingDesc {
        RenderArea renderArea;
        ColorAttachmentDesc colorAttachment;
        DepthAttachmentDesc depthAttachment;
    };
    
    struct GPUMesh {
        Handle<GPUBuffer> vertices;
        Handle<GPUBuffer> indices;
        struct Submesh {
            uint32_t firstIndex;
            uint32_t indexCount;
        };
        std::vector<Submesh> subMeshes;
    };
    
    struct GPUCubemap {
        Handle<GPUTexture> skybox;
        Handle<GPUTexture> irradiance;
        Handle<GPUTexture> prefiltered;
        Handle<GPUTexture> brdf;
    };
}
