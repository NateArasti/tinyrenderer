#pragma once

#include <span>

#include "rendering_structs.h"
#include "handle.h"
#include "material.h"
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "cubemap.h"

using namespace tr::Data;

namespace tr::Rendering {
    class RenderingResources {
    public:
        virtual ~RenderingResources() = default;

        virtual const GPUMesh& mesh(Handle<Mesh> handle) const = 0;
        virtual const Material& material(Handle<Material> handle) const = 0;
        virtual Handle<GPUTexture> texture(Handle<Texture> handle) const = 0;
        virtual const GPUCubemap& cubemap(Handle<Cubemap> handle) const = 0;

        virtual Handle<BindingLayout> createBindingLayout(std::span<const BindingLayoutDesc> bindings) = 0;
        virtual void destroyBindingLayout(Handle<BindingLayout> handle) = 0;

        virtual Handle<BindingSet> createBindingSet(Handle<BindingLayout> layout) = 0;
        virtual void updateBindingSet(Handle<BindingSet> set, std::span<const BindingDesc> bindings) = 0;
        virtual void destroyBindingSet(Handle<BindingSet> handle) = 0;

        virtual Handle<PipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual void destroyPipelineLayout(Handle<PipelineLayout> handle) = 0;

        virtual Handle<Pipeline> createPipeline(Handle<PipelineLayout> layout, const PipelineDesc& desc) = 0;
        virtual void destroyPipeline(Handle<Pipeline> handle) = 0;

        virtual Handle<GPUBuffer> createBuffer(const BufferDesc& bufferEntry) = 0;
        virtual void updateBuffer(Handle<GPUBuffer> handle, std::span<const std::byte> bytes) = 0;
        virtual void destroyBuffer(Handle<GPUBuffer> handle) = 0;

        virtual Handle<GPUTexture> createTexture(const TextureDesc& texDesc) = 0;
        virtual void destroyTexture(Handle<GPUTexture> handle) = 0;

        virtual Handle<Mesh> uploadMesh(const Mesh& mesh) = 0;
        virtual void destroyMesh(Handle<Mesh> handle) = 0;

        virtual Handle<Texture> uploadTexture(const Texture& texture) = 0;
        virtual void destroyTexture(Handle<Texture> handle) = 0;

        virtual Handle<Cubemap> uploadCubemap(const Cubemap& cubemap) = 0;
        virtual void destroyCubemap(Handle<Cubemap> handle) = 0;

        virtual Handle<Material> uploadMaterial(const Material& material) = 0;
        virtual void destroyMaterial(Handle<Material> handle) = 0;
    };
}
