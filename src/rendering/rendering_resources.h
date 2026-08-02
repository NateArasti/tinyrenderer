#pragma once

#include "handle.h"
#include "material.h"
#include "mesh.h"
#include "shader.h"
#include "texture.h"

namespace tr::Rendering {
    class RenderingResources {
    public:
        virtual ~RenderingResources() = default;

        virtual void createBaseShaders(tr::Data::Shader& shader) = 0;
        virtual tr::Resources::Handle<tr::Data::Mesh> createMesh(const tr::Data::Mesh& mesh) = 0;
        virtual tr::Resources::Handle<tr::Data::Texture> createTexture(const tr::Data::Texture& texture) = 0;
        virtual void registerMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material
        ) = 0;
        virtual void clear() = 0;
    };
}
