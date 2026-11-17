#include "tiny_renderer.h"

#include <algorithm>
#include <cmath>

#include <fmt/format.h>

#include "demo_scenes.h"
#include "free_move_controller.h"
#include "orbit_controller.h"
#include "renderer.h"

namespace tr {
    void TinyRenderer::setupUI() {
        uiState.leftSections = {
            { .name = "Model", .opened = true, .drawCallback = [this]() { drawModelSection(); } },
            { .name = "Light", .drawCallback = [this]() { drawLightSection(); } },
            { .name = "Environment", .drawCallback = [this]() { drawEnvironmentSection(); } }
        };
        uiState.rightSections = {
            { .name = "Info", .opened = true, .drawCallback = [this]() { drawInfoSection(); } },
            { .name = "Settings", .drawCallback = [this]() { drawSettingsSection(); } },
            { .name = "Camera", .drawCallback = [this]() { drawCameraSection(); } },
            { .name = "Post Processing", .drawCallback = [this]() { drawPostProcessingSection(); } }
        };
        uiState.drawOverlay = [this]() { drawImportErrorPopup(); };
    }

    void TinyRenderer::drawModelSection() {
        if (uiProvider.button("Load Model")) {
            loadSelectedModel();
        }
        const auto demos = Loading::getDemoScenes();
        std::vector<const char*> demoNames;
        demoNames.reserve(demos.size());
        for (const auto& demo : demos) {
            demoNames.push_back(demo.name.data());
        }
        if (uiProvider.beginRow("DemoScene")) {
            uiProvider.combo("##Scene", &selectedDemoScene, demoNames);
            uiProvider.nextColumn();
            if (uiProvider.button("Load Demo")) {
                const auto& demo = demos[selectedDemoScene];
                loadScene(demo.build);
                modelName = demo.name;
                syncSceneUI();
            }
            uiProvider.endRow();
        }
        uiProvider.separator();
        uiProvider.text(modelName);
        uiProvider.separator();
        const glm::vec3 bounds = currentScene->getBounds().getSize() * sceneScale;
        uiProvider.text(fmt::format("Bounds: {:.2f} x {:.2f} x {:.2f}", bounds.x, bounds.y, bounds.z));
        uiProvider.text(fmt::format("Vertices: {}", currentScene->getVertCount()));
        uiProvider.text(fmt::format("Polygons: {}", currentScene->getTrisCount()));
        uiProvider.text(fmt::format("Objects: {}", currentScene->getObjects().size()));
        uiProvider.separator();
        if (uiProvider.dragFloat("Scale", &sceneScale, 0.01f, 0.001f, 1000.0f, "%.3f")) {
            sceneScale = std::max(sceneScale, 0.001f);
            needToUpdateSceneMatrix = true;
        }
    }

    void TinyRenderer::drawLightSection() {
        uiProvider.checkbox("Enabled", &environment.directionalLight.enabled);
        uiProvider.checkbox("Shadows", &environment.directionalLight.shadowsEnabled);
        uiProvider.separator();
        uiProvider.beginDisabled(!environment.directionalLight.enabled);
        uiProvider.dragFloat(
            "Intensity",
            &environment.directionalLight.intensity,
            0.01f
        );
        uiProvider.colorEdit3("Color", &environment.directionalLight.color.x);
        if (uiProvider.dragFloat("Yaw", &lightYaw, 0.5f, -180.0f, 180.0f, "%.1f°", UI::SliderFlags::WrapAround)) {
            updateLightDirection();
        }
        if (uiProvider.dragFloat("Pitch", &lightPitch, 0.5f, -180.0f, 180.0f, "%.1f°", UI::SliderFlags::WrapAround)) {
            updateLightDirection();
        }
        uiProvider.endDisabled();
    }

