#include "vulkan_resources.h"


#include "vulkan_utility.h"

namespace tr::Rendering::Vulkan {
    void VulkanResources::createFallbackTextures() {
        const std::array<uint8_t, 4> white{ 255, 255, 255, 255 };
        auto whiteImage = _allocator.uploadTexture(
            std::as_bytes(std::span(white)),
            1, 1,
            vk::Format::eR8G8B8A8Unorm,
            false,
            1,
            false
        );
        _fallbackTexture = storeTexture("Fallback texture", std::move(whiteImage), true);

        const std::array<uint8_t, 24> black{};
        for (auto* handle : { &_fallbackCubemap.skybox, &_fallbackCubemap.irradiance }) {
            auto blackImage = _allocator.uploadTexture(
                std::as_bytes(std::span(black)),
                1, 1,
                vk::Format::eR8G8B8A8Unorm,
                false,
                6,
                true
            );
            const auto name = handle == &_fallbackCubemap.skybox
                ? "Fallback skybox"
                : "Fallback irradiance";
            *handle = storeTexture(name, std::move(blackImage), false);
        }
    }

    Handle<GPUTexture> VulkanResources::storeTexture(
        std::string_view name,
        VulkanImage image,
        bool repeat,
        bool comparison
    ) {
        VulkanTexture record;
        record.image = *image.image;
        record.view = *image.view;
        record.aspectMask = image.aspectMask;
        record.mipLevels = image.mipLevels;
        record.arrayLayers = image.arrayLayers;
        record.allocation = std::move(image);
        auto mode = vk::SamplerAddressMode::eClampToEdge;
        if (comparison) {
            mode = vk::SamplerAddressMode::eClampToBorder;
        }
        else if (repeat) {
            mode = vk::SamplerAddressMode::eRepeat;
        }
        record.ownedSampler = vk::raii::Sampler(_context.device, {
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = mode,
            .addressModeV = mode,
            .addressModeW = mode,
            .anisotropyEnable = repeat,
            .maxAnisotropy = repeat ? _context.physicalDevice.getProperties().limits.maxSamplerAnisotropy : 1.0f,
            .compareEnable = comparison,
            .compareOp = vk::CompareOp::eLess,
            .maxLod = static_cast<float>(record.allocation.mipLevels - 1),
            .borderColor = vk::BorderColor::eFloatOpaqueWhite,
        });
        record.sampler = *record.ownedSampler;
        _context.setDebugName(record.image, name);
        _context.setDebugName(record.view, std::string(name) + " view");
        _context.setDebugName(record.sampler, std::string(name) + " sampler");
        Handle<GPUTexture> handle{ _nextHandle++ };
        _textures.emplace(handle.index, std::move(record));
        return handle;
    }

    Handle<GPUTexture> VulkanResources::importTexture(
        std::string_view name,
        vk::Image image,
        vk::ImageView view
    ) {
        Handle<GPUTexture> handle{ _nextHandle++ };
        VulkanTexture record;
        record.image = image;
        record.view = view;
        _context.setDebugName(record.image, name);
        _context.setDebugName(record.view, std::string(name) + " view");
        _textures.emplace(handle.index, std::move(record));
        return handle;
    }

    void VulkanResources::forgetTexture(Handle<GPUTexture> handle) {
        _textures.erase(handle.index);
    }

    Handle<GPUTexture> VulkanResources::createTexture(const TextureDesc& desc) {
        const bool depth = isDepthFormat(desc.format);
        vk::ImageUsageFlags usage =
            depth ? vk::ImageUsageFlagBits::eDepthStencilAttachment : vk::ImageUsageFlagBits::eColorAttachment;
        if (desc.sampled) {
            usage |= vk::ImageUsageFlagBits::eSampled;
        }
        auto image = _allocator.createImage({
            .format = toVulkanFormat(desc.format),
            .extent = { desc.width, desc.height, 1 },
            .samples = static_cast<vk::SampleCountFlagBits>(desc.samples),
            .usage = usage,
            .aspectMask = depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
        });
        return storeTexture(desc.name, std::move(image), false, desc.comparisonSampler);
    }

    void VulkanResources::destroyTexture(Handle<GPUTexture> handle) {
        if (!handle.isValid()) {
            return;
        }
        auto entry = _textures.extract(handle.index);
        if (!entry.empty()) {
            _retiredTextures[_frameSlot].push_back(std::move(entry.mapped()));
        }
    }

    const VulkanTexture& VulkanResources::texture(Handle<GPUTexture> handle) const {
        return _textures.at(handle.index);
    }
}
