#include "obj_importer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/base.h>
#include <glm/glm.hpp>
#include "rapidobj/rapidobj.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "rhi.h"

namespace tr::Loading {
    namespace {
        using namespace tr::Data;
        using namespace tr::Resources;

        struct VertexKey {
            int position;
            int texcoord;
            int normal;
            uint32_t smoothingGroup;
            size_t face;

            bool operator==(const VertexKey&) const = default;
        };

        struct VertexKeyHash {
            size_t operator()(const VertexKey& key) const noexcept {
                size_t result = 0;
                const auto combine = [&result](size_t value) {
                    result ^= value + 0x9e3779b9 + (result << 6) + (result >> 2);
                };
                combine(std::hash<int>{}(key.position));
                combine(std::hash<int>{}(key.texcoord));
                combine(std::hash<int>{}(key.normal));
                combine(std::hash<uint32_t>{}(key.smoothingGroup));
                combine(std::hash<size_t>{}(key.face));
                return result;
            }
        };

        bool equalsIgnoreCase(std::string_view a, std::string_view b) {
            return std::ranges::equal(
                a,
                b,
                [](char lhs, char rhs) {
                    return std::tolower(static_cast<unsigned char>(lhs))
                        == std::tolower(static_cast<unsigned char>(rhs));
                }
            );
        }

        Handle<Texture> loadTexture(
            LoadContext ctx,
            const std::filesystem::path& modelDirectory,
            std::string_view textureName,
            TextureColorSpace colorSpace,
            std::unordered_map<std::string, Handle<Texture>>& cache
        ) {
            if (textureName.empty()) {
                return {};
            }

            const auto texturePath = (modelDirectory / std::filesystem::path(textureName)).lexically_normal();
            std::string cacheKey = texturePath.generic_string();
            std::ranges::transform(cacheKey, cacheKey.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            cacheKey += colorSpace == TextureColorSpace::SRGB ? "#srgb" : "#linear";
            if (const auto found = cache.find(cacheKey); found != cache.end()) {
                return found->second;
            }

            std::ifstream input(texturePath, std::ios::binary | std::ios::ate);
            if (!input) {
                fmt::println("Couldn't open texture file: {}", texturePath.string());
                return {};
            }
            const auto size = input.tellg();
            if (size < 0 || size > std::numeric_limits<int>::max()) {
                fmt::println("Texture file is too large: {}", texturePath.string());
                return {};
            }
            std::vector<std::byte> content(static_cast<size_t>(size));
            input.seekg(0, std::ios::beg);
            input.read(
                reinterpret_cast<char*>(content.data()),
                static_cast<std::streamsize>(content.size())
            );
            if (!input && !content.empty()) {
                fmt::println("Couldn't read texture file: {}", texturePath.string());
                return {};
            }

            int width = 0;
            int height = 0;
            stbi_uc* decoded = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(content.data()),
                static_cast<int>(content.size()),
                &width,
                &height,
                nullptr,
                STBI_rgb_alpha
            );
            if (!decoded || width <= 0 || height <= 0) {
                fmt::println(
                    "Couldn't decode texture {}: {}",
                    texturePath.string(),
                    stbi_failure_reason() ? stbi_failure_reason() : "unknown error"
                );
                stbi_image_free(decoded);
                return {};
            }

            const size_t rowSize = static_cast<size_t>(width) * STBI_rgb_alpha;
            const size_t pixelDataSize = rowSize * static_cast<size_t>(height);
            Texture texture;
            texture.name = std::filesystem::path(textureName).filename().string();
            texture.width = static_cast<uint32_t>(width);
            texture.height = static_cast<uint32_t>(height);
            texture.channels = STBI_rgb_alpha;
            texture.colorSpace = colorSpace;
            texture.pixels.assign(decoded, decoded + pixelDataSize);
            stbi_image_free(decoded);

            for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
                const auto topBegin = texture.pixels.begin() + static_cast<size_t>(top) * rowSize;
                const auto bottomBegin = texture.pixels.begin() + static_cast<size_t>(bottom) * rowSize;
                std::swap_ranges(topBegin, topBegin + rowSize, bottomBegin);
            }

            const Handle<Texture> handle = ctx.resources.createTexture(texture);
            cache.emplace(std::move(cacheKey), handle);
            return handle;
        }

