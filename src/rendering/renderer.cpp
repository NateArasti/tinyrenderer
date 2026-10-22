#include "renderer.h"

#include <map>
#include <tuple>
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
#include "instance_data.h"

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
                .skyboxIntensity = env.skyboxIntensity,
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
        _colorPass = std::make_unique<ColorPass>(_mainShader, resources);
        _postProcessingPass = std::make_unique<PostProcessingPass>(resources);
        _uiPass = std::make_unique<UIPass>(resources, _rhi.uiRenderer());
    }

    Renderer::~Renderer() {
        _rhi.waitIdle();
        _uiPass.reset();
        _postProcessingPass.reset();
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

    void Renderer::prepareScene(tr::Data::Scene& scene) {
        auto& resources = _rhi.resources();

        auto fallbackTexture = resources.texture({});
        for (int i = 0; i < MAX_TEXTURES; ++i) {
            _texturesTable[i] = fallbackTexture;
        }
        for (int i = 0; i < scene.ownedTextures.size(); ++i) {
            _texturesTable[i + 1] = resources.texture(scene.ownedTextures[i]);
        }

        std::vector<std::byte> materialsBuffer;
        for (auto materialHandle : scene.ownedMaterials) {
            const auto& material = resources.material(materialHandle);
            std::vector<std::byte> materialBytes = material.packMaterial(_mainShader);
            materialsBuffer.insert(materialsBuffer.end(), materialBytes.begin(), materialBytes.end());
        }
        _materialsTableBuffer = resources.createBuffer(BufferDesc{
            .data = std::as_bytes(std::span(materialsBuffer)),
            .usageFlags = BufferUsageFlags::Storage,
        });
        _colorPass->setMaterialTable(_materialsTableBuffer, _texturesTable);
    }

    void Renderer::unloadSceneResources(Data::Scene& scene) {
        _rhi.waitIdle();
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

        if (_instanceBuffer.isValid()) {
            resources.destroyBuffer(_instanceBuffer);
            _instanceBuffer = {};
        }
        if (_materialsTableBuffer.isValid()) {
            resources.destroyBuffer(_materialsTableBuffer);
            _materialsTableBuffer = {};
        }
        auto fallbackTexture = resources.texture({});
        for (int i = 0; i < MAX_TEXTURES; ++i) {
            _texturesTable[i] = fallbackTexture;
        }
    }

    void Renderer::updateSceneMatrix(tr::Data::Scene& scene) {
        _rhi.waitIdle();

        auto& resources = _rhi.resources();

        std::unordered_map<Handle<Material>, uint32_t> materialIndices;
        for (uint32_t i = 0; i < scene.ownedMaterials.size(); ++i) {
            materialIndices.emplace(scene.ownedMaterials.at(i), i);
        }

        if (_instanceBuffer.isValid()) {
            resources.destroyBuffer(_instanceBuffer);
            _instanceBuffer = {};
        }
        std::vector<InstanceData> instanceBuffer;
        
        _opaqueDrawQueue.clear();
        _transparentDrawQueue.clear();
        using BatchKey = std::tuple<Handle<Mesh>, uint32_t>;
        std::map<BatchKey, std::vector<InstanceData>> batchGroups;

        const auto sceneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scene.scale));
        for (const auto& object : scene.getObjects()) {
            if (!object || !object->mesh.isValid()) {
                continue;
            }

            const glm::mat4 modelMatrix = sceneTransform * object->transform.getMatrix();
            const glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
            for (uint32_t i = 0; i < object->materials.size(); ++i) {
                const auto& material = object->materials[i];
                if (_rhi.resources().material(material).blendMode == BlendMode::Opaque) {
                    batchGroups[{ object->mesh, i }].push_back(
                        InstanceData{
                            .model = modelMatrix,
                            .normal = normalMatrix,
                            .materialIndex = materialIndices.at(material),
                        }
                    );
                }
                else {
                    _transparentDrawQueue.push_back(DrawCommand{
                        .mesh = object->mesh,
                        .subMeshIndex = i,
                        .firstInstance = static_cast<uint32_t>(instanceBuffer.size()),
                        .sortPosition = modelMatrix[3],
                    });
                    instanceBuffer.push_back(InstanceData{
                        .model = modelMatrix,
                        .normal = normalMatrix,
                        .materialIndex = materialIndices.at(object->materials[i])
                    });
                }
            }
        }

        for (auto& [key, instancesData] : batchGroups) {
            _opaqueDrawQueue.push_back(DrawCommand{
                .mesh = std::get<0>(key),
                .subMeshIndex = std::get<1>(key),
                .firstInstance = static_cast<uint32_t>(instanceBuffer.size()),
                .instanceCount = static_cast<uint32_t>(instancesData.size()),
            });
            instanceBuffer.insert(instanceBuffer.end(), instancesData.begin(), instancesData.end());
        }
        _instanceBuffer = resources.createBuffer(BufferDesc{
            .data = std::as_bytes(std::span(instanceBuffer)),
            .usageFlags = BufferUsageFlags::Storage,
        });
    }

    void Renderer::prepareUI() {
        _uiPass->prepareFrame();
    }

    void Renderer::updatePipelines(const FrameContext& frame) {
        const auto samples = _rhi.getMsaaSamples();
        const auto finalColorFormat = frame.colorFormat;
        if (_colorFormat == finalColorFormat && _samples == samples) {
            return;
        }

        _rhi.waitIdle();
        _skyboxPass->createPipeline(HDR_FORMAT, samples);
        _colorPass->createPipelines(HDR_FORMAT, _rhi.getDepthFormat(), samples);
        _postProcessingPass->createPipeline(finalColorFormat);
        _colorFormat = finalColorFormat;
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
        frame.instanceBuffer = _instanceBuffer;

        auto& resources = _rhi.resources();
        resources.updateBuffer(frame.cameraDataBuffer, std::as_bytes(std::span(&frame.cameraData, 1)));
        resources.updateBuffer(frame.environmentDataBuffer, std::as_bytes(std::span(&frame.environmentData, 1)));
    }

    void Renderer::sortTransparentQueue(const Data::Scene& scene, const Data::Camera& camera) {
        const auto cameraPosition = camera.transform.position;
        std::sort(_transparentDrawQueue.begin(),
            _transparentDrawQueue.end(),
            [cameraPosition](const DrawCommand& a, const DrawCommand& b) {
                const auto distanceA = glm::vec3(a.sortPosition) - cameraPosition;
                const auto distanceB = glm::vec3(b.sortPosition) - cameraPosition;
                return glm::dot(distanceA, distanceA) > glm::dot(distanceB, distanceB);
            }
        );
    }

    void Renderer::setupGraph(
        RenderGraph& graph,
        const FrameContext& frame,
        const tr::Data::Environment& environment,
        const PostProcessingData& postProcessingData,
        ImDrawData* uiDrawData
    ) {
        auto& resources = _rhi.resources();

        const auto backbuffer = graph.importTexture(
            "Backbuffer",
            frame.backbuffer,
            frame.backbufferInitialState
        );
        const auto hdr = graph.importTexture(
            "HDR",
            frame.hdrTarget,
            TextureState::Undefined
        );
        const auto color =
            frame.colorTarget == frame.hdrTarget
            ? hdr
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

        _shadowPass->draws =
            environment.directionalLight.enabled && environment.directionalLight.shadowsEnabled
            ? std::span<const DrawCommand>(_opaqueDrawQueue)
            : std::span<const DrawCommand>();
        graph.addPass(*_shadowPass);

        _colorPass->target = color;
        _colorPass->depth = depth;
        _colorPass->resolve = _samples > 1 ? hdr : RGHandle{};
        _colorPass->shadow = _shadowPass->target;
        _colorPass->irradiance = irradiance;
        _colorPass->prefiltered = prefiltered;
        _colorPass->brdf = brdf;
        _colorPass->opaque = _opaqueDrawQueue;
        _colorPass->transparent = _transparentDrawQueue;
        graph.addPass(*_colorPass);

        _postProcessingPass->postProcessingData = postProcessingData;
        _postProcessingPass->source = hdr;
        _postProcessingPass->target = backbuffer;
        graph.addPass(*_postProcessingPass);

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
        const PostProcessingData& postProcessingData,
        ImDrawData* uiDrawData
    ) {
        auto frame = _rhi.startFrame();
        if (!frame) {
            return;
        }

        updatePipelines(*frame);
        updateFrameData(*frame, scene, camera, environment);
        sortTransparentQueue(scene, camera);

        RenderGraph graph(_rhi.resources());
        setupGraph(graph, *frame, environment, postProcessingData, uiDrawData);
        graph.build();
        graph.execute(*frame);

        _rhi.endFrame();
    }
}
