#pragma once

#include <span>

#include <glm/glm.hpp>

#include "handle.h"
#include "shader.h"
#include "texture.h"
#include "material.h"
#include "mesh.h"
#include "camera.h"

namespace tr::Rendering {
    struct DrawCommand {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        tr::Resources::Handle<tr::Data::Mesh> mesh;
        std::span<const tr::Resources::Handle<tr::Data::Material>> materials;
    };

    class RHI {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        RHI(const RHI&) = delete;
        RHI& operator=(const RHI&) = delete;
        RHI(RHI&&) = delete;
        RHI& operator=(RHI&&) = delete;

        virtual void clearResources() = 0;
        virtual void resize(uint32_t width, uint32_t height) = 0;

        virtual void createShader(
            tr::Resources::Handle<tr::Data::Shader> handle,
            const tr::Data::Shader& shader) = 0;
        virtual void createTexture(
            tr::Resources::Handle<tr::Data::Texture> handle,
            const tr::Data::Texture& texture) = 0;
        virtual void createMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material,
            const tr::Data::Shader& shader) = 0;
        virtual void createMesh(
            tr::Resources::Handle<tr::Data::Mesh> handle,
            const tr::Data::Mesh& mesh) = 0;

        virtual void startFrame(const tr::Data::Camera& camera) = 0;
        virtual void draw(const DrawCommand& command) = 0;
        virtual void endFrame() = 0;
    };
}
