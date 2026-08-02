#include "fbx_importer.h"

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
#include <ufbx.h>

#include "stb_image.h"

#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "rhi.h"
#include "transform.h"

namespace tr::Loading {
    namespace {
        using namespace tr::Data;
        using namespace tr::Resources;

        struct SceneDeleter {
            void operator()(ufbx_scene* scene) const {
                ufbx_free_scene(scene);
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

        std::string toString(ufbx_string source) {
            return source.data ? std::string(source.data, source.length) : std::string();
        }

        glm::vec3 toGlm(ufbx_vec3 source) {
            return {
                static_cast<float>(source.x),
                static_cast<float>(source.y),
                static_cast<float>(source.z)
            };
        }

        glm::vec4 toGlm(ufbx_vec4 source) {
            return {
                static_cast<float>(source.x),
                static_cast<float>(source.y),
                static_cast<float>(source.z),
                static_cast<float>(source.w)
            };
        }

        glm::mat4 toGlm(const ufbx_matrix& source) {
            glm::mat4 result(1.0f);
            for (size_t column = 0; column < 4; ++column) {
                result[column] = glm::vec4(toGlm(source.cols[column]), column == 3 ? 1.0f : 0.0f);
            }
            return result;
        }

        std::vector<std::byte> readFile(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input) {
                return {};
            }
            const auto size = input.tellg();
            if (size <= 0 || size > std::numeric_limits<int>::max()) {
                return {};
            }
            std::vector<std::byte> result(static_cast<size_t>(size));
            input.seekg(0, std::ios::beg);
            input.read(
                reinterpret_cast<char*>(result.data()),
                static_cast<std::streamsize>(result.size())
            );
            if (!input) {
                return {};
            }
            return result;
        }

        const ufbx_texture* getFileTexture(const ufbx_texture* texture) {
            if (!texture) {
                return nullptr;
            }
            if (texture->type == UFBX_TEXTURE_FILE) {
                return texture;
            }
            return texture->file_textures.count > 0 ? texture->file_textures.data[0] : nullptr;
        }

        Handle<Texture> loadTexture(
            LoadContext& context,
            const std::filesystem::path& modelDirectory,
            const ufbx_texture* sourceTexture,
            TextureColorSpace colorSpace,
            std::unordered_map<std::string, Handle<Texture>>& cache
        ) {
            sourceTexture = getFileTexture(sourceTexture);
            if (!sourceTexture) {
                return {};
            }

            const std::string cacheKey =
                std::to_string(sourceTexture->typed_id)
                + (colorSpace == TextureColorSpace::SRGB ? "#srgb" : "#linear");
            if (const auto found = cache.find(cacheKey); found != cache.end()) {
                return found->second;
            }

            const void* encodedData = sourceTexture->content.data;
            size_t encodedSize = sourceTexture->content.size;
            std::vector<std::byte> fileData;
            std::filesystem::path texturePath;

            if (!encodedData || encodedSize == 0) {
                std::vector<std::filesystem::path> candidates;
                const auto addCandidate = [&](ufbx_string filename, bool relative) {
                    if (!filename.data || filename.length == 0) {
                        return;
                    }
                    auto candidate = std::filesystem::path(toString(filename));
                    if (relative && candidate.is_relative()) {
                        candidate = modelDirectory / candidate;
                    }
                    candidates.push_back(candidate.lexically_normal());
                };
                addCandidate(sourceTexture->filename, false);
                addCandidate(sourceTexture->relative_filename, true);
                addCandidate(sourceTexture->absolute_filename, false);

                for (const auto& candidate : candidates) {
                    fileData = readFile(candidate);
                    if (!fileData.empty()) {
                        texturePath = candidate;
                        encodedData = fileData.data();
                        encodedSize = fileData.size();
                        break;
                    }
                }
            }

            if (!encodedData || encodedSize == 0
                || encodedSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
                fmt::println("Couldn't read FBX texture: {}", toString(sourceTexture->name));
                return {};
            }

            int width = 0;
            int height = 0;
            stbi_uc* decoded = stbi_load_from_memory(
                static_cast<const stbi_uc*>(encodedData),
                static_cast<int>(encodedSize),
                &width,
                &height,
                nullptr,
                STBI_rgb_alpha
            );
            if (!decoded || width <= 0 || height <= 0) {
                fmt::println(
                    "Couldn't decode FBX texture {}: {}",
                    texturePath.empty() ? toString(sourceTexture->name) : texturePath.string(),
                    stbi_failure_reason() ? stbi_failure_reason() : "unknown error"
                );
                stbi_image_free(decoded);
                return {};
            }

            const size_t rowSize = static_cast<size_t>(width) * STBI_rgb_alpha;
            const size_t pixelDataSize = rowSize * static_cast<size_t>(height);
            Texture texture;
            texture.name = toString(sourceTexture->name);
            if (texture.name.empty() && !texturePath.empty()) {
                texture.name = texturePath.filename().string();
            }
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

            const auto handle = context.resources.createTexture(texture);
            cache.emplace(cacheKey, handle);
            return handle;
        }

