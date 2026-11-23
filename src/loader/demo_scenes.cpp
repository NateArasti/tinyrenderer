#include "demo_scenes.h"

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

#include "mesh_primitives.h"
#include "scene_builder.h"

namespace tr::Loading {
    using namespace tr::Data;

    std::span<const DemoScene> getDemoScenes() {
        static constexpr std::array demos{
            DemoScene{ "simple_example", "Simple Example", createSimpleExampleScene },
            DemoScene{ "pbr_grid", "PBR Grid", createMetalRoughGridScene },
            DemoScene{ "3d_grid", "3D Grid", create3DGridScene },
            DemoScene{ "transparency", "Transparency", createTransparencyScene },
        };
        return demos;
    }

    void createSimpleExampleScene(SceneBuilder& builder) {
        const auto planeMesh = builder.registerMesh(Primitives::createPlaneMesh());
        const auto cubeMesh = builder.registerMesh(Primitives::createCubeMesh());
        const auto torusMesh = builder.registerMesh(Primitives::createTorusMesh());

        Material floorMaterial;
        floorMaterial.name = "floor";
        floorMaterial
            .set("diffuseColor", glm::vec4(0.25f, 0.25f, 0.25f, 1.0f))
            .set("metallicFactor", 0.0f)
            .set("roughnessFactor", 1.0f);
        const auto floor = builder.registerMaterial(floorMaterial);

        Material columnMaterial;
        columnMaterial.name = "column";
        columnMaterial
            .set("diffuseColor", glm::vec4(0.1f, 0.1f, 0.1f, 1.0f))
            .set("metallicFactor", 0.5f)
            .set("roughnessFactor", 0.5f);
        const auto columnMaterialHandle = builder.registerMaterial(columnMaterial);

        Material redMaterial;
        redMaterial.name = "red";
        redMaterial
            .set("diffuseColor", glm::vec4(1.0f, 0.1f, 0.1f, 1.0f))
            .set("metallicFactor", 0.5f)
            .set("roughnessFactor", 0.5f);
        const auto red = builder.registerMaterial(redMaterial);

        Material blueMaterial;
        blueMaterial.name = "blue";
        blueMaterial
            .set("diffuseColor", glm::vec4(0.1f, 0.1f, 1.0f, 1.0f))
            .set("metallicFactor", 1.0f)
            .set("roughnessFactor", 0.0f);
        const auto blue = builder.registerMaterial(blueMaterial);

        GameObject plane;
        plane.mesh = planeMesh;
        plane.materials.push_back(floor);
        plane.transform.scale = glm::vec3(10);
        builder.addObject(plane);

        GameObject cube;
        cube.mesh = cubeMesh;
        cube.materials.push_back(red);
        cube.transform.position = glm::vec3(0, 1.0f, 0);
        cube.transform.scale = glm::vec3(2);
        builder.addObject(cube);

        GameObject torus;
        torus.mesh = torusMesh;
        torus.materials.push_back(blue);
        torus.transform.position = glm::vec3(0, 3.25f, 0);
        torus.transform.eulerAngles = glm::vec3(-25, 45, 0);
        builder.addObject(torus);

        GameObject column;
        column.mesh = cubeMesh;
        column.materials.push_back(columnMaterialHandle);

        column.transform.position = glm::vec3(-3.5f, 1.5f, 2);
        column.transform.scale = glm::vec3(1, 3, 1);
        builder.addObject(column);
        
        column.transform.position = glm::vec3(3.75f, 2, -3.5f);
        column.transform.scale = glm::vec3(1, 4, 1);
        builder.addObject(column);
        
        column.transform.position = glm::vec3(-3.25f, 0.75f, -3);
        column.transform.scale = glm::vec3(0.5f, 1.5, 0.5f);
        builder.addObject(column);
        
        column.transform.position = glm::vec3(3.25, 2.5f, 3);
        column.transform.scale = glm::vec3(1.5f, 5, 1.5f);
        builder.addObject(column);
    }

    void createMetalRoughGridScene(SceneBuilder& builder) {
        constexpr uint32_t gridSize = 7;
        constexpr float spacing = 2.5f;
        constexpr float radius = 1.0f;
        constexpr float halfSpan = (gridSize - 1) * spacing * 0.5f;

        const auto sphereMesh = builder.registerMesh(Primitives::createSphereMesh());

        for (uint32_t y = 0; y < gridSize; ++y) {
            for (uint32_t x = 0; x < gridSize; ++x) {
                const float metallic = static_cast<float>(x) / (gridSize - 1);
                const float roughness = static_cast<float>(y) / (gridSize - 1);
                Material material;
                material.name = "sphere_m" + std::to_string(metallic) + "_r" + std::to_string(roughness);
                material
                    .set("diffuseColor", glm::vec4(0.5f, 0.0f, 0.0f, 1.0f))
                    .set("metallicFactor", metallic)
                    .set("roughnessFactor", roughness);

                GameObject sphere;
                sphere.mesh = sphereMesh;
                sphere.materials.push_back(builder.registerMaterial(material));
                sphere.transform.position = glm::vec3(
                    x * spacing - halfSpan, 0, y * spacing - halfSpan
                );
                sphere.transform.scale = glm::vec3(radius);
                builder.addObject(sphere);
            }
        }
    }

