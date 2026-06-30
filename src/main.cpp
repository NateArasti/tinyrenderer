#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <memory>
#include <fmt/base.h>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "application.h"
#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "handle.h"

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

namespace {
    Handle<Shader> loadDefaultShader(ResourceManager& resourceManager) {
        auto shader = std::make_unique<Shader>("base", "../shaders/pbr.spv");
        shader->vertName = "vertMain";
        shader->fragName = "fragMain";
        shader->cullMode = CullMode::None;
        shader->params = {
            {
                "diffuseColor",
                glm::vec4(1)
            },
            {
                "ambientColor",
                glm::vec4(0.03f, 0.03f, 0.03f, 1.0f)
            },
            {
                "specularColor",
                glm::vec4(0.5)
            },
            {
                "metallicFactor",
                0.0f
            },
            {
                "roughnessFactor",
                1.0f
            },
            {
                "albedo",
                Handle<Texture>{}
            }
        };
        return resourceManager.shadersPool.add(std::move(shader));
    }

    Handle<Texture> loadTexture(ResourceManager& resourceManager, const char* path) {
        auto texture = std::make_unique<Texture>();
        int texWidth, texHeight, texChannels;
        stbi_uc* raw = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!raw) {
            throw std::runtime_error(std::string("Failed to load texture: ") + stbi_failure_reason());
        }

        uint32_t forcedChannelCount = 4; // STBI_rgb_alpha forced
        texture->width = static_cast<uint32_t>(texWidth);
        texture->height = static_cast<uint32_t>(texHeight);
        texture->channels = forcedChannelCount;
        texture->pixels.assign(raw, raw + texWidth * texHeight * forcedChannelCount);

        stbi_image_free(raw);
        
        return resourceManager.texturesPool.add(std::move(texture));
    }

    Handle<Material> createCubeMaterial(ResourceManager& resourceManager, Handle<Shader> shader) {
        auto material = std::make_unique<Material>(shader);
        material->name = "base";
        material->set("diffuseColor", glm::vec4(1, 1, 1, 1));
        return resourceManager.materialsPool.add(std::move(material));
    }

    Handle<Material> createPlaneMaterial(
        ResourceManager& resourceManager,
        Handle<Shader> shader,
        Handle<Texture> texture
    ) {
        auto material = std::make_unique<Material>(shader);
        material->name = "base";
        material->
            set("albedo", texture)
            .set("metallicFactor", 0.5f)
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

    std::unique_ptr<Scene> createDefaultScene(ResourceManager& resourceManager) {
        fmt::println("Switching to default scene");

        Handle<Shader> baseShader = loadDefaultShader(resourceManager);
        Handle<Texture> texture = loadTexture(resourceManager, "../textures/texture.jpg");
        Handle<Material> cubeMaterial = createCubeMaterial(resourceManager, baseShader);
        Handle<Material> floorMaterial = createPlaneMaterial(resourceManager, baseShader, texture);
        Handle<Mesh> cubeMesh = createCubeMesh(resourceManager);
        Handle<Mesh> planeMesh = createPlaneMesh(resourceManager);

        auto scene = std::make_unique<Scene>();
        scene->directionalLight = {
            .direction = glm::vec3(-1, -1, -1),
            .intensity = 1,
            .color = glm::vec4(1, 0.8f, 0.8f, 1),
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
}

int main() {
    Application application{
        .name = APP_NAME,
        .version = "v0.0.1",
        .window = std::make_unique<Window>(1280, 720, APP_NAME)
    };
    Window* window = application.window.get();

    fmt::println("Starting {} with ({}, {}) window. Version={}.", application.name, window->width(), window->height(), application.version);
    
    auto resourceManager = std::make_unique<ResourceManager>();
    auto rhi = std::make_unique<Vulkan::VulkanRenderer>(application);
    auto renderer = std::make_unique<Renderer>(rhi.get(), resourceManager.get(), window);
    auto camera = std::make_unique<Camera>();
    camera->transform.position = glm::vec3(-4, 4, 4);
    camera->transform.eulerAngles = glm::vec3(-45.0f, -45.0f, 0.0f);
    camera->fov = 60;
    camera->near = 0.1f;
    camera->far = 50;

    resourceManager->clear();
    auto scene = createDefaultScene(*resourceManager);
    renderer->reloadResources();

    std::unique_ptr<CameraController> cameraController = std::make_unique<FreeMoveController>(*camera);

    while (!window->shouldClose()) {
        window->pollEvents();
        cameraController->update(*camera, *window);

        if (window->wasResized()) {
            rhi->resize(window->width(), window->height());
            window->clearResizedFlag();
            fmt::println("Window was resized to ({}, {}).", window->width(), window->height());
        }

        if (window->width() != 0 || window->height() != 0) {
            renderer->renderScene(*camera, *scene);
        }
    }

    fmt::println("Session end.");
}
