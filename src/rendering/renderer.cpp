#include "renderer.h"

namespace tr::Rendering {
    Renderer::Renderer(RHI* rhi, tr::Data::ResourceManager* resourceManager)
        : _renderingInterface(rhi), _resourceManager(resourceManager)
    {
    }

    void Renderer::clearState() {
        _renderingInterface->clearResources();
    }

    void Renderer::reloadResources() {
        clearState();
        for (auto [handle, shader] : _resourceManager->shadersPool) {
            _renderingInterface->createShader(handle, *shader);
        }
        for (auto [handle, texture] : _resourceManager->texturesPool) {
            _renderingInterface->createTexture(handle, *texture);
        }
        for (auto [handle, material] : _resourceManager->materialsPool) {
            auto* shader = _resourceManager->shadersPool.get(material->shader);
            _renderingInterface->createMaterial(handle, *material, *shader);
        }
        for (auto [handle, mesh] : _resourceManager->meshesPool) {
            _renderingInterface->createMesh(handle, *mesh);
        }
    }

    void Renderer::renderScene(const tr::Data::Camera& camera, const tr::Data::Scene& scene) {
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
                .materials = std::span<const tr::Resources::Handle<tr::Data::Material>>(object->materials)
            };

            bool isOpaque = true;
            for (const auto& materialHandle : object->materials) {
                auto* material = _resourceManager->materialsPool.get(materialHandle);
                if (!material) continue;
                auto* shader = _resourceManager->shadersPool.get(material->shader);
                if (shader && shader->blendMode == tr::Data::BlendMode::Transparent) {
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
