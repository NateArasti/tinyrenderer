#include "tiny_renderer.h"

#include <cmath>

#include <fmt/base.h>

#include "camera_controller.h"
#include "demo_scenes.h"
#include "renderer.h"
#include "vulkan_rhi.h"

namespace tr {
    TinyRenderer::TinyRenderer() {
        fmt::println(
            "Starting {} with ({}, {}) window. Version={}.",
            application.name,
            application.window->width(),
            application.window->height(),
            application.version
        );

        setupUI();
        setupRendering();
        auto startScene = Loading::getDemoScenes()[0];
        modelName = startScene.name;
        loadScene(startScene.build);
        setupCamera();
        setupLight();

        gpuName = rhi->getDeviceName();
        supportedMsaaSamples = rhi->getSupportedMsaaSamples();
        syncSceneUI();
    }

    TinyRenderer::~TinyRenderer() {
        if (currentScene) {
            renderer->unloadSceneResources(*currentScene);
        }
        if (environment.skyboxHandle.isValid()) {
            renderer->getResources().destroyCubemap(environment.skyboxHandle);
        }
    }

    void TinyRenderer::setupRendering() {
        rhi = std::make_unique<Rendering::Vulkan::VulkanRHI>(application);
        renderer = std::make_unique<Rendering::Renderer>(*rhi);
    }

    void TinyRenderer::setupLight() {
        environment.directionalLight = {
            .direction = glm::vec3(0.5f, -1.0f, -0.5f),
            .intensity = 5.0f,
            .color = glm::vec4(1.0f)
        };
    }

    void TinyRenderer::handleResize() {
        App::Window& window = *application.window;
        if (!window.wasResized()) return;

        rhi->resize(window.width(), window.height());
        window.clearResizedFlag();
        fmt::println("Window was resized to ({}, {}).", window.width(), window.height());
    }

    int TinyRenderer::run() {
        App::Window& window = *application.window;

        while (!window.shouldClose()) {
            input.beginFrame();
            window.pollEvents();

            const float deltaTime = window.deltaTime();
            if (fpsInitialized) {
                const float smoothing = 1.0f - std::exp(-deltaTime / 0.25f);
                smoothedDeltaTime += (deltaTime - smoothedDeltaTime) * smoothing;
            }
            else {
                fpsInitialized = true;
            }

            if (input.isKeyJustPressed(App::Key::F3)) {
                showUI = !showUI;
            }

            handleResize();

            bool canReadInput = true;
            UI::UIFrame uiFrame;
            if (showUI) {
                renderer->prepareUI();
                uiFrame = uiProvider.update(uiState);
                canReadInput = !uiFrame.wantsMouse && !uiFrame.wantsKeyboard;
            }

            if (needToUpdateSceneMatrix) {
                renderer->updateSceneMatrix(*currentScene, sceneScale);
                needToUpdateSceneMatrix = false;
            }

            if (canReadInput) {
                cameraController->update(camera, input, window.deltaTime());
            }

            if (window.width() != 0 && window.height() != 0) {
                renderer->render(*currentScene, camera, environment, postProcessingData, uiFrame.drawData, sceneScale);
            }
        }

        fmt::println("Session end.");
        return 0;
    }
}