        Handle<Material> createDefaultMaterial(Scene& scene, LoadContext ctx) {
            auto material = std::make_unique<Material>();
            material->name = "default";
            material->set("diffuseColor", glm::vec4(1.0f));
            auto handle = scene.materials.add(std::move(material));
            ctx.resources.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        std::vector<Handle<Material>> createMaterials(
            Scene& scene,
            LoadContext& ctx,
            const std::filesystem::path& modelDirectory,
            const rapidobj::Materials& sourceMaterials
        ) {
            std::vector<Handle<Material>> materials;
            materials.reserve(sourceMaterials.size());
            std::unordered_map<std::string, Handle<Texture>> textureCache;

            for (const auto& source : sourceMaterials) {
                const bool transparent = source.dissolve < 1.0f || !source.alpha_texname.empty();
                auto material = std::make_unique<Material>();
                material->blendMode = transparent ? BlendMode::Transparent : BlendMode::Opaque;
                material->name = source.name;

                const auto loadMap = [&](
                    std::string_view textureName,
                    std::string_view mapParam,
                    std::string_view flagParam,
                    TextureColorSpace colorSpace
                ) {
                    const auto texture = loadTexture(
                        ctx,
                        modelDirectory,
                        textureName,
                        colorSpace,
                        textureCache
                    );
                    if (texture.isValid()) {
                        material->set(std::string(mapParam), texture);
                        material->set(std::string(flagParam), 1.0f);
                    }
                    return texture.isValid();
                };

                const bool hasDiffuseMap = loadMap(
                    source.diffuse_texname,
                    "diffuseMap",
                    "hasDiffuseMap",
                    TextureColorSpace::SRGB
                );
                const bool hasEmissionMap = loadMap(
                    source.emissive_texname,
                    "emissionMap",
                    "hasEmissionMap",
                    TextureColorSpace::SRGB
                );
                const std::string_view normalTexture = source.normal_texname.empty()
                    ? std::string_view(source.bump_texname)
                    : std::string_view(source.normal_texname);
                loadMap(
                    normalTexture,
                    "normalMap",
                    "hasNormalMap",
                    TextureColorSpace::Linear
                );
                loadMap(
                    source.metallic_texname,
                    "metallicMap",
                    "hasMetallicMap",
                    TextureColorSpace::Linear
                );
                loadMap(
                    source.roughness_texname,
                    "roughnessMap",
                    "hasRoughnessMap",
                    TextureColorSpace::Linear
                );
                loadMap(
                    source.specular_texname,
                    "specularMap",
                    "hasSpecularMap",
                    TextureColorSpace::SRGB
                );
                loadMap(
                    source.alpha_texname,
                    "opacityMap",
                    "hasOpacityMap",
                    TextureColorSpace::Linear
                );

                const bool hasDiffuseColor = source.diffuse[0] != 0.0f
                    || source.diffuse[1] != 0.0f
                    || source.diffuse[2] != 0.0f;
                const glm::vec3 diffuse = hasDiffuseMap && !hasDiffuseColor
                    ? glm::vec3(1.0f)
                    : glm::vec3(source.diffuse[0], source.diffuse[1], source.diffuse[2]);
                material->set(
                    "diffuseColor",
                    glm::vec4(diffuse, source.dissolve)
                );
                material->set(
                    "ambientColor",
                    glm::vec4(source.ambient[0], source.ambient[1], source.ambient[2], 1.0f)
                );

                const bool hasEmissionColor = source.emission[0] != 0.0f
                    || source.emission[1] != 0.0f
                    || source.emission[2] != 0.0f;
                const glm::vec3 emission = hasEmissionMap && !hasEmissionColor
                    ? glm::vec3(1.0f)
                    : glm::vec3(source.emission[0], source.emission[1], source.emission[2]);
                material->set("emissionColor", glm::vec4(emission, 1.0f));

                const bool hasSpecularColor = source.specular[0] != 0.0f
                    || source.specular[1] != 0.0f
                    || source.specular[2] != 0.0f;
                const glm::vec3 specular = hasSpecularColor
                    ? glm::vec3(source.specular[0], source.specular[1], source.specular[2])
                    : glm::vec3(0.04f);
                material->set("specularColor", glm::vec4(specular, 1.0f));
                material->set("metallicFactor", std::clamp(source.metallic, 0.0f, 1.0f));
                material->set("normalScale", source.bump_texopt.bump_multiplier);

                const float legacyRoughness = std::sqrt(2.0f / (std::max(source.shininess, 0.0f) + 2.0f));
                const float roughness = source.roughness > 0.0f ? source.roughness : legacyRoughness;
                material->set("roughnessFactor", std::clamp(roughness, 0.04f, 1.0f));

                auto handle = scene.materials.add(std::move(material));
                ctx.resources.registerMaterial(handle, *scene.materials.get(handle));
                materials.push_back(handle);
            }
            return materials;
        }

        Mesh::Vertex makeVertex(
            const rapidobj::Attributes& attributes,
            const rapidobj::Index& index
        ) {
            Mesh::Vertex vertex{};
            vertex.color = glm::vec4(1.0f);

            if (index.position_index >= 0) {
                const size_t offset = static_cast<size_t>(index.position_index) * 3;
                if (offset + 2 < attributes.positions.size()) {
                    vertex.position = {
                        attributes.positions[offset],
                        attributes.positions[offset + 1],
                        attributes.positions[offset + 2]
                    };
                }
                if (offset + 2 < attributes.colors.size()) {
                    vertex.color = {
                        attributes.colors[offset],
                        attributes.colors[offset + 1],
                        attributes.colors[offset + 2],
                        1.0f
                    };
                }
            }
            if (index.normal_index >= 0) {
                const size_t offset = static_cast<size_t>(index.normal_index) * 3;
                if (offset + 2 < attributes.normals.size()) {
                    vertex.normal = {
                        attributes.normals[offset],
                        attributes.normals[offset + 1],
                        attributes.normals[offset + 2]
                    };
                }
            }
            if (index.texcoord_index >= 0) {
                const size_t offset = static_cast<size_t>(index.texcoord_index) * 2;
                if (offset + 1 < attributes.texcoords.size()) {
                    vertex.uv = {
                        attributes.texcoords[offset],
                        attributes.texcoords[offset + 1]
                    };
                }
            }
            return vertex;
        }

        void createMesh(
            Scene& scene,
            LoadContext& ctx,
            const rapidobj::Attributes& attributes,
            const rapidobj::Shape& shape,
            const std::vector<Handle<Material>>& materials,
            const Handle<Material> defaultMaterial
        ) {
            if (shape.mesh.indices.empty()) {
                return;
            }

            Mesh mesh;
            mesh.name = shape.name;
            std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexLookup;
            std::vector<bool> needsNormal;
            std::vector<int32_t> materialOrder;
            std::unordered_map<int32_t, size_t> materialBuckets;
            std::vector<std::vector<uint32_t>> indicesByMaterial;

            bool hasNormals = false;

            for (size_t face = 0; face < shape.mesh.material_ids.size(); ++face) {
                const int32_t materialId = shape.mesh.material_ids[face];
                auto [bucketIt, inserted] = materialBuckets.try_emplace(
                    materialId,
                    indicesByMaterial.size()
                );
                if (inserted) {
                    materialOrder.push_back(materialId);
                    indicesByMaterial.emplace_back();
                }
                auto& bucket = indicesByMaterial[bucketIt->second];
                const uint32_t smoothingGroup = shape.mesh.smoothing_group_ids[face];

                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto& sourceIndex = shape.mesh.indices[face * 3 + corner];
                    const bool missingNormal = sourceIndex.normal_index < 0;
                    hasNormals = hasNormals && !missingNormal;
                    const VertexKey key{
                        .position = sourceIndex.position_index,
                        .texcoord = sourceIndex.texcoord_index,
                        .normal = sourceIndex.normal_index,
                        .smoothingGroup = missingNormal ? smoothingGroup : 0,
                        .face = missingNormal && smoothingGroup == 0 ? face : 0
                    };
                    auto [vertexIt, vertexInserted] = vertexLookup.try_emplace(
                        key,
                        static_cast<uint32_t>(mesh.vertices.size())
                    );
                    if (vertexInserted) {
                        mesh.vertices.push_back(makeVertex(attributes, sourceIndex));
                        needsNormal.push_back(missingNormal);
                    }
                    bucket.push_back(vertexIt->second);
                }
            }

            auto object = std::make_unique<GameObject>();
            object->name = shape.name;
            for (size_t bucket = 0; bucket < indicesByMaterial.size(); ++bucket) {
                const auto& sourceIndices = indicesByMaterial[bucket];
                mesh.indices.insert(mesh.indices.end(), sourceIndices.begin(), sourceIndices.end());
                mesh.subMeshData.push_back(static_cast<uint32_t>(sourceIndices.size()));

                const int32_t materialId = materialOrder[bucket];
                if (materialId >= 0 && static_cast<size_t>(materialId) < materials.size()) {
                    object->materials.push_back(materials[materialId]);
                }
                else {
                    object->materials.push_back(defaultMaterial);
                }
            }

            if (!hasNormals) {
                mesh.recalculateNormals();
            }

            const auto bounds = scene.registerMesh(mesh);
            scene.includeBounds(bounds);
            object->mesh = ctx.resources.createMesh(mesh);
            scene.getObjects().push_back(std::move(object));
        }

        void loadObj(
            Scene& scene,
            LoadContext& ctx,
            const std::filesystem::path& path
        ) {
            rapidobj::Result result = rapidobj::ParseFile(path);

            if (result.error) {
                fmt::println("Couldn't load OBJ file: {}", result.error.code.message() );
                return;
            }

            bool success = rapidobj::Triangulate(result);
            if (!success) {
                fmt::println("Couldn't triangulate OBJ file: {}", result.error.code.message() );
                return;
            }

            const auto materials = createMaterials(scene, ctx, path.parent_path(), result.materials);
            const auto defaultMaterial = createDefaultMaterial(scene, ctx);
            for (const auto& shape : result.shapes) {
                createMesh(
                    scene,
                    ctx,
                    result.attributes,
                    shape,
                    materials,
                    defaultMaterial
                );
            }
        }
    }

    void OBJImporter::load(
        Scene& scene,
        LoadContext& context,
        const std::filesystem::path& path
    ) const {
        if (equalsIgnoreCase(path.extension().string(), ".obj")) {
            loadObj(scene, context, path);
        }
    }
}
