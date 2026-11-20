#include <memory>
#include <stdexcept>

#include "demo_scenes.h"
#include "renderer.h"
#include "scene_builder.h"
#include "vulkan_test.h"

namespace tr::Tests {
    class SettingsChange : public VulkanTest {
    private:
        std::unique_ptr<Rendering::Renderer> _renderer;
        std::unique_ptr<const Data::Scene> _scene;
        Data::Camera _camera;
        Data::Environment _environment;

    protected:
        void SetUp() override {
            setUpVulkan("tinyrenderer settings change test", "Settings change");
            ASSERT_TRUE(_rhi->validationEnabled());

            _renderer = std::make_unique<Rendering::Renderer>(*_rhi);
            Loading::SceneBuilder builder(_renderer->getResources());
            Loading::createSimpleExampleScene(builder);
            _scene = builder.build();
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

        bool renderFrame() {
            for (int attempt = 0; attempt < 4; ++attempt) {
                _application->window->pollEvents();
                if (_renderer->render(*_scene, _camera, _environment, {}, nullptr, 1.0f)) {
                    return true;
                }
            }
            return false;
        }
    };

    TEST_F(SettingsChange, RenderSize) {
        ASSERT_TRUE(renderFrame());

        _rhi->setRenderSize(320, 180);
        EXPECT_EQ(_rhi->getRenderSize(), glm::uvec2(320, 180));
        ASSERT_TRUE(renderFrame());

        // A fitted viewport for this size would round to zero pixels wide without a minimum.
        _rhi->setRenderSize(1, 2048);
        EXPECT_EQ(_rhi->getRenderSize(), glm::uvec2(1, 2048));
        ASSERT_TRUE(renderFrame());

        EXPECT_THROW(_rhi->setRenderSize(0, 1), std::invalid_argument);
        EXPECT_THROW(_rhi->setRenderSize(1, 0), std::invalid_argument);
        EXPECT_EQ(_rhi->getRenderSize(), glm::uvec2(1, 2048));
        EXPECT_TRUE(renderFrame());
    }

    TEST_F(SettingsChange, VSync) {
        ASSERT_TRUE(renderFrame());
        const bool canDisable = _rhi->supportsVsyncToggle();

        _rhi->setVsyncEnabled(false);
        ASSERT_TRUE(renderFrame());
        EXPECT_EQ(_rhi->getVsyncEnabled(), !canDisable);

        _rhi->setVsyncEnabled(true);
        ASSERT_TRUE(renderFrame());
        EXPECT_TRUE(_rhi->getVsyncEnabled());
    }

    TEST_F(SettingsChange, MSAA) {
        ASSERT_TRUE(renderFrame());
        for (const auto samples : _rhi->getSupportedMsaaSamples()) {
            SCOPED_TRACE(samples);
            _rhi->setMsaaSamples(samples);
            EXPECT_EQ(_rhi->getMsaaSamples(), samples);
            ASSERT_TRUE(renderFrame());
        }
    }
}
