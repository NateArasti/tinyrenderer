#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <cmath>
#include <string>
#include <memory>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "application.h"
#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "handle.h"
#include "pbr.h"
#include "shadow.h"

#include "ui_provider.h"
#include "ui_data.h"
#include "file_picker.h"
#include "orbit_controller.h"
#include "free_move_controller.h"
#include "vulkan_renderer.h"

constexpr std::string_view APP_NAME = "tinyrenderer";

using namespace tr;
using namespace tr::App;
using namespace tr::Resources;
using namespace tr::Data;
using namespace tr::Rendering;
using namespace tr::Controllers;
using namespace tr::UI;

namespace {
    Handle<Material> createCubeMaterial(ResourceManager& resourceManager, Handle<Shader> shader) {
        auto material = std::make_unique<Material>(shader);
        material->name = "base";
        material->set("diffuseColor", glm::vec4(1, 1, 1, 1));
        return resourceManager.materialsPool.add(std::move(material));
    }

    Handle<Material> createPlaneMaterial( ResourceManager& resourceManager, Handle<Shader> shader) {
        auto material = std::make_unique<Material>(shader);
        material->name = "base";
        material->
            set("metallicFactor", 0.5f)
            .set("roughnessFactor", 0.5f);
        return resourceManager.materialsPool.add(std::move(material));
    }

    Handle<Mesh> createCubeMesh(ResourceManager& resourceManager) {
        auto mesh = std::make_unique<Mesh>();

        mesh->vertices = {
            // Bottom Face
            {.position = {-0.5f, -0.5f, -0.5f}, .normal = {0, -1, 0}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, -0.5f,  0.5f}, .normal = {0, -1, 0}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f, -0.5f}, .normal = {0, -1, 0}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f,  0.5f}, .normal = {0, -1, 0}, .uv = {1, 1}, .color = {1, 1, 1, 1} },

            // Top Face
            {.position = {-0.5f,  0.5f, -0.5f}, .normal = {0, 1, 0}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f,  0.5f, -0.5f}, .normal = {0, 1, 0}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f,  0.5f,  0.5f}, .normal = {0, 1, 0}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f,  0.5f,  0.5f}, .normal = {0, 1, 0}, .uv = {1, 1}, .color = {1, 1, 1, 1} },

            // Front Face
            {.position = {-0.5f,  0.5f,  0.5f}, .normal = {0, 0, 1}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f,  0.5f,  0.5f}, .normal = {0, 0, 1}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, -0.5f,  0.5f}, .normal = {0, 0, 1}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f,  0.5f}, .normal = {0, 0, 1}, .uv = {1, 1}, .color = {1, 1, 1, 1} },

            // Back Face
            {.position = { 0.5f,  0.5f, -0.5f}, .normal = {0, 0, -1}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f,  0.5f, -0.5f}, .normal = {0, 0, -1}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f, -0.5f}, .normal = {0, 0, -1}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, -0.5f, -0.5f}, .normal = {0, 0, -1}, .uv = {1, 1}, .color = {1, 1, 1, 1} },

            // Left Face
            {.position = {-0.5f,  0.5f,  0.5f}, .normal = {-1, 0, 0}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, -0.5f,  0.5f}, .normal = {-1, 0, 0}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f,  0.5f, -0.5f}, .normal = {-1, 0, 0}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, -0.5f, -0.5f}, .normal = {-1, 0, 0}, .uv = {1, 1}, .color = {1, 1, 1, 1} },

            // Right Face
            {.position = { 0.5f,  0.5f, -0.5f}, .normal = {1, 0, 0}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f, -0.5f}, .normal = {1, 0, 0}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f,  0.5f,  0.5f}, .normal = {1, 0, 0}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, -0.5f,  0.5f}, .normal = {1, 0, 0}, .uv = {1, 1}, .color = {1, 1, 1, 1} }
        };

        for (int i = 0; i < 24; i += 4) {
            mesh->indices.push_back(i);
            mesh->indices.push_back(i + 1);
            mesh->indices.push_back(i + 2);
            mesh->indices.push_back(i + 1);
            mesh->indices.push_back(i + 3);
            mesh->indices.push_back(i + 2);
        }

        mesh->subMeshData.push_back(36);

        return resourceManager.meshesPool.add(std::move(mesh));
    }

    Handle<Mesh> createPlaneMesh(ResourceManager& resourceManager) {
        auto mesh = std::make_unique<Data::Mesh>();
        mesh->vertices = {
            {.position = {-0.5f, 0, -0.5f}, .normal = {0, 1, 0}, .uv = {0, 0}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, 0, -0.5f}, .normal = {0, 1, 0}, .uv = {1, 0}, .color = {1, 1, 1, 1} },
            {.position = {-0.5f, 0,  0.5f}, .normal = {0, 1, 0}, .uv = {0, 1}, .color = {1, 1, 1, 1} },
            {.position = { 0.5f, 0,  0.5f}, .normal = {0, 1, 0}, .uv = {1, 1}, .color = {1, 1, 1, 1} },
        };
        
        mesh->indices.push_back(0);
        mesh->indices.push_back(1);
        mesh->indices.push_back(2);
        mesh->indices.push_back(1);
        mesh->indices.push_back(3);
        mesh->indices.push_back(2);

        mesh->subMeshData.push_back(6);

        return resourceManager.meshesPool.add(std::move(mesh));
    }

    std::unique_ptr<Scene> createDefaultScene(ResourceManager& resourceManager, Handle<Shader> baseShader) {
        fmt::println("Switching to default scene");

        Handle<Material> cubeMaterial = createCubeMaterial(resourceManager, baseShader);
        Handle<Material> floorMaterial = createPlaneMaterial(resourceManager, baseShader);
        Handle<Mesh> cubeMesh = createCubeMesh(resourceManager);
        Handle<Mesh> planeMesh = createPlaneMesh(resourceManager);

        auto scene = std::make_unique<Scene>();
        scene->directionalLight = {
            .direction = glm::vec3(0.5f, -1, 0.5f),
            .intensity = 1,
            .color = glm::vec4(1, 1, 1, 1),
        };

        auto& objects = scene->getObjects();

        auto cube1 = std::make_unique<GameObject>();
        cube1->mesh = cubeMesh;
        cube1->materials.push_back(cubeMaterial);
        cube1->transform.position = glm::vec3(0.0f, 0.25f, 0.0f);
        cube1->transform.eulerAngles = glm::vec3(0.0f, 20.0f, 0.0f);
        cube1->transform.scale = glm::vec3(1.0f, 0.25f, 1.5f);
        objects.push_back(std::move(cube1));

        auto plane = std::make_unique<GameObject>();
        plane->mesh = planeMesh;
        plane->materials.push_back(floorMaterial);
        plane->transform.position = glm::vec3(0, 0, 0);
        plane->transform.eulerAngles = glm::vec3(0, 0, 0);
        plane->transform.scale = glm::vec3(5, 5, 5);
        objects.push_back(std::move(plane));

        return scene;
    }

    std::unique_ptr<Scene> loadModel(ResourceManager& resourceManager, Handle<Shader> baseShader) {
        return std::make_unique<Scene>();
    }
}

