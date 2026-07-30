#pragma once

#include <span>

#include <glm/glm.hpp>
#include <imgui.h>

#include "handle.h"
#include "shader.h"
#include "texture.h"
#include "material.h"
#include "mesh.h"
#include "camera.h"
#include "scene_data.h"

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

        virtual std::string getDeviceName() const = 0;
        virtual void resize(uint32_t width, uint32_t height) = 0;

        virtual void createShadowShader(const tr::Data::Shader& shader) = 0;
        virtual void createBaseShaders(tr::Data::Shader& referenceShader) = 0;

        virtual tr::Resources::Handle<tr::Data::Mesh> createMesh(const tr::Data::Mesh& mesh) = 0;
        virtual tr::Resources::Handle<tr::Data::Texture> createTexture(const tr::Data::Texture& texture) = 0;

        virtual void registerMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material
        ) = 0;

        virtual void clearResources() = 0;

        virtual void startFrame(const tr::Rendering::SceneData& sceneData) = 0;
        virtual void startShadowPass() = 0;
        virtual void drawShadows(const DrawCommand& command) = 0;
        virtual void endShadowPass() = 0;
        virtual void startColorPass() = 0;
        virtual void draw(const DrawCommand& command) = 0;
        virtual void endColorPass() = 0;
        virtual void prepareUI() = 0;
        virtual void drawUI(ImDrawData* drawData) = 0;
        virtual void endFrame() = 0;
    };
}
