#include "renderer.h"

#include <algorithm>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "window.h"
#include "scene_data.h"

namespace tr::Rendering {
    Renderer::Renderer(RHI& rhi, tr::App::Window& window)
        : _window(window), _renderingInterface(rhi) { }

    void Renderer::clearState() {
        _renderingInterface.clearResources();
    }

    void Renderer::render(
        const tr::Data::Scene& scene,
        const tr::Data::Camera& camera,
        const tr::Data::Light& light,
        ImDrawData* uiDrawData
    ) {
        const float aspect = static_cast<float>(_window.width()) / static_cast<float>(_window.height());
        glm::mat4 cameraProjection;
        if (camera.projection == tr::Data::CameraProjection::Perspective) {
            cameraProjection = glm::perspectiveRH_ZO(
                glm::radians(camera.fov),
                aspect,
                camera.near,
                camera.far
            );
        }
        else {
            const float halfHeight = camera.orthographicSize * 0.5f;
            const float halfWidth = halfHeight * aspect;
            cameraProjection = glm::orthoRH_ZO(
                -halfWidth,
                halfWidth,
                -halfHeight,
                halfHeight,
                camera.near,
                camera.far
            );
        }

        auto [sceneCenter, sceneSize] = scene.getSceneBounds();

        const glm::mat4 sceneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scene.scale));
        const glm::vec3 scaledSceneCenter = sceneCenter * scene.scale;
        const glm::vec3 scaledSceneSize = sceneSize * glm::abs(scene.scale);
        float sceneRadius = 2 * std::max(std::max(scaledSceneSize.x, scaledSceneSize.y), scaledSceneSize.z);
        glm::vec3 dir = glm::normalize(light.direction);
        float padding = sceneRadius;
        glm::vec3 eye = scaledSceneCenter - dir * (sceneRadius + padding);
        glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::mat4 lightView = glm::lookAt(eye, scaledSceneCenter, up);
        glm::mat4 lightProj = glm::orthoRH_ZO(
            -sceneRadius, sceneRadius,
            -sceneRadius, sceneRadius,
            0.0f, 2.0f * sceneRadius + padding
        );
        glm::mat4 lightViewProj = lightProj * lightView;

        SceneData sceneData{
            .view = glm::inverse(camera.transform.getMatrix()),
            .proj = cameraProjection,
            .cameraPos = camera.transform.position,
            .lightIntensity = light.intensity,
            .lightDirection = light.direction,
            .lightColor = light.color,
            .lightViewProj = lightViewProj,
        };
        _renderingInterface.startFrame(sceneData);

        auto cameraPosition = camera.transform.position;
        _opaqueDrawQueue.clear();
        _transparentDrawQueue.clear();

        for (const auto& object : scene.getObjects()) {
            if (!object || !object->mesh.isValid()) {
                continue;
            }

            DrawCommand command {
                .modelMatrix = sceneTransform * object->transform.getMatrix(),
                .mesh = object->mesh,
                .materials = std::span<const tr::Resources::Handle<tr::Data::Material>>(object->materials)
            };

            bool isOpaque = true;
            for (const auto& materialHandle : object->materials) {
                const auto* material = scene.materials.get(materialHandle);
                if (material && material->blendMode == tr::Data::BlendMode::Transparent) {
                    isOpaque = false;
                    break;
                }
            }
            if (isOpaque) {
                _opaqueDrawQueue.push_back(command);
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

        _renderingInterface.startShadowPass();
        for (const auto& command : _opaqueDrawQueue) {
            _renderingInterface.drawShadows(command); // dedicated simplified rendering
        }
        _renderingInterface.endShadowPass();

        _renderingInterface.startColorPass();
        for (const auto& command : _opaqueDrawQueue) {
            _renderingInterface.draw(command);
        }
        for (const auto& [command, depth] : _transparentDrawQueue) {
            _renderingInterface.draw(command);
        }
        _renderingInterface.endColorPass();

        _renderingInterface.drawUI(uiDrawData);

        _renderingInterface.endFrame();
    }
}