struct TinyRenderer {
    std::unique_ptr<ResourceManager> resourceManager;
    std::unique_ptr<Renderer> renderer;

    std::unique_ptr<Scene> currentScene;

    using CreateScene = std::function<std::unique_ptr<Scene>(ResourceManager& resourceManager, Handle<Shader> baseShader)>;

    void calculateSceneBounds(tr::Data::Scene& scene, ResourceManager& resourceManager) {
        glm::vec3 min(std::numeric_limits<float>::max());
        glm::vec3 max(std::numeric_limits<float>::min());
        bool hasGeometry = false;

        for (const auto& object : scene.getObjects()) {
            auto* mesh = resourceManager.meshesPool.get(object->mesh);
            if (!mesh) continue;

            glm::mat4 model = object->transform.getMatrix();
            for (const auto& vertex : mesh->vertices) {
                glm::vec3 worldPos = glm::vec3(model * glm::vec4(vertex.position, 1.0f));
                min = glm::min(min, worldPos);
                max = glm::max(max, worldPos);
                hasGeometry = true;
            }
        }

        if (!hasGeometry) return;

        scene.sceneCenter = (min + max) * 0.5f;
        scene.sceneSize = max - min;
    }

    void setupScene(CreateScene sceneFactory) {
        resourceManager->clear();
        auto baseShader = resourceManager->shadersPool.add(std::make_unique<EmbeddedShaders::Pbr>());
        currentScene = sceneFactory(*resourceManager, baseShader);
        calculateSceneBounds(*currentScene, *resourceManager);
        renderer->reloadResources();
    }
};