    void TinyRenderer::drawEnvironmentSection() {
        if (uiProvider.button("Load Skybox")) {
            loadSkybox();
        }
        if (environment.skyboxHandle.isValid()) {
            uiProvider.sameLine();
            if (uiProvider.button("Clear Skybox")) {
                renderer->getResources().destroyCubemap(environment.skyboxHandle);
                environment.skyboxHandle = {};
            }
            uiProvider.colorEdit3("Tint", &environment.skyboxTint.x);
            uiProvider.sliderFloat("Intensity", &environment.skyboxIntensity, 0.0f, 10.0f);
            uiProvider.sliderAngle("Rotation", &environment.skyboxRotation);
        }
        else {
            auto& sky = environment.proceduralSky;
            uiProvider.dragFloat("Density", &sky.density, 0.01f, 0.0f, 10.0f, "%.2f");
            uiProvider.sliderInt("Optical Depth Samples", &sky.opticalDepthSamples, 1, 64, "%d");
            uiProvider.sliderInt("Scattering Samples", &sky.scatteringSamples, 2, 128, "%d");
            uiProvider.sliderAngle("Sun Radius", &sky.sunRadius, 0.01f, 15.0f, "%.2f deg");
            uiProvider.sliderAngle("Sun Glow Width", &sky.sunGlowWidth, 0.01f, 60.0f, "%.2f deg");
            uiProvider.dragFloat("Sun Glow Strength", &sky.sunGlowStrength, 0.01f, 0.0f, 10.0f, "%.2f");
            uiProvider.sliderAngle("Moon Radius", &sky.moonRadius, 0.01f, 15.0f, "%.2f deg");
            uiProvider.sliderFloat("Moon Reflectance", &sky.moonReflectance, 0.0f, 1.0f, "%.2f");
            uiProvider.sliderAngle("Moon Glow Width", &sky.moonGlowWidth, 0.01f, 60.0f, "%.2f deg");
            uiProvider.dragFloat("Moon Glow Strength", &sky.moonGlowStrength, 0.01f, 0.0f, 10.0f, "%.2f");
            if (uiProvider.collapsingHeader("Stars")) {
                uiProvider.indent();
                uiProvider.pushID("Stars");
                uiProvider.dragFloat("Scale", &sky.starScale, 1.0f, 1.0f, 1000.0f, "%.0f", UI::SliderFlags::AlwaysClamp);
                uiProvider.sliderFloat("Density", &sky.starDensity, 0.0f, 1.0f, "%.3f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Core Width", &sky.starCoreWidth, 0.001f, 0.001f, 0.5f, "%.3f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Glow Width", &sky.starGlowWidth, 0.001f, 0.001f, 0.5f, "%.3f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Glow Strength", &sky.starGlowStrength, 0.001f, 0.0f, 10.0f, "%.3f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Brightness", &sky.starBrightness, 0.001f, 0.0f, 10.0f, "%.4f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Min Brightness", &sky.starMinBrightness, 0.01f, 0.0f, 10.0f, "%.2f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Brightness Variation", &sky.starBrightnessVariation, 0.01f, 0.0f, 10.0f, "%.2f", UI::SliderFlags::AlwaysClamp);
                uiProvider.dragFloat("Brightness Falloff", &sky.starBrightnessFalloff, 0.1f, 0.1f, 32.0f, "%.1f", UI::SliderFlags::AlwaysClamp);
                uiProvider.popID();
                uiProvider.unindent();
            }
        }
    }

    void TinyRenderer::drawInfoSection() {
        uiProvider.text(application.name);
        uiProvider.sameLine();
        uiProvider.text(application.version);
        uiProvider.separator();
        uiProvider.text(fmt::format("FPS: {}", static_cast<int>(1.0f / smoothedDeltaTime)));
        uiProvider.separator();
        uiProvider.text(fmt::format("GPU: {}", gpuName));
    }

