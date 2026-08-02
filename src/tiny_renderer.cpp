#define GLM_ENABLE_EXPERIMENTAL

#include "tiny_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/base.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "import_error.h"
#include "application.h"
#include "camera.h"
#include "camera_controller.h"
#include "file_picker.h"
#include "free_move_controller.h"
#include "input.h"
#include "orbit_controller.h"
#include "pbr.h"
#include "renderer.h"
#include "loader.h"
#include "ui_data.h"
#include "ui_provider.h"
#include "vulkan_renderer.h"
#include "window.h"
#include "cubemap_loader.h"

namespace tr {
    namespace {
        enum class CameraControllerType {
            FreeMove,
            Orbit
        };

        constexpr std::string_view APP_NAME = "tinyrenderer";
        constexpr std::string_view APP_VERSION = "v0.0.1";
    }

    struct TinyRenderer::Impl {
        using SceneFactory = std::function<std::unique_ptr<Data::Scene>(Loading::LoadContext)>;

        App::Application application{
            .name = APP_NAME,
            .version = APP_VERSION,
            .window = std::make_unique<App::Window>(1280, 720, APP_NAME)
        };
        App::Input input{ *application.window };
        UI::UIProvider uiProvider{ *application.window, input };

        Data::EmbeddedShaders::Pbr baseShader;
        std::unique_ptr<Rendering::Vulkan::VulkanRenderer> rhi;
        std::unique_ptr<Rendering::Renderer> renderer;
        
        std::unique_ptr<Data::Scene> currentScene;
        Data::Environment environment;

        Data::Camera camera;
        std::unique_ptr<Controllers::CameraController> cameraController;

        UI::UIState uiState;
        UI::UIWindow debugWindow;
        UI::UIWindow modelWindow;
        UI::UIWindow envWindow;
        UI::UIWindow lightWindow;
        UI::UIWindow cameraWindow;
        std::string gpuName;
        std::string modelName = "Default Scene";
        std::string importError;
        std::vector<uint32_t> supportedMsaaSamples;
        float lightYaw = 0.0f;
        float lightPitch = 0.0f;
        float smoothedDeltaTime = 1.0f / 60.0f;
        CameraControllerType cameraControllerType = CameraControllerType::FreeMove;
        float freeMoveSpeed = 10.0f;
        float freeMoveShiftMultiplier = 2.5f;
        float freeMoveSensitivity = 0.1f;
        float orbitMaxRadius = 50.0f;
        float orbitSensitivity = 0.2f;
        float orbitZoomSensitivity = 0.75f;
        bool fpsInitialized = false;
        bool showUI = false;
        bool importErrorPopupPending = false;

        Impl() {
            fmt::println(
                "Starting {} with ({}, {}) window. Version={}.",
                application.name,
                application.window->width(),
                application.window->height(),
                application.version
            );

            setupUI();
            setupRendering();
            loadScene(Loading::Loader::loadDefaultScene);
            setupCamera();
            setupLight();

            gpuName = rhi->getDeviceName();
            supportedMsaaSamples = rhi->getSupportedMsaaSamples();
            syncSceneUI();
        }

