#pragma once

#include <vector>
#include <memory>

#include "rhi.h"
#include "pool.h"
#include "shader.h"
#include "texture.h"
#include "material.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"

namespace tr::Rendering {
    class Renderer {
    private:
        Rendering::RHI* _renderingInterface;
        std::vector<std::pair<DrawCommand, float>> _transparentDrawQueue;
        
        tr::Resources::Pool<tr::Data::Shader> _shadersPool;
        tr::Resources::Pool<tr::Data::Texture> _texturesPool;
        tr::Resources::Pool<tr::Data::Material> _materialsPool;
        tr::Resources::Pool<tr::Data::Mesh> _meshesPool;
        
    public:
        explicit Renderer(Rendering::RHI& rhi);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void clearState();
        tr::Resources::Handle<tr::Data::Shader> upload(std::unique_ptr<tr::Data::Shader> shader);
        tr::Resources::Handle<tr::Data::Texture> upload(std::unique_ptr<tr::Data::Texture> texture);
        tr::Resources::Handle<Data::Material> upload(
            std::unique_ptr<tr::Data::Material> material,
            tr::Resources::Handle<tr::Data::Shader> shader
        );
        tr::Resources::Handle<tr::Data::Mesh> upload(std::unique_ptr<tr::Data::Mesh> mesh);
        void renderScene(const tr::Data::Camera& camera, const tr::Data::Scene& scene);
    };
}
