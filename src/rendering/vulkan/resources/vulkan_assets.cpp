#include "vulkan_resources.h"

#include <stdexcept>

namespace tr::Rendering::Vulkan {
    Handle<Material> VulkanResources::uploadMaterial(const Material& material) {
        Handle<Material> handle{ _nextHandle++ };
        _materials.emplace(handle.index, material);
        return handle;
    }

    void VulkanResources::destroyMaterial(Handle<Material> handle) {
        _materials.erase(handle.index);
    }

    Handle<Mesh> VulkanResources::uploadMesh(const Mesh& mesh) {
        GPUMesh buffers;
        buffers.vertices = { _nextHandle++ };
        buffers.indices = { _nextHandle++ };
        _buffers.emplace(
            buffers.vertices.index,
            _allocator.uploadBuffer(std::as_bytes(std::span(mesh.vertices)), vk::BufferUsageFlagBits::eVertexBuffer)
        );
        _buffers.emplace(
            buffers.indices.index,
            _allocator.uploadBuffer(std::as_bytes(std::span(mesh.indices)), vk::BufferUsageFlagBits::eIndexBuffer)
        );
        uint32_t first = 0;
        for (auto count : mesh.subMeshData) {
            buffers.subMeshes.push_back({ first, count });
            first += count;
        }
        Handle<Mesh> handle{ _nextHandle++ };
        _meshes.emplace(handle.index, std::move(buffers));
        return handle;
    }

    void VulkanResources::destroyMesh(Handle<Mesh> handle) {
        auto entry = _meshes.extract(handle.index);
        if (entry.empty()) {
            return;
        }
        _context.device.waitIdle();
        destroyBuffer(entry.mapped().vertices);
        destroyBuffer(entry.mapped().indices);
    }

    Handle<Texture> VulkanResources::uploadTexture(const Texture& texture) {
        const auto format = texture.colorSpace == TextureColorSpace::SRGB
            ? vk::Format::eR8G8B8A8Srgb
            : vk::Format::eR8G8B8A8Unorm;
        auto allocation = _allocator.uploadTexture(
            std::as_bytes(std::span(texture.pixels)),
            texture.width,
            texture.height,
            format,
            true,
            1,
            false
        );
        const auto name = texture.name.empty() ? "Texture" : texture.name;
        const auto image = storeTexture(name, std::move(allocation), true);
        Handle<Texture> handle{ _nextHandle++ };
        _assetTextures.emplace(handle.index, image);
        return handle;
    }

    void VulkanResources::destroyTexture(Handle<Texture> handle) {
        auto entry = _assetTextures.extract(handle.index);
        if (entry.empty()) {
            return;
        }
        _context.device.waitIdle();
        destroyTexture(entry.mapped());
    }

    Handle<Cubemap> VulkanResources::uploadCubemap(const Cubemap& source) {
        if (!source.width || !source.height || source.pixels.size() != size_t(source.width) * source.height * 4) {
            throw std::invalid_argument("Cubemap source must contain RGBA float pixels");
        }
        auto image = _allocator.uploadTexture(
            std::as_bytes(std::span(source.pixels)),
            source.width,
            source.height,
            vk::Format::eR32G32B32A32Sfloat,
            false,
            1,
            false
        );
        auto skybox = _cubemapConverter.convertEquirectangular(image);
        auto irradiance = _cubemapConverter.convolveIrradiance(skybox);
        auto prefilter = _cubemapConverter.prefilterEnvironment(skybox);
        auto brdf = _cubemapConverter.integrateBRDF();
        const auto name = source.name.empty() ? "Cubemap" : source.name;
        const GPUCubemap cubemap{
            .skybox = storeTexture(name + " skybox", std::move(skybox), false),
            .irradiance = storeTexture(name + " irradiance", std::move(irradiance), false),
            .prefiltered = storeTexture(name + " prefilter", std::move(prefilter), false),
            .brdf = storeTexture(name + " brdf", std::move(brdf), false),
        };
        Handle<Cubemap> handle{ _nextHandle++ };
        _cubemaps.emplace(handle.index, cubemap);
        return handle;
    }

    void VulkanResources::destroyCubemap(Handle<Cubemap> handle) {
        auto entry = _cubemaps.extract(handle.index);
        if (entry.empty()) {
            return;
        }
        _context.device.waitIdle();
        destroyTexture(entry.mapped().skybox);
        destroyTexture(entry.mapped().irradiance);
        destroyTexture(entry.mapped().prefiltered);
        destroyTexture(entry.mapped().brdf);
    }

    const GPUCubemap& VulkanResources::cubemap(Handle<Cubemap> handle) const {
        auto found = _cubemaps.find(handle.index);
        return found == _cubemaps.end() ? _fallbackCubemap : found->second;
    }

    Handle<GPUTexture> VulkanResources::texture(Handle<Texture> handle) const {
        return handle.isValid() ? _assetTextures.at(handle.index) : _fallbackTexture;
    }

    const GPUMesh& VulkanResources::mesh(Handle<Mesh> handle) const {
        return _meshes.at(handle.index);
    }

    const Material& VulkanResources::material(Handle<Material> handle) const {
        return _materials.at(handle.index);
    }
}
