#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "application.h"
#include "camera.h"
#include "demo_scenes.h"
#include "environment.h"
#include "input.h"
#include "post_processing_data.h"
#include "renderer.h"
#include "scene_builder.h"
#include "ui_provider.h"
#include "vulkan_rhi.h"

namespace tr::Tests {
    class DemoSmokeTest : public testing::TestWithParam<Loading::DemoScene> {
    protected:
        std::mutex _validationMutex;
        std::vector<std::string> _validationMessages;
        std::unique_ptr<App::Application> _application;
        std::unique_ptr<App::Input> _input;
        std::unique_ptr<UI::UIProvider> _uiProvider;
        std::unique_ptr<Rendering::Vulkan::VulkanRHI> _rhi;
        std::unique_ptr<Rendering::Renderer> _renderer;
        std::unique_ptr<const Data::Scene> _scene;

        void SetUp() override {
            _application = std::make_unique<App::Application>(App::Application{
                .name = "tinyrenderer demo smoke test",
                .version = "0.0.1",
                .window = std::make_unique<App::Window>(640, 480, GetParam().name, false)
            });
            _input = std::make_unique<App::Input>(*_application->window);
            _uiProvider = std::make_unique<UI::UIProvider>(*_application->window, *_input);

            Rendering::Vulkan::ValidationOptions validation{
                .enabled = true,
                .synchronizationValidation = true,
                .onMessage = [this](auto, std::string_view message) {
                    std::lock_guard lock(_validationMutex);
                    _validationMessages.emplace_back(message);
                }
            };
            _rhi = std::make_unique<Rendering::Vulkan::VulkanRHI>(*_application, std::move(validation));
            ASSERT_TRUE(_rhi->validationEnabled());
            _renderer = std::make_unique<Rendering::Renderer>(*_rhi);

            Loading::SceneBuilder builder(_renderer->getResources());
            GetParam().build(builder);
            _scene = builder.build();
            ASSERT_FALSE(_scene->getObjects().empty());
            _renderer->prepareScene(*_scene);
            _renderer->updateSceneMatrix(*_scene, 1.0f);
        }

        void TearDown() override {
            if (_renderer && _scene) {
                _renderer->unloadSceneResources(*_scene);
            }
            _scene.reset();
            _renderer.reset();
            _rhi.reset();
            _uiProvider.reset();
            _input.reset();
            _application.reset();

            std::lock_guard lock(_validationMutex);
            for (const auto& message : _validationMessages) {
                ADD_FAILURE() << message;
            }
        }
    };

    TEST_P(DemoSmokeTest, LoadsMovesCameraAndCloses) {
        constexpr uint32_t frameCount = 24;
        const auto bounds = _scene->getBounds();
        const float radius = std::max(1.0f, glm::length(bounds.getSize()) * 0.5f);
        const glm::vec3 initialPosition = bounds.getCenter() + glm::vec3(0, radius, -2.0f * radius);
        const glm::vec3 initialRotation(26.5f, 0, 0);

        Data::Camera camera;
        camera.near = 0.1f;
        camera.far = std::max(150.0f, radius * 8.0f);
        Data::Environment environment;
        environment.directionalLight = {
            .direction = glm::vec3(0.5f, -1.0f, -0.5f),
            .intensity = 5.0f,
            .color = glm::vec4(1)
        };
        const Data::PostProcessingData postProcessing;

        uint32_t renderedFrames = 0;
        for (uint32_t attempt = 0; attempt < frameCount * 2 && renderedFrames < frameCount; ++attempt) {
            _input->beginFrame();
            _application->window->pollEvents();
            ASSERT_FALSE(_application->window->shouldClose());
            if (_application->window->wasResized()) {
                _rhi->resize(_application->window->width(), _application->window->height());
                _application->window->clearResizedFlag();
            }

            const float t = static_cast<float>(renderedFrames) / (frameCount - 1);
            camera.transform.position = initialPosition + glm::vec3(0.5f, 0.15f, 0.5f) * t;
            camera.transform.eulerAngles = initialRotation + glm::vec3(2, 4, 0) * t;
            if (_renderer->render(*_scene, camera, environment, postProcessing, nullptr, 1.0f)) {
                ++renderedFrames;
            }
        }
        EXPECT_EQ(renderedFrames, frameCount);
    }

    INSTANTIATE_TEST_SUITE_P(
        DemoScenes,
        DemoSmokeTest,
        testing::ValuesIn(Loading::getDemoScenes()),
        [](const testing::TestParamInfo<Loading::DemoScene>& info) {
            return std::string(info.param.id);
        }
    );
}