        void setupUI() {
            debugWindow = UI::UIWindow{
                .name = "Debug",
                .width = 260.0f,
                .height = 105.0f,
                .drawCallback = [this]() {
                    ImGui::Text("FPS: %d", static_cast<int>(1.0f / smoothedDeltaTime));
                    ImGui::TextUnformatted(gpuName.c_str());

                    const uint32_t currentSamples = rhi->getMsaaSamples();
                    const std::string preview = std::to_string(currentSamples) + "x";
                    if (ImGui::BeginCombo("MSAA", preview.c_str())) {
                        for (uint32_t samples : supportedMsaaSamples) {
                            const std::string label = std::to_string(samples) + "x";
                            const bool selected = samples == currentSamples;
                            if (ImGui::Selectable(label.c_str(), selected)) {
                                rhi->setMsaaSamples(samples);
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                },
            };

            modelWindow = UI::UIWindow{
                .name = "Model",
                .height = 180.0f,
                .drawCallback = [this]() {
                    if (ImGui::Button("Load Model")) {
                        loadSelectedModel();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load Default Model")) {
                        loadScene(Loading::Loader::loadDefaultScene);
                        modelName = "Default Scene";
                        syncSceneUI();
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted(modelName.c_str());
                    ImGui::Separator();
                    const glm::vec3 bounds = currentScene->getSceneBounds().second * currentScene->scale;
                    ImGui::Text("Bounds: %.2f x %.2f x %.2f", bounds.x, bounds.y, bounds.z);
                    ImGui::Text("Vertices: %zu", currentScene->verticesCount);
                    ImGui::Text("Polygons: %zu", currentScene->polygonCount);
                    ImGui::Text("Objects: %zu", currentScene->getObjects().size());
                    ImGui::Separator();
                    currentScene->scale = std::max(currentScene->scale, 0.001f);
                    ImGui::DragFloat("Scale", &currentScene->scale, 0.01f, 0.001f, 1000.0f, "%.3f");

                    drawImportErrorPopup();
                }
            };

            envWindow = UI::UIWindow{
                .name = "Environment",
                .drawCallback = [this]() {
                    if (ImGui::Button("Load Skybox")) {
                        loadSkybox();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Skybox")) {
                        renderer->getResources().destroyCubemap(environment.skyboxHandle);
                        environment.skyboxHandle = {};
                    }
                    ImGui::Spacing();
                    ImGui::ColorEdit3("Color", &environment.clearColor.x);

                    drawImportErrorPopup();
                }
            };

            lightWindow = UI::UIWindow{
                .name = "Light",
                .height = 175.0f,
                .drawCallback = [this]() {
                    ImGui::Checkbox("Enabled", &environment.directionalLight.enabled);
                    ImGui::Checkbox("Shadows", &environment.directionalLight.shadowsEnabled);
                    ImGui::Separator();
                    ImGui::BeginDisabled(!environment.directionalLight.enabled);
                    ImGui::DragFloat(
                        "Intensity",
                        &environment.directionalLight.intensity,
                        0.01f
                    );
                    ImGui::ColorEdit3("Color", &environment.directionalLight.color.x);
                    if (ImGui::SliderFloat("Yaw", &lightYaw, -180.0f, 180.0f, "%.1f°")) {
                        updateLightDirection();
                    }
                    if (ImGui::SliderFloat("Pitch", &lightPitch, -90.0f, 0.0f, "%.1f°")) {
                        updateLightDirection();
                    }
                    ImGui::EndDisabled();
                }
            };

            cameraWindow = UI::UIWindow{
                .name = "Camera",
                .width = 300.0f,
                .height = 250.0f,
                .drawCallback = [this]() {
                    ImGui::PushItemWidth(160.0f);

                    int controller = static_cast<int>(cameraControllerType);
                    if (ImGui::Combo("Controller", &controller, "Free Move\0Orbit\0")) {
                        setCameraController(static_cast<CameraControllerType>(controller));
                    }

                    if (cameraControllerType == CameraControllerType::FreeMove) {
                        auto& freeMove = static_cast<Controllers::FreeMoveController&>(*cameraController);
                        ImGui::DragFloat("Speed", &freeMove.speed, 0.1f, 0.1f, 100.0f);
                        ImGui::DragFloat(
                            "Shift Multiplier",
                            &freeMove.shiftMultiplier,
                            0.1f,
                            1.0f,
                            20.0f
                        );
                        ImGui::DragFloat("Look Sensitivity", &freeMove.sensitivity, 0.01f, 0.01f, 2.0f);
                    }
                    else {
                        auto& orbit = static_cast<Controllers::OrbitController&>(*cameraController);
                        ImGui::DragFloat("Max Radius", &orbit.maxRadius, 0.1f, 0.1f, 1000.0f);
                        ImGui::DragFloat("Orbit Sensitivity", &orbit.sensitivity, 0.01f, 0.01f, 2.0f);
                        ImGui::DragFloat("Zoom Sensitivity", &orbit.zoomSensitivity, 0.01f, 0.01f, 10.0f);
                    }

                    ImGui::Separator();
                    int projection = static_cast<int>(camera.projection);
                    if (ImGui::Combo("Projection", &projection, "Perspective\0Orthographic\0")) {
                        camera.projection = static_cast<Data::CameraProjection>(projection);
                    }

                    if (camera.projection == Data::CameraProjection::Perspective) {
                        ImGui::SliderFloat("Field of View", &camera.fov, 1.0f, 179.0f, "%.1f deg");
                    }
                    else {
                        ImGui::DragFloat("Size", &camera.orthographicSize, 0.1f, 0.1f, 1000.0f);
                    }
                    ImGui::DragFloat("Near", &camera.near, 0.01f, 0.001f, camera.far);
                    ImGui::DragFloat("Far", &camera.far, 0.1f, camera.near, 10000.0f);

                    ImGui::PopItemWidth();
                }
            };

            uiState.leftWindows = { &modelWindow, &envWindow, &lightWindow };
            uiState.rightWindows = { &debugWindow, &cameraWindow };
        }

        void setupRendering() {
            rhi = std::make_unique<Rendering::Vulkan::VulkanRenderer>(application);
            rhi->resources().createBaseShaders(baseShader);

            renderer = std::make_unique<Rendering::Renderer>(
                *rhi,
                *application.window
            );
        }

        void setupCamera() {
            camera.transform.position = glm::vec3(-4, 4, 4);
            camera.transform.eulerAngles = glm::vec3(-45.0f, -45.0f, 0.0f);
            camera.fov = 60;
            camera.near = 0.1f;
            camera.far = 150;
            setCameraController(CameraControllerType::FreeMove);
        }

        void setupLight() {
            environment.directionalLight = {
                .direction = glm::vec3(0.5f, -1.0f, 0.5f),
                .intensity = 5.0f,
                .color = glm::vec4(1.0f)
            };
        }

        void setCameraController(CameraControllerType type) {
            if (cameraController) {
                if (cameraControllerType == CameraControllerType::FreeMove) {
                    const auto& freeMove = static_cast<const Controllers::FreeMoveController&>(*cameraController);
                    freeMoveSpeed = freeMove.speed;
                    freeMoveShiftMultiplier = freeMove.shiftMultiplier;
                    freeMoveSensitivity = freeMove.sensitivity;
                }
                else {
                    const auto& orbit = static_cast<const Controllers::OrbitController&>(*cameraController);
                    orbitMaxRadius = orbit.maxRadius;
                    orbitSensitivity = orbit.sensitivity;
                    orbitZoomSensitivity = orbit.zoomSensitivity;
                }
            }

            cameraControllerType = type;
            if (type == CameraControllerType::FreeMove) {
                auto controller = std::make_unique<Controllers::FreeMoveController>(camera);
                controller->speed = freeMoveSpeed;
                controller->shiftMultiplier = freeMoveShiftMultiplier;
                controller->sensitivity = freeMoveSensitivity;
                cameraController = std::move(controller);
            }
            else {
                auto controller = std::make_unique<Controllers::OrbitController>(camera);
                controller->maxRadius = orbitMaxRadius;
                controller->sensitivity = orbitSensitivity;
                controller->zoomSensitivity = orbitZoomSensitivity;
                cameraController = std::move(controller);
            }
        }

        void loadScene(const SceneFactory& sceneFactory) {
            renderer->clearState();
            currentScene = sceneFactory(Loading::LoadContext{
                .resources = renderer->getResources(),
            });
        }

        void loadSkybox() {
            auto filePicker = App::createFilePicker();
            filePicker->requestFile({
                .title = "Load Skybox",
                .filters = {
                    { "Images", "*.png;*.jpg;*.jpeg;*.hdr;*.exr" },
                    { "All files", "*.*" }
                }
            });
            auto path = filePicker->pollResult();

            if (!path) {
                fmt::println("No file was selected");
                return;
            }

            fmt::println("Loading file {}", path->string());

            renderer->getResources().destroyCubemap(environment.skyboxHandle);
            environment.skyboxHandle = {};

            try {
                auto cubemap = tr::Loading::CubemapLoader::loadCubemap(path.value(), renderer->getResources());
                environment.skyboxHandle = renderer->getResources().createCubemap(*cubemap);
            }
            catch (const Loading::ImportError& error) {
                importError = error.what();
                fmt::println("Skybox import failed: {}", importError);
                importErrorPopupPending = true;
                showUI = true;
                syncSceneUI();
                return;
            }
        }

        void loadSelectedModel() {
            auto filePicker = App::createFilePicker();
            filePicker->requestFile({
                .title = "Load Model",
                .filters = {
                    { "3D models", "*.obj;*.gltf;*.glb;*.fbx" },
                    { "All files", "*.*" }
                }
            });
            auto path = filePicker->pollResult();

            if (!path) {
                fmt::println("No file was selected");
                return;
            }

            fmt::println("Loading file {}", path->string());
            renderer->clearState();
            Loading::LoadContext context{
                .resources = renderer->getResources()
            };
            try {
                currentScene = Loading::Loader::loadModel(context, *path);
            }
            catch (const Loading::ImportError& error) {
                importError = error.what();
                fmt::println("Model import failed: {}", importError);
                currentScene = Loading::Loader::loadDefaultScene(context);
                modelName = "Default Scene";
                importErrorPopupPending = true;
                showUI = true;
                syncSceneUI();
                return;
            }

            modelName = path->filename().string();
            syncSceneUI();
        }

        void drawImportErrorPopup() {
            if (importErrorPopupPending) {
                ImGui::OpenPopup("Import Error");
                importErrorPopupPending = false;
            }

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(
                viewport->GetCenter(),
                ImGuiCond_Appearing,
                ImVec2(0.5f, 0.5f)
            );
            ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal(
                "Import Error",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize
            )) {
                ImGui::TextWrapped("%s", importError.c_str());
                ImGui::Spacing();
                if (ImGui::Button("OK", ImVec2(100.0f, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void syncSceneUI() {
            const glm::vec3 direction = glm::normalize(environment.directionalLight.direction);
            lightYaw = glm::degrees(std::atan2(direction.x, direction.z));
            lightPitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
        }

        void updateLightDirection() {
            const float yaw = glm::radians(lightYaw);
            const float pitch = glm::radians(lightPitch);
            environment.directionalLight.direction = glm::vec3(
                std::cos(pitch) * std::sin(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::cos(yaw)
            );
        }

        void handleResize() {
            App::Window& window = *application.window;
            if (!window.wasResized()) return;

            rhi->resize(window.width(), window.height());
            window.clearResizedFlag();
            fmt::println("Window was resized to ({}, {}).", window.width(), window.height());
        }

        int run() {
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
                ImDrawData* uiDrawData = nullptr;
                if (showUI) {
                    rhi->prepareUI();
                    UI::UIFrame uiFrame = uiProvider.update(uiState);
                    uiDrawData = uiFrame.drawData;
                    canReadInput = !uiFrame.wantsMouse && !uiFrame.wantsKeyboard;
                }

                if (canReadInput) {
                    cameraController->update(camera, input, window.deltaTime());
                }

                if (window.width() != 0 && window.height() != 0) {
                    renderer->render(*currentScene, camera, environment, uiDrawData);
                }
            }

            fmt::println("Session end.");
            return 0;
        }
    };

    TinyRenderer::TinyRenderer() : _impl(std::make_unique<Impl>()) {}
    TinyRenderer::~TinyRenderer() = default;

    int TinyRenderer::run() {
        return _impl->run();
    }
}