    void create3DGridScene(SceneBuilder& builder) {
        constexpr int size = 8;
        constexpr float cubeSize = 1;
        constexpr float padding = 0.1f;
        constexpr glm::vec3 offset = glm::vec3(0.5f * (size + padding * (size - 1)));
        
        const auto cubeMesh = builder.registerMesh(Primitives::createCubeMesh());
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                for (int z = 0; z < size; ++z) {
                    const float r = static_cast<float>(x) / (size - 1);
                    const float g = static_cast<float>(y) / (size - 1);
                    const float b = static_cast<float>(z) / (size - 1);
                    Material material;
                    material.name = "color_r" + std::to_string(r) + "_g" + std::to_string(g) + "_b" + std::to_string(b);
                    material.set("diffuseColor", glm::vec4(r, g, b, 1.0f));

                    GameObject cube;
                    cube.mesh = cubeMesh;
                    cube.transform.scale = glm::vec3(2);
                    cube.materials.push_back(builder.registerMaterial(material));
                    cube.transform.position = glm::vec3(
                        x + (cubeSize + padding) * (x - 1),
                        y + (cubeSize + padding) * (y - 1),
                        z + (cubeSize + padding) * (z - 1)
                    ) - offset;
                    cube.transform.scale = glm::vec3(cubeSize);
                    builder.addObject(cube);
                }
            }
        }
    }

    void createTransparencyScene(SceneBuilder& builder) {
        constexpr uint32_t wallCount = 64;
        constexpr float turns = 3.0f;
        constexpr float innerRadius = 2.0f;
        constexpr float outerRadius = 6.0f;
        constexpr float wallHeight = 3.0f;
        constexpr float wallThickness = 0.06f;
        constexpr float opacity = 0.28f;

        const auto planeMesh = builder.registerMesh(Primitives::createPlaneMesh());
        const auto cubeMesh = builder.registerMesh(Primitives::createCubeMesh());
        const auto torusMesh = builder.registerMesh(Primitives::createTorusMesh());

        Material floorMaterial;
        floorMaterial.name = "floor";
        floorMaterial
            .set("diffuseColor", glm::vec4(0.25f, 0.25f, 0.25f, 1.0f))
            .set("metallicFactor", 0.0f)
            .set("roughnessFactor", 1.0f);

        GameObject plane;
        plane.mesh = planeMesh;
        plane.materials.push_back(builder.registerMaterial(floorMaterial));
        plane.transform.scale = glm::vec3(14);
        builder.addObject(plane);

        Material torusMaterial;
        torusMaterial.name = "torus";
        torusMaterial.blendMode = BlendMode::Opaque;
        torusMaterial
            .set("diffuseColor", glm::vec4(0.9f, 0.9f, 0.95f, 1.0f))
            .set("metallicFactor", 0.75f)
            .set("roughnessFactor", 0.2f);

        GameObject torus;
        torus.mesh = torusMesh;
        torus.materials.push_back(builder.registerMaterial(torusMaterial));
        torus.transform.position = glm::vec3(0, wallHeight * 0.5f, 0);
        torus.transform.eulerAngles = glm::vec3(-25, 45, 0);
        torus.transform.scale = glm::vec3(1.5f);
        builder.addObject(torus);

        const std::array colors{
            glm::vec3(1.0f, 0.15f, 0.2f),
            glm::vec3(1.0f, 0.55f, 0.1f),
            glm::vec3(0.95f, 0.9f, 0.15f),
            glm::vec3(0.15f, 0.9f, 0.4f),
            glm::vec3(0.1f, 0.65f, 1.0f),
            glm::vec3(0.65f, 0.2f, 1.0f),
        };

        for (uint32_t i = 0; i < wallCount; ++i) {
            const float t0 = static_cast<float>(i) / wallCount;
            const float t1 = static_cast<float>(i + 1) / wallCount;
            const float angle0 = t0 * turns * 2.0f * std::numbers::pi_v<float>;
            const float angle1 = t1 * turns * 2.0f * std::numbers::pi_v<float>;
            const float radius0 = glm::mix(innerRadius, outerRadius, t0);
            const float radius1 = glm::mix(innerRadius, outerRadius, t1);
            const glm::vec3 start(radius0 * std::cos(angle0), wallHeight * 0.5f, radius0 * std::sin(angle0));
            const glm::vec3 end(radius1 * std::cos(angle1), wallHeight * 0.5f, radius1 * std::sin(angle1));
            const glm::vec3 direction = end - start;

            const float colorPosition = t0 * static_cast<float>(colors.size());
            const auto colorIndex = static_cast<size_t>(colorPosition);
            const glm::vec3 color = glm::mix(
                colors[colorIndex],
                colors[(colorIndex + 1) % colors.size()],
                colorPosition - static_cast<float>(colorIndex)
            );
            Material material;
            material.name = "wall_" + std::to_string(i);
            material.blendMode = BlendMode::Transparent;
            material
                .set("diffuseColor", glm::vec4(color, opacity))
                .set("metallicFactor", 0.0f)
                .set("roughnessFactor", 0.35f);

            GameObject wall;
            wall.mesh = cubeMesh;
            wall.materials.push_back(builder.registerMaterial(material));
            wall.transform.position = (start + end) * 0.5f;
            wall.transform.eulerAngles = glm::vec3(0, glm::degrees(std::atan2(-direction.z, direction.x)), 0);
            wall.transform.scale = glm::vec3(glm::length(direction), wallHeight, wallThickness);
            builder.addObject(wall);
        }
    }
}
