#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application.h"
#include "camera.h"
#include "camera_controller.h"
#include "environment.h"
#include "input.h"
#include "post_processing_data.h"
#include "renderer.h"
#include "rhi.h"
#include "scene.h"
#include "scene_builder.h"
#include "system_info.h"
#include "ui_data.h"
#include "ui_provider.h"

namespace tr {
    class TinyRenderer {
    private:
        enum class CameraControllerType {
            FreeMove,
            Orbit
        };

        static constexpr std::string_view APP_NAME = "tinyrenderer";
        static constexpr std::string_view APP_VERSION = "v0.0.1";

        using SceneLoader = void (*)(Loading::SceneBuilder&);

        App::Application application{
            .name = APP_NAME,
            .version = APP_VERSION,
            .window = std::make_unique<App::Window>(1280, 720, APP_NAME)
        };
        App::Input input{ *application.window };

        std::optional<App::WindowMode> pendingWindowMode;

        UI::UIProvider uiProvider{ *application.window, input };
        UI::UIState uiState;

        std::unique_ptr<Rendering::RHI> rhi;
        std::unique_ptr<Rendering::Renderer> renderer;
        
        std::unique_ptr<const Data::Scene> currentScene;
        float sceneScale = 1.0f;
        
        Data::Environment environment;
        Data::PostProcessingData postProcessingData;

        Data::Camera camera;
        std::unique_ptr<Controllers::CameraController> cameraController;

        std::string modelName;
        int selectedDemoScene = 0;

        std::string importError;

        float lightYaw = 0.0f;
        float lightPitch = 0.0f;

        float smoothedDeltaTime = 1.0f / 60.0f;
        std::vector<uint32_t> supportedMsaaSamples;
        glm::ivec2 pendingRenderSize{};
        std::string renderSizeError;
        std::optional<double> cpuFrameMilliseconds;
        std::optional<double> gpuFrameMilliseconds;
        std::string cpuName;
        std::string gpuName;
        std::optional<App::MemoryUsage> ramUsage;
        std::optional<App::MemoryUsage> gpuMemoryUsage;
        double memoryUsageRefreshSeconds = 0.5;
        bool gpuMemoryShared = false;

        CameraControllerType cameraControllerType = CameraControllerType::FreeMove;
        float freeMoveSpeed = 10.0f;
        float freeMoveShiftMultiplier = 2.5f;
        float freeMoveSensitivity = 0.1f;
        float orbitMaxRadius = 50.0f;
        float orbitSensitivity = 0.2f;
        float orbitZoomSensitivity = 0.75f;

        bool fpsInitialized = false;
        bool showUI = true;
        bool importErrorPopupPending = false;
        bool needToUpdateSceneMatrix = false;

        void setupUI();
        void drawModelSection();
        void drawLightSection();
        void drawEnvironmentSection();
        void drawInfoSection();
        void drawSettingsSection();
        void drawCameraSection();
        void drawPostProcessingSection();
        void drawImportErrorPopup();
        void syncSceneUI();
        void setupRendering();
        void setupCamera();
        void setupLight();
        void setCameraController(CameraControllerType type);
        void updateLightDirection();
        void setScene(std::unique_ptr<const Data::Scene> scene);
        void loadScene(SceneLoader load);
        void loadSkybox();
        void loadSelectedModel();
        void handleResize();

    public:
        TinyRenderer();
        ~TinyRenderer();

        TinyRenderer(const TinyRenderer&) = delete;
        TinyRenderer& operator=(const TinyRenderer&) = delete;
        TinyRenderer(TinyRenderer&&) = delete;
        TinyRenderer& operator=(TinyRenderer&&) = delete;

        int run();
    };
}
