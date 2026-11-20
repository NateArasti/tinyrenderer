#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "camera.h"
#include "demo_scenes.h"
#include "environment.h"
#include "post_processing_data.h"
#include "renderer.h"
#include "scene_builder.h"
#include "vulkan_test.h"

namespace tr::Tests {
    class DemoSceneTest : public VulkanTest {
    public:
        explicit DemoSceneTest(Loading::DemoScene demo) : _demo(demo) {}

    private:
        Loading::DemoScene _demo;
        std::unique_ptr<Rendering::Renderer> _renderer;
        std::unique_ptr<const Data::Scene> _scene;

        void TestBody() override;

        void SetUp() override {
            setUpVulkan("tinyrenderer demo smoke test", _demo.name);
            ASSERT_TRUE(_rhi->validationEnabled());
            _renderer = std::make_unique<Rendering::Renderer>(*_rhi);

            Loading::SceneBuilder builder(_renderer->getResources());
            _demo.build(builder);
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
            VulkanTest::TearDown();
        }
    };

    void DemoSceneTest::TestBody() {
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
                _rhi->setOutputSize(_application->window->width(), _application->window->height());
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

    void registerDemoSceneTests() {
        for (const auto& demo : Loading::getDemoScenes()) {
            const std::string name(demo.id);
            testing::RegisterTest("DemoScenes", name.c_str(), nullptr, nullptr, __FILE__, __LINE__, [demo] {
                return new DemoSceneTest(demo);
            });
        }
    }
}
