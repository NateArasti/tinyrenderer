#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include "tiny_renderer.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/glm.hpp>

#include "application.h"
#include "camera.h"
#include "camera_controller.h"
#include "file_picker.h"
#include "free_move_controller.h"
#include "input.h"
#include "pbr.h"
#include "renderer.h"
#include "resource_manager.h"
#include "loader.h"
#include "shadow.h"
#include "ui_data.h"
#include "ui_provider.h"
#include "vulkan_renderer.h"
#include "window.h"

namespace tr {
    namespace {
        constexpr std::string_view APP_NAME = "tinyrenderer";
        constexpr std::string_view APP_VERSION = "v0.0.1";

        constexpr std::string_view DEBUG_FPS = "DEBUG_FPS";
        constexpr std::string_view DEBUG_GPU = "DEBUG_GPU";

        constexpr std::string_view MODEL_NAME = "MODEL_NAME";
        constexpr std::string_view MODEL_LOAD = "MODEL_LOAD";
        constexpr std::string_view MODEL_LOAD_DEFAULT = "MODEL_LOAD_DEFAULT";

        constexpr std::string_view LIGHT_INTENSITY = "LIGHT_INTENSITY";
    }

    struct TinyRenderer::Impl {
        using SceneFactory = std::function<std::unique_ptr<Data::Scene>(
            Data::ResourceManager&,
            Resources::Handle<Data::Shader>
        )>;

        App::Application application{
            .name = APP_NAME,
            .version = APP_VERSION,
            .window = std::make_unique<App::Window>(1280, 720, APP_NAME)
        };
        App::Input input{ *application.window };
        UI::UIProvider uiProvider{ *application.window, input };

        Data::ResourceManager resourceManager;
        std::unique_ptr<Rendering::Vulkan::VulkanRenderer> rhi;
        std::unique_ptr<Rendering::Renderer> renderer;
        
        std::unique_ptr<Data::Scene> currentScene;

        Data::Camera camera;
        std::unique_ptr<Controllers::CameraController> cameraController;

        UI::UIState uiState;
        UI::UIWindow debugWindow;
        UI::UIWindow modelWindow;
        UI::UIWindow lightWindow;
        bool showUI = true;

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

            debugWindow.setLabel(DEBUG_GPU, rhi->getDeviceName());
            syncSceneUI();
        }

        void setupUI() {
            debugWindow = UI::UIWindow{
                .leftSide = false,
                .name = "Debug",
                .lines = {
                    UI::UILabel{std::string(DEBUG_FPS), "FPS: 60"},
                    UI::UILabel{std::string(DEBUG_GPU), "Some Rendering Device"},
                },
                .additionalFlags = ImGuiWindowFlags_NoInputs
            };

            modelWindow = UI::UIWindow{
                .name = "Model",
                .lines = {
                    UI::UILabel{std::string(MODEL_NAME), "Default Scene"},
                    UI::UISeparator{},
                    UI::UIButton{std::string(MODEL_LOAD), "Load Model"},
                    UI::UISameLine{},
                    UI::UIButton{std::string(MODEL_LOAD_DEFAULT), "Load Default Model"},
                },
                .additionalFlags = ImGuiWindowFlags_None
            };

            lightWindow = UI::UIWindow{
                .name = "Light",
                .lines = {
                    UI::UIProperty{
                        .key = std::string(LIGHT_INTENSITY),
                        .label = "Intensity",
                        .value = 1.0f,
                        .onChanged = [this](const UI::UIValue& intensity) {
                            currentScene->directionalLight.intensity = std::get<float>(intensity);
                        }
                    }
                },
                .additionalFlags = ImGuiWindowFlags_None
            };

            modelWindow.get<UI::UIButton>(MODEL_LOAD)->onClick = [this]() {
                loadSelectedModel();
            };
            modelWindow.get<UI::UIButton>(MODEL_LOAD_DEFAULT)->onClick = [this]() {
                loadScene(Loading::Loader::loadDefaultScene);
                modelWindow.setLabel(MODEL_NAME, "Default Scene");
                syncSceneUI();
            };

            uiState.windows = { &debugWindow, &modelWindow, &lightWindow };
        }

        void setupRendering() {
            rhi = std::make_unique<Rendering::Vulkan::VulkanRenderer>(application);

            Data::EmbeddedShaders::Shadow shadowShader;
            rhi->createShadowShader(shadowShader);

            renderer = std::make_unique<Rendering::Renderer>(
                *rhi,
                resourceManager,
                *application.window
            );
        }

        void setupCamera() {
            camera.transform.position = glm::vec3(-4, 4, 4);
            camera.transform.eulerAngles = glm::vec3(-45.0f, -45.0f, 0.0f);
            camera.fov = 60;
            camera.near = 0.1f;
            camera.far = 50;
            cameraController = std::make_unique<Controllers::FreeMoveController>(camera);
        }

        void loadScene(const SceneFactory& sceneFactory) {
            resourceManager.clear();
            const auto baseShader = resourceManager.shadersPool.add(
                std::make_unique<Data::EmbeddedShaders::Pbr>()
            );
            currentScene = sceneFactory(resourceManager, baseShader);
            renderer->reloadResources();
        }

        void loadSelectedModel() {
            auto filePicker = App::createFilePicker();
            filePicker->requestModelFile();
            auto file = filePicker->pollResult();

            if (!file) {
                fmt::println("Didn't load any file");
                return;
            }

            fmt::println("Loading file {}", file->name);
            loadScene([&file](Data::ResourceManager& resources, Resources::Handle<Data::Shader> shader) {
                return Loading::Loader::loadModel(
                    resources,
                    shader,
                    file->content
                );
            });
            modelWindow.setLabel(MODEL_NAME, file->name);
            syncSceneUI();
        }

        void syncSceneUI() {
            lightWindow.setProperty(
                LIGHT_INTENSITY,
                currentScene->directionalLight.intensity
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

                debugWindow.setLabel(
                    DEBUG_FPS,
                    fmt::format("FPS: {}", static_cast<int>(1.0f / window.deltaTime()))
                );

                if (input.isKeyJustPressed(App::Key::F1)) {
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
                    renderer->renderScene(camera, *currentScene, uiDrawData);
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
