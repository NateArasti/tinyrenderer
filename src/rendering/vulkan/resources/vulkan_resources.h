#pragma once

#include <array>
#include <unordered_map>

#include "rendering_resources.h"
#include "vulkan_allocator.h"
#include "vulkan_descriptor_allocator.h"
#include "cubemap_converter.h"
#include "defines.h"
#include "../vulkan_rendering_structs.h"

namespace tr::Rendering::Vulkan {
    class VulkanResources : public RenderingResources {
    private:
        VulkanContext& _context;
        VulkanAllocator& _allocator;
        CubemapConverter _cubemapConverter;
        VulkanDescriptorAllocator _descriptorAllocator;

        uint32_t _nextHandle = 0;
        uint32_t _frameSlot = 0;

        Handle<GPUTexture> _fallbackTexture;
        GPUCubemap _fallbackCubemap;

        std::unordered_map<uint32_t, VulkanBuffer> _buffers;
        std::unordered_map<uint32_t, VulkanTexture> _textures;
        std::unordered_map<uint32_t, VulkanBindingLayout> _bindingLayouts;
        std::unordered_map<uint32_t, VulkanBindingSet> _bindingSets;
        std::unordered_map<uint32_t, VulkanPipelineLayout> _pipelineLayouts;
        std::unordered_map<uint32_t, VulkanPipeline> _pipelines;
        
        std::unordered_map<uint32_t, GPUMesh> _meshes;
        std::unordered_map<uint32_t, Material> _materials;
        std::unordered_map<uint32_t, Handle<GPUTexture>> _assetTextures;
        std::unordered_map<uint32_t, GPUCubemap> _cubemaps;
        std::array<std::vector<VulkanTexture>, MAX_FRAMES_IN_FLIGHT> _retiredTextures;

        void createFallbackTextures();
        Handle<GPUTexture> storeTexture(
            std::string_view name,
            VulkanImage image,
            bool repeat,
            bool comparison = false
        );

    public:
        VulkanResources(VulkanContext& context, VulkanAllocator& allocator);
        ~VulkanResources() override;

        void beginFrame(uint32_t slot);
        Handle<GPUTexture> importTexture(std::string_view name, vk::Image image, vk::ImageView view);
        void forgetTexture(Handle<GPUTexture> handle);

        const VulkanTexture& texture(Handle<GPUTexture> handle) const;
        const VulkanBuffer& buffer(Handle<GPUBuffer> handle) const;
        const VulkanPipeline& pipeline(Handle<Pipeline> handle) const;
        vk::PipelineLayout pipelineLayout(Handle<PipelineLayout> handle) const;
        vk::DescriptorSet bindingSet(Handle<BindingSet> handle) const;

        const GPUMesh& mesh(Handle<Mesh> handle) const override;
        const Material& material(Handle<Material> handle) const override;
        const GPUCubemap& cubemap(Handle<Cubemap> handle) const override;
        Handle<GPUTexture> texture(Handle<Texture> handle) const override;

        Handle<BindingLayout> createBindingLayout(std::span<const BindingLayoutDesc> bindings) override;
        void destroyBindingLayout(Handle<BindingLayout> handle) override;

        Handle<BindingSet> createBindingSet(Handle<BindingLayout> layout) override;
        void updateBindingSet(Handle<BindingSet> set, std::span<const BindingDesc> bindings) override;
        void destroyBindingSet(Handle<BindingSet> handle) override;

        Handle<PipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) override;
        void destroyPipelineLayout(Handle<PipelineLayout> handle) override;

        Handle<Pipeline> createPipeline(Handle<PipelineLayout> layout, const PipelineDesc& desc) override;
        void destroyPipeline(Handle<Pipeline> handle) override;

        Handle<GPUBuffer> createBuffer(const BufferDesc& desc) override;
        void updateBuffer(Handle<GPUBuffer> handle, std::span<const std::byte> bytes) override;
        void destroyBuffer(Handle<GPUBuffer> handle) override;

        Handle<GPUTexture> createTexture(const TextureDesc& desc) override;
        void destroyTexture(Handle<GPUTexture> handle) override;

        Handle<Mesh> uploadMesh(const Mesh& mesh) override;
        void destroyMesh(Handle<Mesh> handle) override;

        Handle<Texture> uploadTexture(const Texture& texture) override;
        void destroyTexture(Handle<Texture> handle) override;

        Handle<Cubemap> uploadCubemap(const Cubemap& cubemap) override;
        void destroyCubemap(Handle<Cubemap> handle) override;

        Handle<Material> uploadMaterial(const Material& material) override;
        void destroyMaterial(Handle<Material> handle) override;
    };
}
