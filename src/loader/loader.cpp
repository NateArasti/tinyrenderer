#include "loader.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <string>

#include <glm/glm.hpp>

#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "rhi.h"

#include "gltf_importer.h"
#include "fbx_importer.h"
#include "obj_importer.h"

#include "import_error.h"

namespace tr::Loading {
    namespace {
        using namespace tr::Data;
        using namespace tr::Resources;

        struct UploadedMesh {
            Handle<Mesh> handle;
            MeshBounds bounds;
        };

        Handle<Material> createSphereMaterial(Scene& scene, LoadContext ctx, float metallic, float roughness) {
            auto material = std::make_unique<Material>();
            material->name = "sphere_m" + std::to_string(metallic) + "_r" + std::to_string(roughness);
            material->set("diffuseColor", glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));
            material->set("metallicFactor", metallic).set("roughnessFactor", roughness);
            auto handle = scene.materials.add(std::move(material));
            ctx.resources.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        Handle<Material> createPlaneMaterial(Scene& scene, LoadContext ctx) {
            auto material = std::make_unique<Material>();
            material->name = "floor";
            material->set("diffuseColor", glm::vec4(0.25f, 0.25f, 0.25f, 1.0f));
            material->set("metallicFactor", 0.0f).set("roughnessFactor", 1.0f);
            auto handle = scene.materials.add(std::move(material));
            ctx.resources.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        UploadedMesh createSphereMesh(Scene& scene, LoadContext ctx) {
            constexpr uint32_t segments = 64;
            constexpr uint32_t rings = 32;
            Mesh mesh;
            mesh.vertices.reserve((rings + 1) * (segments + 1));
            mesh.indices.reserve(6 * segments * (rings - 1));

            for (uint32_t ring = 0; ring <= rings; ++ring) {
                const float v = static_cast<float>(ring) / rings;
                const float theta = v * std::numbers::pi_v<float>;
                for (uint32_t segment = 0; segment <= segments; ++segment) {
                    const float u = static_cast<float>(segment) / segments;
                    const float phi = u * 2.0f * std::numbers::pi_v<float>;
                    const glm::vec3 normal(
                        std::sin(theta) * std::cos(phi),
                        std::cos(theta),
                        -std::sin(theta) * std::sin(phi)
                    );
                    mesh.vertices.push_back({ normal, normal, { u, v } });
                }
            }

            for (uint32_t ring = 0; ring < rings; ++ring) {
                for (uint32_t segment = 0; segment < segments; ++segment) {
                    const uint32_t a = ring * (segments + 1) + segment;
                    const uint32_t b = a + segments + 1;
                    if (ring != 0) {
                        mesh.indices.insert(mesh.indices.end(), { a, a + 1, b });
                    }
                    if (ring != rings - 1) {
                        mesh.indices.insert(mesh.indices.end(), { a + 1, b + 1, b });
                    }
                }
            }
            mesh.subMeshData.push_back(static_cast<uint32_t>(mesh.indices.size()));
            const auto bounds = scene.registerMesh(mesh);
            return { ctx.resources.createMesh(mesh), bounds };
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
            return { ctx.resources.createMesh(mesh), bounds };
        }
    }

    std::unique_ptr<Data::Scene> Loader::loadDefaultScene(LoadContext ctx) {
        auto scene = std::make_unique<Scene>();

        constexpr uint32_t gridSize = 7;
        constexpr float spacing = 2.5f;
        constexpr float radius = 1.0f;
        constexpr float halfSpan = (gridSize - 1) * spacing * 0.5f;
        const auto floorMaterial = createPlaneMaterial(*scene, ctx);
        const auto sphereMesh = createSphereMesh(*scene, ctx);
        const auto planeMesh = createPlaneMesh(*scene, ctx);

        for (uint32_t y = 0; y < gridSize; ++y) {
            for (uint32_t x = 0; x < gridSize; ++x) {
                auto sphere = std::make_unique<Data::GameObject>();
                sphere->mesh = sphereMesh.handle;
                sphere->materials.push_back(createSphereMaterial(
                    *scene, ctx,
                    static_cast<float>(x) / (gridSize - 1),
                    static_cast<float>(y) / (gridSize - 1)
                ));
                // Grid Y runs along world Z; world Y is height.
                sphere->transform.position = glm::vec3(
                    x * spacing - halfSpan, radius, y * spacing - halfSpan
                );
                sphere->transform.scale = glm::vec3(radius);
                scene->includeBounds(sphereMesh.bounds, sphere->transform.getMatrix());
                scene->getObjects().push_back(std::move(sphere));
            }
        }

        auto plane = std::make_unique<Data::GameObject>();
        plane->mesh = planeMesh.handle;
        plane->materials.push_back(floorMaterial);
        plane->transform.scale = glm::vec3(gridSize * spacing + 2.0f);
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

        if (scene->getObjects().empty()) {
            throw ImportError(
                "The file contains no renderable meshes or its format is unsupported."
            );
        }

        return scene;
    }
}
