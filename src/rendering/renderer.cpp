#include "renderer.h"

#include <glm/glm.hpp>

#include "window.h"
#include "scene_data.h"

namespace tr::Rendering {
    Renderer::Renderer(RHI* rhi, tr::Data::ResourceManager* resourceManager, tr::App::Window* window)
        : _renderingInterface(rhi), _resourceManager(resourceManager), _window(window)
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
        SceneData sceneData{
            .view = glm::inverse(camera.transform.getMatrix()),
            .proj = glm::perspective(
                glm::radians(camera.fov),
                static_cast<float>(_window->width()) / static_cast<float>(_window->height()),
                camera.near, camera.far),
            .lightDirection = scene.directionalLight.direction,
            .lightIntensity = scene.directionalLight.intensity,
            .lightColor = scene.directionalLight.color,
            .cameraPos = camera.transform.position,
        };
        _renderingInterface->startFrame(sceneData);

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
