#pragma once

#include <unordered_map>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "resource_factory.h"
#include "rendering_resources.h"
#include "vulkan_material.h"
#include "vulkan_mesh.h"
#include "vulkan_shader.h"
#include "vulkan_texture.h"

#include "cubemap.h"
#include "resources/vulkan_cubemap.h"
#include "resources/cubemap_converter.h"

namespace tr::Rendering::Vulkan {
    class VulkanResources : public tr::Rendering::RenderingResources {
    private:
        static constexpr uint32_t MAX_MATERIALS = 4096;
        static constexpr uint32_t MAX_TEXTURES_PER_MATERIAL = 8;

        VulkanContext& _context;
        ResourceFactory& _factory;

        CubemapConverter _cubemapConverter;

        const vk::raii::DescriptorSetLayout& _sceneDescriptorSetLayout;
        vk::Format _colorFormat;

        vk::raii::DescriptorPool _materialDescriptorPool = nullptr;
        uint32_t _generation = 0;
        uint32_t _cubemapIndex = 0;
        uint32_t _textureIndex = 0;
        uint32_t _meshIndex = 0;

        VulkanShader _opaqueShader;
        VulkanShader _transparentShader;
        VulkanTexture _fallbackTexture;
        std::unordered_map<tr::Resources::Handle<tr::Data::Cubemap>, VulkanCubemap> _cubemaps;
        std::unordered_map<tr::Resources::Handle<tr::Data::Texture>, VulkanTexture> _textures;
        std::unordered_map<tr::Resources::Handle<tr::Data::Mesh>, VulkanMesh> _meshes;
        std::unordered_map<tr::Resources::Handle<tr::Data::Material>, VulkanMaterial> _materials;

        void createDescriptorPool();
        void createFallbackTexture();
        VulkanShader createShader(const tr::Data::Shader& shader, tr::Data::BlendMode blendMode);

    public:
        VulkanResources(
            VulkanContext& context,
            ResourceFactory& factory,
            const vk::raii::DescriptorSetLayout& sceneDescriptorSetLayout,
            vk::Format colorFormat
        );

        VulkanResources(const VulkanResources&) = delete;
        VulkanResources& operator=(const VulkanResources&) = delete;
        VulkanResources(VulkanResources&&) = delete;
        VulkanResources& operator=(VulkanResources&&) = delete;

        void clear() override;
        void createBaseShaders(tr::Data::Shader& shader) override;
        void recreateBaseShaders(vk::Format colorFormat);
        tr::Resources::Handle<tr::Data::Texture> createTexture(const tr::Data::Texture& texture) override;
        tr::Resources::Handle<tr::Data::Mesh> createMesh(const tr::Data::Mesh& mesh) override;
        void registerMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material
        ) override;
        
        tr::Resources::Handle<tr::Data::Cubemap> createCubemap(const tr::Data::Cubemap& cubemap) override;
        void destroyCubemap(Resources::Handle<Data::Cubemap> handle) override;

        const VulkanMesh& mesh(tr::Resources::Handle<tr::Data::Mesh> handle) const;
        const VulkanMaterial& material(tr::Resources::Handle<tr::Data::Material> handle) const;
        const VulkanCubemap* cubemap(Resources::Handle<Data::Cubemap> handle) const;
    };
}