    void TinyRenderer::drawSettingsSection() {
        const uint32_t currentSamples = rhi->getMsaaSamples();
        std::vector<std::string> sampleLabels;
        std::vector<const char*> sampleNames;
        sampleLabels.reserve(supportedMsaaSamples.size());
        sampleNames.reserve(supportedMsaaSamples.size());
        int selected = 0;
        for (size_t i = 0; i < supportedMsaaSamples.size(); ++i) {
            const uint32_t samples = supportedMsaaSamples[i];
            sampleLabels.push_back(fmt::format("{}x", samples));
            sampleNames.push_back(sampleLabels.back().c_str());
            if (samples == currentSamples) {
                selected = static_cast<int>(i);
            }
        }
        if (uiProvider.combo("MSAA", &selected, sampleNames)) {
            rhi->setMsaaSamples(supportedMsaaSamples[selected]);
        }

        bool vsyncEnabled = rhi->getVsyncEnabled();
        uiProvider.beginDisabled(!rhi->supportsVsyncToggle());
        if (uiProvider.checkbox("VSync", &vsyncEnabled)) {
            rhi->setVsyncEnabled(vsyncEnabled);
        }
        if (!rhi->supportsVsyncToggle()) {
            uiProvider.tooltip("VSync cannot be disabled on this surface.");
        }
        uiProvider.endDisabled();
    }

    void TinyRenderer::drawCameraSection() {
        const char* controllerNames[] = { "Free Move", "Orbit" };
        int controller = static_cast<int>(cameraControllerType);
        if (uiProvider.combo("Controller", &controller, controllerNames)) {
            setCameraController(static_cast<CameraControllerType>(controller));
        }

        if (cameraControllerType == CameraControllerType::FreeMove) {
            uiProvider.pushID("FreeMove");
            auto& freeMove = static_cast<Controllers::FreeMoveController&>(*cameraController);
            uiProvider.dragFloat("Speed", &freeMove.speed, 0.1f, 0.1f, 100.0f);
            uiProvider.dragFloat(
                "Shift Multiplier",
                &freeMove.shiftMultiplier,
                0.1f,
                1.0f,
                20.0f
            );
            uiProvider.dragFloat("Look Sensitivity", &freeMove.sensitivity, 0.01f, 0.01f, 2.0f);
            uiProvider.popID();
        }
        else {
            uiProvider.pushID("Orbit");
            auto& orbit = static_cast<Controllers::OrbitController&>(*cameraController);
            uiProvider.dragFloat("Max Radius", &orbit.maxRadius, 0.1f, 0.1f, 1000.0f);
            uiProvider.dragFloat("Orbit Sensitivity", &orbit.sensitivity, 0.01f, 0.01f, 2.0f);
            uiProvider.dragFloat("Zoom Sensitivity", &orbit.zoomSensitivity, 0.01f, 0.01f, 10.0f);
            uiProvider.popID();
        }

        uiProvider.separator();
        const char* projectionNames[] = { "Perspective", "Orthographic" };
        int projection = static_cast<int>(camera.projection);
        if (uiProvider.combo("Projection", &projection, projectionNames)) {
            camera.projection = static_cast<Data::CameraProjection>(projection);
        }

        if (camera.projection == Data::CameraProjection::Perspective) {
            uiProvider.sliderFloat("Field of View", &camera.fov, 1.0f, 179.0f, "%.1f deg");
        }
        else {
            uiProvider.dragFloat("Size", &camera.orthographicSize, 0.1f, 0.1f, 1000.0f);
        }
        uiProvider.dragFloat("Near", &camera.near, 0.01f, 0.001f, camera.far);
        uiProvider.dragFloat("Far", &camera.far, 0.1f, camera.near, 10000.0f);
    }

    void TinyRenderer::drawPostProcessingSection() {
        uiProvider.inputInt("Exposure", &postProcessingData.exposure);
        int32_t selected = static_cast<int32_t>(postProcessingData.tonemapping);
        if (uiProvider.combo("Tone Mapping", &selected, Data::toneMappingNames)) {
            postProcessingData.tonemapping = static_cast<Data::Tonemapping>(selected);
        }
    }

    void TinyRenderer::drawImportErrorPopup() {
        uiProvider.messagePopup("Import Error", importError, importErrorPopupPending);
    }

    void TinyRenderer::syncSceneUI() {
        const glm::vec3 direction = glm::normalize(environment.directionalLight.direction);
        lightYaw = glm::degrees(std::atan2(direction.x, direction.z));
        lightPitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
    }
}
