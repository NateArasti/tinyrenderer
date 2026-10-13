#include "renderer.h"

#include <algorithm>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render_graph.h"
#include "passes/ui_pass.h"
#include "passes/skybox_pass.h"
#include "passes/shadow_pass.h"
#include "passes/color_pass.h"
#include "camera_data.h"
#include "environment_data.h"

namespace tr::Rendering {
    namespace {
        CameraData createCameraData(const tr::Data::Camera& camera, float aspect) {
            glm::mat4 cameraProjection;
            if (camera.projection == tr::Data::CameraProjection::Perspective) {
                cameraProjection = glm::perspectiveLH_ZO(glm::radians(camera.fov), aspect, camera.near, camera.far);
            }
            else {
                const float halfHeight = camera.orthographicSize * 0.5f;
                const float halfWidth = halfHeight * aspect;
                cameraProjection =
                    glm::orthoLH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.near, camera.far);
            }

            return CameraData{
                .view = glm::inverse(camera.transform.getMatrix()),
                .proj = cameraProjection,
                .cameraPos = camera.transform.position,
            };
        }

        EnvironmentData createEnvironmentData(const tr::Data::Scene& scene, const tr::Data::Environment& env) {
            auto [sceneCenter, sceneSize] = scene.getSceneBounds();
            const glm::vec3 scaledSceneCenter = sceneCenter * scene.scale;
            const glm::vec3 scaledSceneSize = sceneSize * glm::abs(scene.scale);
            float sceneRadius =
                std::max(0.01f, 2 * std::max(std::max(scaledSceneSize.x, scaledSceneSize.y), scaledSceneSize.z));
            glm::vec3 dir = glm::normalize(env.directionalLight.direction);
            float padding = sceneRadius;
            glm::vec3 eye = scaledSceneCenter - dir * (sceneRadius + padding);
            glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 lightView = glm::lookAtLH(eye, scaledSceneCenter, up);
            glm::mat4 lightProj = glm::orthoLH_ZO(-sceneRadius,
                sceneRadius,
                -sceneRadius,
                sceneRadius,
                0.0f,
                2.0f * sceneRadius + padding);
            glm::mat4 lightViewProj = lightProj * lightView;

            return EnvironmentData{
                .clearColor = env.clearColor,
                .skyboxRotation = env.skyboxRotation,
                .skyboxTint = env.skyboxTint,
                .skyboxExposure = env.skyboxExposure,
                .lightDirection = env.directionalLight.direction,
                .lightIntensity = env.directionalLight.enabled ? env.directionalLight.intensity : 0.0f,
                .lightColor = env.directionalLight.color,
                .lightViewProj = lightViewProj,
                .hasSkybox = env.skyboxHandle.isValid() ? 1.0f : 0.0f,
            };
        }
    }

    Renderer::Renderer(RHI& rhi) : _rhi(rhi) {
        auto& resources = _rhi.resources();
        const CameraData camera{};
        const EnvironmentData environment{};
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _cameraDataBuffers[i] = resources.createBuffer({
                .data = std::as_bytes(std::span(&camera, 1)),
                .usageFlags = BufferUsageFlags::UniformBuffer,
            });
            _environmentDataBuffers[i] = resources.createBuffer({
                .data = std::as_bytes(std::span(&environment, 1)),
                .usageFlags = BufferUsageFlags::UniformBuffer,
            });
        }

        _skyboxPass = std::make_unique<SkyboxPass>(resources);
        _shadowPass = std::make_unique<ShadowPass>(resources);
        _colorPass = std::make_unique<ColorPass>(resources);
        _uiPass = std::make_unique<UIPass>(resources, _rhi.uiRenderer());
    }

    Renderer::~Renderer() {
        _rhi.waitIdle();
        invalidateSceneCaches();
        _uiPass.reset();
        _colorPass.reset();
        _shadowPass.reset();
        _skyboxPass.reset();

        for (auto buffer : _cameraDataBuffers) {
            _rhi.resources().destroyBuffer(buffer);
        }
        for (auto buffer : _environmentDataBuffers) {
            _rhi.resources().destroyBuffer(buffer);
        }
    }

    void Renderer::invalidateSceneCaches() {
        _rhi.waitIdle();
        _colorPass->clearMaterials();
        _opaqueDrawQueue.clear();
        _transparentDrawQueue.clear();
    }

    void Renderer::unloadSceneResources(Data::Scene& scene) {
        invalidateSceneCaches();
        auto& resources = _rhi.resources();
        for (auto handle : scene.ownedMaterials) {
            resources.destroyMaterial(handle);
        }
        for (auto handle : scene.ownedMeshes) {
            resources.destroyMesh(handle);
        }
        for (auto handle : scene.ownedTextures) {
            resources.destroyTexture(handle);
        }
        scene.ownedMaterials.clear();
        scene.ownedMeshes.clear();
        scene.ownedTextures.clear();
    }

    void Renderer::prepareUI() {
        _uiPass->prepareFrame();
    }

    void Renderer::updatePipelines(const FrameContext& frame) {
        const auto samples = _rhi.getMsaaSamples();
        const auto colorFormat = frame.colorFormat;
        if (_colorFormat == colorFormat && _samples == samples) {
            return;
        }

        _rhi.waitIdle();
        _skyboxPass->createPipeline(colorFormat, samples);
        _colorPass->createPipelines(colorFormat, _rhi.getDepthFormat(), samples);
        _colorFormat = colorFormat;
        _samples = samples;
    }

    void Renderer::updateFrameData(
        FrameContext& frame,
        const Data::Scene& scene,
        const Data::Camera& camera,
        const Data::Environment& environment
    ) {
        frame.cameraData = createCameraData(camera, frame.extent.x / frame.extent.y);
        frame.cameraData.proj[1][1] *= _rhi.getProjectionYSign();
        frame.environmentData = createEnvironmentData(scene, environment);
        frame.cameraDataBuffer = _cameraDataBuffers[frame.slot];
        frame.environmentDataBuffer = _environmentDataBuffers[frame.slot];

        auto& resources = _rhi.resources();
        resources.updateBuffer(frame.cameraDataBuffer, std::as_bytes(std::span(&frame.cameraData, 1)));
        resources.updateBuffer(frame.environmentDataBuffer, std::as_bytes(std::span(&frame.environmentData, 1)));
    }

    void Renderer::prepareDrawCalls(const Data::Scene& scene, const Data::Camera& camera) {
        _opaqueDrawQueue.clear();
        _transparentDrawQueue.clear();

        const auto sceneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scene.scale));
        for (const auto& object : scene.getObjects()) {
            if (!object || !object->mesh.isValid()) {
                continue;
            }

            const auto modelMatrix = sceneTransform * object->transform.getMatrix();
            for (uint32_t i = 0; i < object->materials.size(); ++i) {
                const auto& material = object->materials[i];
                DrawCommand draw{
                    .modelMatrix = modelMatrix,
                    .mesh = object->mesh,
                    .subMeshIndex = i,
                    .material = material,
                };
                if (_rhi.resources().material(material).blendMode == BlendMode::Opaque) {
                    _opaqueDrawQueue.push_back(draw);
                }
                else {
                    _transparentDrawQueue.push_back(draw);
                }
            }
        }

        const auto cameraPosition = camera.transform.position;
        std::sort(_transparentDrawQueue.begin(),
            _transparentDrawQueue.end(),
            [cameraPosition](const DrawCommand& a, const DrawCommand& b) {
                const auto distanceA = glm::vec3(a.modelMatrix[3]) - cameraPosition;
                const auto distanceB = glm::vec3(b.modelMatrix[3]) - cameraPosition;
                return glm::dot(distanceA, distanceA) > glm::dot(distanceB, distanceB);
            }
        );
    }

    void Renderer::setupGraph(
        RenderGraph& graph,
        const FrameContext& frame,
        const Data::Environment& environment,
        ImDrawData* uiDrawData
    ) {
        auto& resources = _rhi.resources();

        const auto backbuffer = graph.importTexture(
            "Backbuffer",
            frame.backbuffer,
            frame.backbufferInitialState
        );
        const auto color = frame.colorTarget == frame.backbuffer
            ? backbuffer
            : graph.importTexture("MSAA color", frame.colorTarget, TextureState::Undefined);
        const auto depth = graph.importTexture("Depth", frame.depthTarget, TextureState::Undefined);

        const auto cubemap = resources.cubemap(environment.skyboxHandle);
        const auto skybox = graph.importTexture(
            "Skybox",
            cubemap.skybox,
            TextureState::ShaderRead
        );
        const auto irradiance = graph.importTexture(
            "Irradiance",
            cubemap.irradiance,
            TextureState::ShaderRead
        );
        const auto prefiltered = graph.importTexture(
            "Prefiltered environment",
            cubemap.prefiltered,
            TextureState::ShaderRead
        );
        const auto brdf = graph.importTexture(
            "BRDF LUT",
            cubemap.brdf,
            TextureState::ShaderRead
        );


        _skyboxPass->target = color;
        _skyboxPass->cubemap = skybox;
        graph.addPass(*_skyboxPass);

        _shadowPass->draws = environment.directionalLight.enabled && environment.directionalLight.shadowsEnabled
            ? std::span<const DrawCommand>(_opaqueDrawQueue)
            : std::span<const DrawCommand>();
        graph.addPass(*_shadowPass);

        _colorPass->target = color;
        _colorPass->depth = depth;
        _colorPass->resolve = _samples > 1 ? backbuffer : RGHandle{};
        _colorPass->shadow = _shadowPass->target;
        _colorPass->irradiance = irradiance;
        _colorPass->prefiltered = prefiltered;
        _colorPass->brdf = brdf;
        _colorPass->opaque = _opaqueDrawQueue;
        _colorPass->transparent = _transparentDrawQueue;
        graph.addPass(*_colorPass);

        if (uiDrawData != nullptr) {
            _uiPass->target = backbuffer;
            _uiPass->drawData = uiDrawData;
            graph.addPass(*_uiPass);
        }

        graph.setFinalState(backbuffer, TextureState::Present);
    }

    void Renderer::render(
        const Data::Scene& scene,
        const Data::Camera& camera,
        const Data::Environment& environment,
        ImDrawData* uiDrawData
    ) {
        auto frame = _rhi.startFrame();
        if (!frame) {
            return;
        }

        updatePipelines(*frame);
        updateFrameData(*frame, scene, camera, environment);
        prepareDrawCalls(scene, camera);

        RenderGraph graph(_rhi.resources());
        setupGraph(graph, *frame, environment, uiDrawData);
        graph.build();
        graph.execute(*frame);

        _rhi.endFrame();
    }
}
