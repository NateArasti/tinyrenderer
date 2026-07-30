#include "loader.h"

#include <limits>
#include <memory>

#include <glm/glm.hpp>

#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "rhi.h"

#include "gltf_importer.h"
#include "fbx_importer.h"
#include "obj_importer.h"

namespace tr::Loading {
    namespace {
        using namespace tr::Data;
        using namespace tr::Resources;

        struct UploadedMesh {
            Handle<Mesh> handle;
            MeshBounds bounds;
        };

        Handle<Material> createCubeMaterial(Scene& scene, LoadContext ctx) {
            auto material = std::make_unique<Material>();
            material->name = "base";
            material->set("diffuseColor", glm::vec4(1, 1, 1, 1));
            auto handle = scene.materials.add(std::move(material));
            ctx.renderingInterface.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        Handle<Material> createPlaneMaterial(tr::Data::Scene& scene, LoadContext ctx) {
            auto material = std::make_unique<Material>();
            material->name = "base";
            material->set("metallicFactor", 0.5f).set("roughnessFactor", 0.5f);
            auto handle = scene.materials.add(std::move(material));
            ctx.renderingInterface.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        UploadedMesh createCubeMesh(Scene& scene, LoadContext ctx) {
            Mesh mesh;
            mesh.vertices = {
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
                mesh.indices.insert(mesh.indices.end(), {
                    i, i + 1, i + 2,
                    i + 1, i + 3, i + 2
                });
            }
            mesh.subMeshData.push_back(36);
            const auto bounds = scene.registerMesh(mesh);
            return { ctx.renderingInterface.createMesh(mesh), bounds };
        }

        UploadedMesh createPlaneMesh(Scene& scene, LoadContext ctx) {
            Mesh mesh;
            mesh.vertices = {
                {{-0.5f, 0, -0.5f}, {0, 1, 0}, {0, 0}},
                {{ 0.5f, 0, -0.5f}, {0, 1, 0}, {1, 0}},
                {{-0.5f, 0,  0.5f}, {0, 1, 0}, {0, 1}},
                {{ 0.5f, 0,  0.5f}, {0, 1, 0}, {1, 1}}
            };
            mesh.indices = {0, 1, 2, 1, 3, 2};
            mesh.subMeshData.push_back(6);

            const auto bounds = scene.registerMesh(mesh);
            return { ctx.renderingInterface.createMesh(mesh), bounds };
        }
    }

    std::unique_ptr<Data::Scene> Loader::loadDefaultScene(LoadContext ctx) {
        auto scene = std::make_unique<Scene>();

        const auto cubeMaterial = createCubeMaterial(*scene, ctx);
        const auto floorMaterial = createPlaneMaterial(*scene, ctx);
        const auto cubeMesh = createCubeMesh(*scene, ctx);
        const auto planeMesh = createPlaneMesh(*scene, ctx);

        auto cube = std::make_unique<Data::GameObject>();
        cube->mesh = cubeMesh.handle;
        cube->materials.push_back(cubeMaterial);
        cube->transform.position = glm::vec3(0.0f, 0.25f, 0.0f);
        cube->transform.eulerAngles = glm::vec3(0.0f, 20.0f, 0.0f);
        cube->transform.scale = glm::vec3(1.0f, 0.25f, 1.5f);
        scene->includeBounds(cubeMesh.bounds, cube->transform.getMatrix());
        scene->getObjects().push_back(std::move(cube));

        auto plane = std::make_unique<Data::GameObject>();
        plane->mesh = planeMesh.handle;
        plane->materials.push_back(floorMaterial);
        plane->transform.scale = glm::vec3(5.0f);
        scene->includeBounds(planeMesh.bounds, plane->transform.getMatrix());
        scene->getObjects().push_back(std::move(plane));

        return scene;
    }

    std::unique_ptr<Data::Scene> Loader::loadModel(
        LoadContext ctx,
        const std::filesystem::path& path
    ) {
        std::vector<std::unique_ptr<Importer>> importers;
        importers.push_back(std::make_unique<OBJImporter>());
        importers.push_back(std::make_unique<GLTFImporter>());
        importers.push_back(std::make_unique<FBXImporter>());

        auto scene = std::make_unique<Scene>();
        for (const auto& importer : importers) {
            importer->load(*scene, ctx, path);
        }

        return scene;
    }
}
