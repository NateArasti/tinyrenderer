#include "vulkan_utility.h"

#include <stdexcept>

namespace tr::Rendering::Vulkan {
    vk::Format toVulkanFormat(Format format) {
        switch (format) {
            case Undefined:
                return vk::Format::eUndefined;
            case R8G8B8A8UNorm:
                return vk::Format::eR8G8B8A8Unorm;
            case R8G8B8A8SRGB:
                return vk::Format::eR8G8B8A8Srgb;
            case B8G8R8A8UNorm:
                return vk::Format::eB8G8R8A8Unorm;
            case B8G8R8A8SRGB:
                return vk::Format::eB8G8R8A8Srgb;
            case D32SFloat:
                return vk::Format::eD32Sfloat;
            case D32SFloatS8:
                return vk::Format::eD32SfloatS8Uint;
            case D24UNormS8:
                return vk::Format::eD24UnormS8Uint;
            case R16G16B16A16SFloat:
                return vk::Format::eR16G16B16A16Sfloat;
            case R32G32B32A32SFloat:
                return vk::Format::eR32G32B32A32Sfloat;
            case R32G32B32SFloat:
                return vk::Format::eR32G32B32Sfloat;
            case R32G32SFloat:
                return vk::Format::eR32G32Sfloat;
            case R32SFloat:
                return vk::Format::eR32Sfloat;
        }
        throw std::invalid_argument("Unsupported format");
    }

    Format fromVulkanFormat(vk::Format format) {
        switch (format) {
            case vk::Format::eUndefined:
                return Format::Undefined;
            case vk::Format::eR8G8B8A8Unorm:
                return Format::R8G8B8A8UNorm;
            case vk::Format::eR8G8B8A8Srgb:
                return Format::R8G8B8A8SRGB;
            case vk::Format::eB8G8R8A8Unorm:
                return Format::B8G8R8A8UNorm;
            case vk::Format::eB8G8R8A8Srgb:
                return Format::B8G8R8A8SRGB;
            case vk::Format::eD32Sfloat:
                return Format::D32SFloat;
            case vk::Format::eD32SfloatS8Uint:
                return Format::D32SFloatS8;
            case vk::Format::eD24UnormS8Uint:
                return Format::D24UNormS8;
            case vk::Format::eR16G16B16A16Sfloat:
                return Format::R16G16B16A16SFloat;
            case vk::Format::eR32G32B32A32Sfloat:
                return Format::R32G32B32A32SFloat;
            case vk::Format::eR32G32B32Sfloat:
                return Format::R32G32B32SFloat;
            case vk::Format::eR32G32Sfloat:
                return Format::R32G32SFloat;
            case vk::Format::eR32Sfloat:
                return Format::R32SFloat;
        }
        throw std::invalid_argument("Unsupported Vulkan format");
    }

    vk::ShaderStageFlags toVulkanStages(ShaderStages stages) {
        vk::ShaderStageFlags result;
        if (static_cast<uint8_t>(stages) & static_cast<uint8_t>(ShaderStages::Vertex)) {
            result |= vk::ShaderStageFlagBits::eVertex;
        }
        if (static_cast<uint8_t>(stages) & static_cast<uint8_t>(ShaderStages::Fragment)) {
            result |= vk::ShaderStageFlagBits::eFragment;
        }
        return result;
    }

    vk::DescriptorType toVulkanBindingType(BindingType bindingType) {
        switch (bindingType) {
        case BindingType::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case BindingType::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case BindingType::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        }
        throw std::invalid_argument("Unsupported binding type");
    }

    vk::CompareOp toVulkanCompareOp(CompareOp compareOp) {
        switch (compareOp) {
            case CompareOp::Always:
                return vk::CompareOp::eAlways;
            case CompareOp::Less:
                return vk::CompareOp::eLess;
            case CompareOp::LessOrEqual:
                return vk::CompareOp::eLessOrEqual;
            case CompareOp::Equal:
                return vk::CompareOp::eEqual;
            case CompareOp::Greater:
                return vk::CompareOp::eGreater;
            case CompareOp::GreaterOrEqual:
                return vk::CompareOp::eGreaterOrEqual;
        }
        throw std::invalid_argument("Unsupported comparison operation");
    }

    vk::CullModeFlags toVulkanCullMode(Data::CullMode cullMode) {
        switch (cullMode) {
            case Data::CullMode::None:
                return vk::CullModeFlagBits::eNone;
            case Data::CullMode::Front:
                return vk::CullModeFlagBits::eFront;
            case Data::CullMode::Back:
                return vk::CullModeFlagBits::eBack;
            case Data::CullMode::Both:
                return vk::CullModeFlagBits::eFrontAndBack;
        }
        throw std::invalid_argument("Unsupported cull mode");
    }

    vk::FrontFace toVulkanFrontFace(WindingOrder winding) {
        return winding == WindingOrder::Clockwise ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;
    }

    vk::BufferUsageFlags toVulkanBufferUsage(BufferUsageFlags usage) {
        vk::BufferUsageFlags flags;
        if (usage & BufferUsageFlags::UniformBuffer) {
            flags |= vk::BufferUsageFlagBits::eUniformBuffer;
        }
        if (usage & BufferUsageFlags::VertexBuffer) {
            flags |= vk::BufferUsageFlagBits::eVertexBuffer;
        }
        if (usage & BufferUsageFlags::IndexBuffer) {
            flags |= vk::BufferUsageFlagBits::eIndexBuffer;
        }
        if (usage & BufferUsageFlags::TransferSrc) {
            flags |= vk::BufferUsageFlagBits::eTransferSrc;
        }
        if (usage & BufferUsageFlags::TransferDst) {
            flags |= vk::BufferUsageFlagBits::eTransferDst;
        }
        if (usage & BufferUsageFlags::Storage) {
            flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        }
        return flags;
    }

    bool isDepthFormat(Format format) {
        return format == Format::D32SFloat || format == Format::D32SFloatS8 || format == Format::D24UNormS8;
    }

    TextureAccess toVulkanTextureState(TextureState state) {
        switch (state) {
            case TextureState::Undefined:
                return {
                    vk::ImageLayout::eUndefined,
                    vk::PipelineStageFlagBits2::eTopOfPipe,
                    {}
                };
            case TextureState::ColorAttachment:
                return {
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
                };
            case TextureState::DepthAttachment:
                return {
                    vk::ImageLayout::eDepthAttachmentOptimal,
                    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                    vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
                };
            case TextureState::ShaderRead:
                return {
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader,
                    vk::AccessFlagBits2::eShaderRead
                };
            case TextureState::Present:
                return {
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::PipelineStageFlagBits2::eBottomOfPipe,
                    {}
                };
        }
        throw std::invalid_argument("Unsupported texture state");
    }
}
