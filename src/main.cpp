#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <memory>
#include <fmt/base.h>
#include <glm/glm.hpp>

#include "application.h"
#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "handle.h"

#include "vulkan_renderer.h"

constexpr std::string_view APP_NAME = "tinyrenderer";

using namespace tr;
using namespace tr::App;
using namespace tr::Resources;
using namespace tr::Data;
using namespace tr::Rendering;

namespace {
    Handle<Shader> loadDefaultShader(Renderer& renderer) {
        auto shader = std::make_unique<Data::Shader>("base", "../shaders/base.spv");
        shader->vertName = "vertMain";
        shader->fragName = "fragMain";
        return renderer.upload(std::move(shader));
    }

    Handle<Material> createMaterial(Renderer& renderer, Handle<Shader> shader) {
        auto material = std::make_unique<Data::Material>(shader);
        material->name = "base";
        return renderer.upload(std::move(material));
    }

    Handle<Mesh> createCubeMesh(Renderer& renderer) {
        auto mesh = std::make_unique<Data::Mesh>();

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

        return renderer.upload(std::move(mesh));
    }

    Handle<Mesh> createPlaneMesh(Renderer& renderer) {
        auto mesh = std::make_unique<Data::Mesh>();
        mesh->vertices = {
            {.position = {-0.5f, 0, -0.5f}, .normal = {0, 1, 0}, .uv = {0, 0}, .color = {0.1f, 0.1f, 0.1f, 1} },
            {.position = { 0.5f, 0, -0.5f}, .normal = {0, 1, 0}, .uv = {1, 0}, .color = {0.1f, 0.1f, 0.1f, 1} },
            {.position = {-0.5f, 0,  0.5f}, .normal = {0, 1, 0}, .uv = {0, 1}, .color = {0.1f, 0.1f, 0.1f, 1} },
            {.position = { 0.5f, 0,  0.5f}, .normal = {0, 1, 0}, .uv = {1, 1}, .color = {0.1f, 0.1f, 0.1f, 1} },
        };
        
        mesh->indices.push_back(0);
        mesh->indices.push_back(1);
        mesh->indices.push_back(2);
        mesh->indices.push_back(1);
        mesh->indices.push_back(3);
        mesh->indices.push_back(2);

        mesh->subMeshData.push_back(6);

        return renderer.upload(std::move(mesh));
    }

    std::unique_ptr<Scene> createDefaultScene(Renderer& renderer) {
        fmt::println("Switching to default scene");

        renderer.clearState();

        Handle<Shader> baseShader = loadDefaultShader(renderer);
        Handle<Material> defaultMaterial = createMaterial(renderer, baseShader);
        Handle<Mesh> cubeMesh = createCubeMesh(renderer);
        Handle<Mesh> planeMesh = createPlaneMesh(renderer);

        auto scene = std::make_unique<Scene>();
        auto& objects = scene->getObjects();

        auto cube1 = std::make_unique<GameObject>();
        cube1->mesh = cubeMesh;
        cube1->materials.push_back(defaultMaterial);
        cube1->transform.position = glm::vec3(0.0f, 0.125f, 0.0f);
        cube1->transform.eulerAngles = glm::vec3(0.0f, 20.0f, 0.0f);
        cube1->transform.scale = glm::vec3(1.0f, 0.25f, 1.5f);
        objects.push_back(std::move(cube1));

        auto plane = std::make_unique<GameObject>();
        plane->mesh = planeMesh;
        plane->materials.push_back(defaultMaterial);
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
    
    auto rhi = std::make_unique<Vulkan::VulkanRenderer>(application);
    auto renderer = std::make_unique<Renderer>(*rhi);
    auto camera = std::make_unique<Camera>();
    camera->transform.position = glm::vec3(-4, 4, 4);
    camera->transform.eulerAngles = glm::vec3(-45.0f, -45.0f, 0.0f);
    camera->fov = 60;
    camera->near = 0.1f;
    camera->far = 50;

    auto scene = createDefaultScene(*renderer);

    while (!window->shouldClose()) {
        window->pollEvents();

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
