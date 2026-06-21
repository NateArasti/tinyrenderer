#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <memory>
#include <fmt/base.h>
#include <glm/glm.hpp>

#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "handle.h"

#include "vulkan_renderer.h"

constexpr std::string_view APP_NAME = "tinyrenderer";

using namespace tr;
using namespace tr::Resources;
using namespace tr::Data;
using namespace tr::Rendering;

namespace {
    Handle<Shader> loadDefaultShader(Renderer& renderer) {
        auto shader = std::make_unique<Data::Shader>();
        shader->name = "base";
        return renderer.upload(std::move(shader));
    }

    Handle<Material> createDefaultMaterial(Renderer& renderer, Handle<Shader> shader) {
        auto material = std::make_unique<Data::Material>(shader);
        material->name = "base";
        return renderer.upload(std::move(material));
    }

    Handle<Mesh> createCubeMesh(Renderer& renderer) {
        auto mesh = std::make_unique<Data::Mesh>();

        // code

        return renderer.upload(std::move(mesh));
    }

    Handle<Mesh> createPlaneMesh(Renderer& renderer) {
        auto mesh = std::make_unique<Data::Mesh>();

        // code

        return renderer.upload(std::move(mesh));
    }

    std::unique_ptr<Scene> createDefaultScene(Renderer& renderer) {
        fmt::println("Switching to default scene");

        renderer.clearState();

        Handle<Shader> shader = loadDefaultShader(renderer);
        Handle<Material> defaultMaterial = createDefaultMaterial(renderer, shader);
        Handle<Mesh> cubeMesh = createCubeMesh(renderer);
        Handle<Mesh> planeMesh = createPlaneMesh(renderer);

        auto scene = std::make_unique<Scene>();
        auto& objects = scene->getObjects();

        auto cube1 = std::make_unique<GameObject>();
        cube1->mesh = cubeMesh;
        cube1->materials.push_back(defaultMaterial);
        cube1->transform.position = glm::vec3(0, 1, 0);
        cube1->transform.eulerAngles = glm::vec3(0, 45, 0);
        cube1->transform.scale = glm::vec3(2, 1, 2);
        objects.push_back(std::move(cube1));

        auto plane = std::make_unique<GameObject>();
        plane->mesh = planeMesh;
        plane->materials.push_back(defaultMaterial);
        plane->transform.position = glm::vec3(0, 0, 0);
        plane->transform.eulerAngles = glm::vec3(0, 0, 0);
        plane->transform.scale = glm::vec3(1, 1, 1);
        objects.push_back(std::move(plane));

        return scene;
    }
}

int main() {
    fmt::println("Hello, this is {}!", APP_NAME);
    
    auto rhi = std::make_unique<Vulkan::VulkanRenderer>();
    auto renderer = std::make_unique<Renderer>(*rhi);
    auto camera = std::make_unique<Camera>();
    auto scene = createDefaultScene(*renderer);
    renderer->renderScene(*camera, *scene);
}
