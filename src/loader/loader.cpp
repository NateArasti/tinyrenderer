#include "loader.h"

#include <limits>
#include <memory>

#include <glm/glm.hpp>

#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "resource_manager.h"

#include "obj_importer.h"

namespace tr::Loading {
    namespace {
        using namespace tr::Data;
        using namespace tr::Resources;

        Handle<Material> createCubeMaterial(ResourceManager& resourceManager, Handle<Shader> shader) {
            auto material = std::make_unique<Material>(shader);
            material->name = "base";
            material->set("diffuseColor", glm::vec4(1, 1, 1, 1));
            return resourceManager.materialsPool.add(std::move(material));
        }

        Handle<Material> createPlaneMaterial(ResourceManager& resourceManager, Handle<Shader> shader) {
            auto material = std::make_unique<Material>(shader);
            material->name = "base";
            material->set("metallicFactor", 0.5f).set("roughnessFactor", 0.5f);
            return resourceManager.materialsPool.add(std::move(material));
        }

        Handle<Mesh> createCubeMesh(ResourceManager& resourceManager) {
            auto mesh = std::make_unique<Mesh>();
            mesh->vertices = {
                {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 0}},
                {{-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 1}},
                {{ 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0}},
                {{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 1}},

                {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
                {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 0}},
                {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 1}},
                {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 1}},

                {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 0}},
                {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 0}},
                {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 1}},
                {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 1}},

                {{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
                {{-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
                {{ 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
                {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},

                {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {0, 0}},
                {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {0, 1}},
                {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {1, 0}},
                {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {1, 1}},

                {{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {0, 0}},
                {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, 1}},
                {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {1, 0}},
                {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {1, 1}}
            };

            for (uint32_t i = 0; i < 24; i += 4) {
                mesh->indices.insert(mesh->indices.end(), {
                    i, i + 1, i + 2,
                    i + 1, i + 3, i + 2
                });
            }
            mesh->subMeshData.push_back(36);
            return resourceManager.meshesPool.add(std::move(mesh));
        }

        Handle<Mesh> createPlaneMesh(ResourceManager& resourceManager) {
            auto mesh = std::make_unique<Mesh>();
            mesh->vertices = {
                {{-0.5f, 0, -0.5f}, {0, 1, 0}, {0, 0}},
                {{ 0.5f, 0, -0.5f}, {0, 1, 0}, {1, 0}},
                {{-0.5f, 0,  0.5f}, {0, 1, 0}, {0, 1}},
                {{ 0.5f, 0,  0.5f}, {0, 1, 0}, {1, 1}}
            };
            mesh->indices = {0, 1, 2, 1, 3, 2};
            mesh->subMeshData.push_back(6);
            return resourceManager.meshesPool.add(std::move(mesh));
        }

        void calculateBounds(Scene& scene, ResourceManager& resourceManager) {
            glm::vec3 min(std::numeric_limits<float>::max());
            glm::vec3 max(std::numeric_limits<float>::lowest());
            bool hasGeometry = false;

            for (const auto& object : scene.getObjects()) {
                const Mesh* mesh = resourceManager.meshesPool.get(object->mesh);
                if (!mesh) continue;

                const glm::mat4 model = object->transform.getMatrix();
                for (const auto& vertex : mesh->vertices) {
                    const glm::vec3 worldPosition = model * glm::vec4(vertex.position, 1.0f);
                    min = glm::min(min, worldPosition);
                    max = glm::max(max, worldPosition);
                    hasGeometry = true;
                }
            }

            if (!hasGeometry) {
                scene.sceneCenter = glm::vec3(0.0f);
                scene.sceneSize = glm::vec3(0.0f);
                return;
            }

            scene.sceneCenter = (min + max) * 0.5f;
            scene.sceneSize = max - min;
        }
    }

    std::unique_ptr<Data::Scene> Loader::loadDefaultScene(LoadContext ctx) {
        auto scene = std::make_unique<Scene>();

        const auto cubeMaterial = createCubeMaterial(ctx.resourceManager, ctx.baseOpaqueShader);
        const auto floorMaterial = createPlaneMaterial(ctx.resourceManager, ctx.baseOpaqueShader);
        const auto cubeMesh = createCubeMesh(ctx.resourceManager);
        const auto planeMesh = createPlaneMesh(ctx.resourceManager);

        auto cube = std::make_unique<Data::GameObject>();
        cube->mesh = cubeMesh;
        cube->materials.push_back(cubeMaterial);
        cube->transform.position = glm::vec3(0.0f, 0.25f, 0.0f);
        cube->transform.eulerAngles = glm::vec3(0.0f, 20.0f, 0.0f);
        cube->transform.scale = glm::vec3(1.0f, 0.25f, 1.5f);
        scene->getObjects().push_back(std::move(cube));

        auto plane = std::make_unique<Data::GameObject>();
        plane->mesh = planeMesh;
        plane->materials.push_back(floorMaterial);
        plane->transform.scale = glm::vec3(5.0f);
        scene->getObjects().push_back(std::move(plane));

        calculateBounds(*scene, ctx.resourceManager);
        return scene;
    }

    std::unique_ptr<Data::Scene> Loader::loadModel(
        LoadContext ctx,
        const std::filesystem::path& path
    ) {
        std::vector<std::unique_ptr<Importer>> importers;
        importers.push_back(std::make_unique<OBJImporter>());

        auto scene = std::make_unique<Scene>();
        for (const auto& importer : importers) {
            importer->load(*scene, ctx, path);
        }
        calculateBounds(*scene, ctx.resourceManager);

        return scene;
    }
}