int main() {
    TinyRenderer tr;

    Application application{
        .name = APP_NAME,
        .version = "v0.0.1",
        .window = std::make_unique<Window>(1280, 720, APP_NAME)
    };
    Window* window = application.window.get();
    Input input(*window);

    fmt::println("Starting {} with ({}, {}) window. Version={}.",
        application.name,
        window->width(),
        window->height(),
        application.version
    );
    
    UIProvider provider(*window, input);

    const std::string DEBUG_FPS = "DEBUG_FPS";
    const std::string DEBUG_GPU = "DEBUG_GPU";

    UIWindow debugWindow{
        .leftSide = false,
        .name = "Debug",
        .lines = {
            UILabel{ DEBUG_FPS, "FPS: 60" },
            UILabel{ DEBUG_GPU, "Some Rendering Device" },
        },
        .additionalFlags = ImGuiWindowFlags_NoInputs
    };

    const std::string MODEL_NAME = "MODEL_NAME";
    const std::string MODEL_LOAD = "MODEL_LOAD";
    const std::string MODEL_LOAD_DEFAULT = "MODEL_LOAD_DEFAULT";

    UIWindow modelWindow{
        .name = "Model",
        .lines = {
            UILabel{ MODEL_NAME, "Default Scene" },
            UISeparator{},
            UIButton{
                .key = MODEL_LOAD,
                .label = "Load Model",
            },
            UISameLine {},
            UIButton{
                .key = MODEL_LOAD_DEFAULT,
                .label = "Load Default Model",
            },
        },
    };
    
    const std::string LIGHT_INTENSITY = "LIGHT_INTENSITY";
    UIWindow lightWindow{
        .name = "Light",
        .lines = {
            UIProperty{
                .key = LIGHT_INTENSITY,
                .label = "Intensity",
                .value = 1.0f,
                .onChanged = [&tr](const UIValue& intensity) {
                    tr.currentScene->directionalLight.intensity = std::get<float>(intensity);
                }
            }
        },
    };

    modelWindow.get<UIButton>(MODEL_LOAD)->onClick = [&modelWindow, &MODEL_NAME, &tr, &lightWindow, &LIGHT_INTENSITY]() {
        std::unique_ptr<FilePicker> filePicker = createFilePicker();
        filePicker->requestModelFile();
        auto file = filePicker->pollResult();
        std::string name = "";
        if (file.has_value()) {
            name = file.value().name;
            fmt::println("Loading file {}", name);
        }
        else {
            fmt::println("Didn't load any file");
        }
        tr.setupScene(loadModel);
        modelWindow.setLabel(MODEL_NAME, name);
        lightWindow.get<UIProperty>(LIGHT_INTENSITY)->value = tr.currentScene->directionalLight.intensity;
    };
    modelWindow.get<UIButton>(MODEL_LOAD_DEFAULT)->onClick = [&modelWindow, &MODEL_NAME, &tr, &lightWindow, &LIGHT_INTENSITY]() {
        tr.setupScene(createDefaultScene);
        modelWindow.setLabel(MODEL_NAME, "Default Scene");
        lightWindow.get<UIProperty>(LIGHT_INTENSITY)->value = tr.currentScene->directionalLight.intensity;
    };

    UIState uiState{
        .windows = {
            &debugWindow,
            &modelWindow,
            &lightWindow
        }
    };

    tr.resourceManager = std::make_unique<ResourceManager>();

    auto rhi = std::make_unique<Vulkan::VulkanRenderer>(application);
    auto shadowShader = std::make_unique<EmbeddedShaders::Shadow>();
    rhi->createShadowShader(*shadowShader);
    tr.renderer = std::make_unique<Renderer>(rhi.get(), tr.resourceManager.get(), window);

    tr.setupScene(createDefaultScene);
    
    auto camera = std::make_unique<Camera>();
    camera->transform.position = glm::vec3(-4, 4, 4);
    camera->transform.eulerAngles = glm::vec3(-45.0f, -45.0f, 0.0f);
    camera->fov = 60;
    camera->near = 0.1f;
    camera->far = 50;
    std::unique_ptr<CameraController> cameraController = std::make_unique<FreeMoveController>(*camera);

    debugWindow.setLabel(DEBUG_GPU, rhi->getDeviceName());
    lightWindow.get<UIProperty>(LIGHT_INTENSITY)->value = tr.currentScene->directionalLight.intensity;

    bool showUI = true;
    bool canReadInput;

    while (!window->shouldClose()) {
        input.beginFrame();
        window->pollEvents();

        debugWindow.setLabel(DEBUG_FPS, fmt::format("FPS: {}", static_cast<int>(1.0f / window->deltaTime())));

        if (input.isKeyJustPressed(Key::F1)) {
            showUI = !showUI;
        }

        if (window->wasResized()) {
            rhi->resize(window->width(), window->height());
            window->clearResizedFlag();
            fmt::println("Window was resized to ({}, {}).", window->width(), window->height());
        }

        canReadInput = true;
        ImDrawData* uiDrawData = nullptr;

        if (showUI) {
            rhi->prepareUI();
            UIFrame uiFrame = provider.update(uiState);
            uiDrawData = uiFrame.drawData;
            canReadInput = !uiFrame.wantsMouse && !uiFrame.wantsKeyboard;
        }

        if (canReadInput) {
            cameraController->update(*camera, input, window->deltaTime());
        }

        if (window->width() != 0 && window->height() != 0) {
            tr.renderer->renderScene(*camera, *tr.currentScene, uiDrawData);
        }
    }

    fmt::println("Session end.");
}