        float mapReal(const ufbx_material_map& map, float fallback) {
            return map.has_value ? static_cast<float>(map.value_real) : fallback;
        }

        glm::vec3 mapVec3(const ufbx_material_map& map, glm::vec3 fallback) {
            return map.has_value ? toGlm(map.value_vec3) : fallback;
        }

        Handle<Material> createDefaultMaterial(Scene& scene, LoadContext& context) {
            auto material = std::make_unique<Material>();
            material->name = "default";
            material->set("diffuseColor", glm::vec4(1.0f));
            material->set("ambientColor", glm::vec4(glm::vec3(0.03f), 1.0f));
            material->set("specularColor", glm::vec4(glm::vec3(0.04f), 1.0f));
            material->set("metallicFactor", 0.0f);
            material->set("roughnessFactor", 1.0f);
            const auto handle = scene.materials.add(std::move(material));
            context.resources.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        std::unordered_map<const ufbx_material*, Handle<Material>> createMaterials(
            Scene& scene,
            LoadContext& context,
            const std::filesystem::path& modelDirectory,
            const ufbx_scene& sourceScene
        ) {
            std::unordered_map<const ufbx_material*, Handle<Material>> result;
            std::unordered_map<std::string, Handle<Texture>> textureCache;

            for (const ufbx_material* source : sourceScene.materials) {
                const auto& pbr = source->pbr;
                const float opacity = std::clamp(mapReal(pbr.opacity, 1.0f), 0.0f, 1.0f);
                const bool transparent =
                    opacity < 0.999f
                    || (pbr.opacity.texture && pbr.opacity.texture_enabled);
                auto material = std::make_unique<Material>();
                material->blendMode = transparent ? BlendMode::Transparent : BlendMode::Opaque;
                material->name = toString(source->name);

                const glm::vec3 baseColor =
                    mapVec3(pbr.base_color, glm::vec3(1.0f))
                    * mapReal(pbr.base_factor, 1.0f);
                const glm::vec3 emission =
                    mapVec3(pbr.emission_color, glm::vec3(0.0f))
                    * mapReal(pbr.emission_factor, 1.0f);
                material->set("diffuseColor", glm::vec4(baseColor, opacity));
                material->set("ambientColor", glm::vec4(glm::vec3(0.03f), 1.0f));
                material->set(
                    "specularColor",
                    glm::vec4(mapVec3(pbr.specular_color, glm::vec3(0.04f)), 1.0f)
                );
                material->set("emissionColor", glm::vec4(emission, 1.0f));
                material->set(
                    "metallicFactor",
                    std::clamp(mapReal(pbr.metalness, 0.0f), 0.0f, 1.0f)
                );
                material->set(
                    "roughnessFactor",
                    std::clamp(mapReal(pbr.roughness, 1.0f), 0.04f, 1.0f)
                );
                material->set(
                    "normalScale",
                    mapReal(source->fbx.bump_factor, 1.0f)
                );

                const auto setTexture = [&](
                    const ufbx_material_map& map,
                    std::string_view mapParam,
                    std::string_view flagParam,
                    TextureColorSpace colorSpace
                ) {
                    if (!map.texture_enabled) {
                        return;
                    }
                    const auto texture = loadTexture(
                        context,
                        modelDirectory,
                        map.texture,
                        colorSpace,
                        textureCache
                    );
                    if (texture.isValid()) {
                        material->set(std::string(mapParam), texture);
                        material->set(std::string(flagParam), 1.0f);
                    }
                };

                setTexture(
                    pbr.base_color,
                    "diffuseMap",
                    "hasDiffuseMap",
                    TextureColorSpace::SRGB
                );
                setTexture(
                    pbr.emission_color,
                    "emissionMap",
                    "hasEmissionMap",
                    TextureColorSpace::SRGB
                );
                setTexture(
                    pbr.normal_map,
                    "normalMap",
                    "hasNormalMap",
                    TextureColorSpace::Linear
                );
                setTexture(
                    pbr.metalness,
                    "metallicMap",
                    "hasMetallicMap",
                    TextureColorSpace::Linear
                );
                setTexture(
                    pbr.roughness,
                    "roughnessMap",
                    "hasRoughnessMap",
                    TextureColorSpace::Linear
                );
                setTexture(
                    pbr.specular_color,
                    "specularMap",
                    "hasSpecularMap",
                    TextureColorSpace::SRGB
                );
                setTexture(
                    pbr.opacity,
                    "opacityMap",
                    "hasOpacityMap",
                    TextureColorSpace::Linear
                );

                const auto handle = scene.materials.add(std::move(material));
                context.resources.registerMaterial(handle, *scene.materials.get(handle));
                result.emplace(source, handle);
            }
            return result;
        }

        struct ImportedMesh {
            Handle<Mesh> mesh;
            std::vector<uint32_t> materialSlots;
            MeshBounds bounds;
        };

        ImportedMesh createMesh(
            Scene& scene,
            LoadContext& context,
            const ufbx_mesh& source
        ) {
            if (source.num_triangles == 0 || !source.vertex_position.exists) {
                return {};
            }

            Mesh mesh;
            mesh.name = toString(source.name);
            std::vector<uint32_t> materialSlots;

            std::vector<Mesh::Vertex> flatVertices;
            flatVertices.reserve(source.num_triangles * 3);
            std::vector<uint32_t> triangleIndices(source.max_face_triangles * 3);

            std::vector<uint32_t> partOrder;
            if (source.material_part_usage_order.count > 0) {
                partOrder.assign(
                    source.material_part_usage_order.data,
                    source.material_part_usage_order.data + source.material_part_usage_order.count
                );
            }
            else {
                partOrder.resize(source.material_parts.count);
                for (size_t index = 0; index < partOrder.size(); ++index) {
                    partOrder[index] = static_cast<uint32_t>(index);
                }
            }

            for (uint32_t partIndex : partOrder) {
                if (partIndex >= source.material_parts.count) {
                    continue;
                }
                const ufbx_mesh_part& part = source.material_parts.data[partIndex];
                const size_t subMeshBegin = flatVertices.size();

                for (uint32_t faceIndex : part.face_indices) {
                    if (faceIndex >= source.faces.count) {
                        continue;
                    }
                    const ufbx_face face = source.faces.data[faceIndex];
                    const uint32_t numTriangles = ufbx_triangulate_face(
                        triangleIndices.data(),
                        triangleIndices.size(),
                        &source,
                        face
                    );
                    for (uint32_t triangle = 0; triangle < numTriangles; ++triangle) {
                        const uint32_t corners[3] = {
                            triangleIndices[triangle * 3],
                            triangleIndices[triangle * 3 + 1],
                            triangleIndices[triangle * 3 + 2]
                        };
                        for (uint32_t corner : corners) {
                            Mesh::Vertex vertex{};
                            vertex.position = toGlm(
                                ufbx_get_vertex_vec3(&source.vertex_position, corner)
                            );
                            if (source.vertex_normal.exists) {
                                vertex.normal = toGlm(
                                    ufbx_get_vertex_vec3(&source.vertex_normal, corner)
                                );
                            }
                            if (source.vertex_uv.exists) {
                                const ufbx_vec2 uv =
                                    ufbx_get_vertex_vec2(&source.vertex_uv, corner);
                                vertex.uv = {
                                    static_cast<float>(uv.x),
                                    static_cast<float>(uv.y)
                                };
                            }
                            if (source.vertex_color.exists) {
                                vertex.color = toGlm(
                                    ufbx_get_vertex_vec4(&source.vertex_color, corner)
                                );
                            }
                            flatVertices.push_back(vertex);
                        }
                    }
                }

                const size_t subMeshSize = flatVertices.size() - subMeshBegin;
                if (subMeshSize == 0) {
                    continue;
                }
                mesh.subMeshData.push_back(static_cast<uint32_t>(subMeshSize));
                materialSlots.push_back(partIndex);
            }

            if (flatVertices.empty()
                || flatVertices.size() > std::numeric_limits<uint32_t>::max()) {
                return {};
            }

            mesh.indices.resize(flatVertices.size());
            ufbx_vertex_stream stream{
                .data = flatVertices.data(),
                .vertex_count = flatVertices.size(),
                .vertex_size = sizeof(Mesh::Vertex)
            };
            ufbx_error indexError{};
            const size_t numVertices = ufbx_generate_indices(
                &stream,
                1,
                mesh.indices.data(),
                mesh.indices.size(),
                nullptr,
                &indexError
            );
            if (numVertices == 0) {
                char description[512]{};
                ufbx_format_error(description, sizeof(description), &indexError);
                fmt::println("Couldn't index FBX mesh {}: {}", mesh.name, description);
                return {};
            }
            flatVertices.resize(numVertices);
            mesh.vertices = std::move(flatVertices);
            const auto bounds = scene.registerMesh(mesh);

            return {
                context.resources.createMesh(mesh),
                std::move(materialSlots),
                bounds
            };
        }

        void loadFbx(
            Scene& scene,
            LoadContext& context,
            const std::filesystem::path& path
        ) {
            ufbx_load_opts options{};
            options.generate_missing_normals = true;
            options.ignore_animation = true;
            options.ignore_missing_external_files = true;
            options.use_blender_pbr_material = true;
            options.target_axes = {
                UFBX_COORDINATE_AXIS_POSITIVE_X,
                UFBX_COORDINATE_AXIS_POSITIVE_Y,
                UFBX_COORDINATE_AXIS_POSITIVE_Z
            };
            options.target_unit_meters = 1.0;

            const std::u8string filename = path.u8string();
            ufbx_error error{};
            std::unique_ptr<ufbx_scene, SceneDeleter> sourceScene(
                ufbx_load_file_len(
                    reinterpret_cast<const char*>(filename.data()),
                    filename.size(),
                    &options,
                    &error
                )
            );
            if (!sourceScene) {
                char description[1024]{};
                ufbx_format_error(description, sizeof(description), &error);
                fmt::println("Couldn't load FBX file: {}", description);
                return;
            }

            const auto materials = createMaterials(
                scene,
                context,
                path.parent_path(),
                *sourceScene
            );
            const auto defaultMaterial = createDefaultMaterial(scene, context);
            std::unordered_map<const ufbx_mesh*, ImportedMesh> meshes;
            meshes.reserve(sourceScene->meshes.count);
            for (const ufbx_mesh* sourceMesh : sourceScene->meshes) {
                meshes.emplace(sourceMesh, createMesh(scene, context, *sourceMesh));
            }
            for (const ufbx_node* node : sourceScene->nodes) {
                if (!node->mesh) {
                    continue;
                }
                const auto foundMesh = meshes.find(node->mesh);
                if (foundMesh == meshes.end() || !foundMesh->second.mesh.isValid()) {
                    continue;
                }

                auto object = std::make_unique<GameObject>();
                object->name = toString(node->name);
                if (object->name.empty()) {
                    object->name = toString(node->mesh->name);
                }
                object->mesh = foundMesh->second.mesh;
                object->transform = Transform::fromTransform(toGlm(node->geometry_to_world));
                scene.includeBounds(foundMesh->second.bounds, object->transform.getMatrix());
                for (uint32_t materialSlot : foundMesh->second.materialSlots) {
                    if (materialSlot < node->materials.count) {
                        const auto foundMaterial = materials.find(
                            node->materials.data[materialSlot]
                        );
                        object->materials.push_back(
                            foundMaterial != materials.end()
                                ? foundMaterial->second
                                : defaultMaterial
                        );
                    }
                    else {
                        object->materials.push_back(defaultMaterial);
                    }
                }
                scene.getObjects().push_back(std::move(object));
            }
        }
    }

    void FBXImporter::load(
        Scene& scene,
        LoadContext& context,
        const std::filesystem::path& path
    ) const {
        if (equalsIgnoreCase(path.extension().string(), ".fbx")) {
            loadFbx(scene, context, path);
        }
    }
}
