#include "renderer.h"

using namespace tr::Resources;
using namespace tr::Data;

namespace tr::Rendering {
    Renderer::Renderer(Rendering::RHI& rhi) : _renderingInterface(&rhi) {
    }

    Renderer::~Renderer() {
    }

    void Renderer::clearState() {
        _shadersPool.clear();
        _texturesPool.clear();
        _materialsPool.clear();
        _meshesPool.clear();
        _renderingInterface->clearResources();
    }

    Handle<Shader> Renderer::upload(std::unique_ptr<Shader> shader) {
        auto handle = _shadersPool.add(std::move(shader));
        _renderingInterface->createShader(handle, *_shadersPool.get(handle));
        return handle;
    }

    Handle<Texture> Renderer::upload(std::unique_ptr<Texture> texture) {
        auto handle = _texturesPool.add(std::move(texture));
        _renderingInterface->createTexture(handle, *_texturesPool.get(handle));
        return handle;
    }

    Handle<Material> Renderer::upload(std::unique_ptr<Material> material, Handle<Shader> shader) {
        auto handle = _materialsPool.add(std::move(material));
        _renderingInterface->createMaterial(handle, *_materialsPool.get(handle), *_shadersPool.get(shader));
        return handle;
    }

    Handle<Mesh> Renderer::upload(std::unique_ptr<Mesh> mesh) {
        auto handle = _meshesPool.add(std::move(mesh));
        _renderingInterface->createMesh(handle, *_meshesPool.get(handle));
        return handle;
    }

    void Renderer::renderScene(const Camera& camera, const Scene& scene) {
        _renderingInterface->startFrame(camera);

        auto cameraPosition = glm::vec3(camera.transform.getMatrix()[3]);
        _transparentDrawQueue.clear();

        for (const auto& object : scene.getObjects()) {
            if (!object || !object->mesh.isValid()) {
                continue;
            }

            DrawCommand command {
                .modelMatrix = object->transform.getMatrix(),
                .mesh = object->mesh,
                .materials = std::span<const Handle<Material>>(object->materials)
            };

            bool isOpaque = true;
            for (const auto& materialHandle : object->materials) {
                auto* material = _materialsPool.get(materialHandle);
                if (!material) continue;
                auto* shader = _shadersPool.get(material->shader);
                if (shader && shader->blendMode == Data::BlendMode::Transparent) {
                    isOpaque = false;
                    break;
                }
            }
            if (isOpaque) {
                _renderingInterface->draw(command);
            }
            else {
                glm::vec3 pos = glm::vec3(command.modelMatrix[3]);
                float depth = glm::length(pos - cameraPosition);
                _transparentDrawQueue.push_back({ command, depth });
            }
        }
        std::sort(_transparentDrawQueue.begin(), _transparentDrawQueue.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second; // back to front
            }
        );
        for (const auto& [command, depth] : _transparentDrawQueue) {
            _renderingInterface->draw(command);
        }

        _renderingInterface->endFrame();
    }
}
