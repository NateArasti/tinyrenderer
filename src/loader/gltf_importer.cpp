#include "gltf_importer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/glm.hpp>

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

        enum class TextureChannel : uint8_t {
            All,
            Green,
            Blue
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

        glm::mat4 toGlm(const fastgltf::math::fmat4x4& source) {
            glm::mat4 result(1.0f);
            for (size_t column = 0; column < 4; ++column) {
                for (size_t row = 0; row < 4; ++row) {
                    result[column][row] = source[column][row];
                }
            }
            return result;
        }

        std::span<const std::byte> getImageBytes(
            const fastgltf::Asset& asset,
            const fastgltf::Image& image
        ) {
            if (const auto* source = std::get_if<fastgltf::sources::Array>(&image.data)) {
                return { source->bytes.data(), source->bytes.size() };
            }
            if (const auto* source = std::get_if<fastgltf::sources::Vector>(&image.data)) {
                return source->bytes;
            }
            if (const auto* source = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
                return source->bytes;
            }
            if (const auto* source = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
                const auto bytes = fastgltf::DefaultBufferDataAdapter{}(
                    asset,
                    source->bufferViewIndex
                );
                return { bytes.data(), bytes.size() };
            }
            return {};
        }

        Handle<Texture> loadTexture(
            LoadContext& context,
            const fastgltf::Asset& asset,
            size_t textureIndex,
            TextureColorSpace colorSpace,
            TextureChannel channel,
            std::unordered_map<std::string, Handle<Texture>>& cache
        ) {
            if (textureIndex >= asset.textures.size()) {
                return {};
            }

            const auto& sourceTexture = asset.textures[textureIndex];
            const auto imageIndex = sourceTexture.imageIndex;
            if (!imageIndex || *imageIndex >= asset.images.size()) {
                return {};
            }

            const std::string cacheKey =
                std::to_string(*imageIndex)
                + (colorSpace == TextureColorSpace::SRGB ? "#srgb#" : "#linear#")
                + std::to_string(static_cast<uint8_t>(channel));
            if (const auto found = cache.find(cacheKey); found != cache.end()) {
                return found->second;
            }

            const auto& sourceImage = asset.images[*imageIndex];
            const auto bytes = getImageBytes(asset, sourceImage);
            if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                fmt::println("Couldn't read glTF image {}", *imageIndex);
                return {};
            }

            int width = 0;
            int height = 0;
            stbi_uc* decoded = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(bytes.data()),
                static_cast<int>(bytes.size()),
                &width,
                &height,
                nullptr,
                STBI_rgb_alpha
            );
            if (!decoded || width <= 0 || height <= 0) {
                fmt::println(
                    "Couldn't decode glTF image {}: {}",
                    *imageIndex,
                    stbi_failure_reason() ? stbi_failure_reason() : "unknown error"
                );
                stbi_image_free(decoded);
                return {};
            }

            const size_t pixelDataSize = static_cast<size_t>(width) * static_cast<size_t>(height) * STBI_rgb_alpha;
            Texture texture;
            texture.name = sourceImage.name.empty()
                ? "image_" + std::to_string(*imageIndex)
                : std::string(sourceImage.name);
            texture.width = static_cast<uint32_t>(width);
            texture.height = static_cast<uint32_t>(height);
            texture.channels = STBI_rgb_alpha;
            texture.colorSpace = colorSpace;
            texture.pixels.assign(decoded, decoded + pixelDataSize);
            stbi_image_free(decoded);

            if (channel != TextureChannel::All) {
                const size_t sourceChannel = channel == TextureChannel::Green ? 1 : 2;
                for (size_t pixel = 0; pixel < pixelDataSize; pixel += STBI_rgb_alpha) {
                    const uint8_t value = texture.pixels[pixel + sourceChannel];
                    texture.pixels[pixel] = value;
                    texture.pixels[pixel + 1] = value;
                    texture.pixels[pixel + 2] = value;
                }
            }

            const auto handle = context.resources.createTexture(texture);
            cache.emplace(cacheKey, handle);
            return handle;
        }

        Handle<Material> createDefaultMaterial(Scene& scene, LoadContext& context) {
            auto material = std::make_unique<Material>();
            material->name = "default";
            material->set("diffuseColor", glm::vec4(1.0f));
            material->set("ambientColor", glm::vec4(glm::vec3(0.03f), 1.0f));
            material->set("specularColor", glm::vec4(glm::vec3(0.04f), 1.0f));
            const auto handle = scene.materials.add(std::move(material));
            context.resources.registerMaterial(handle, *scene.materials.get(handle));
            return handle;
        }

        std::vector<Handle<Material>> createMaterials(
            Scene& scene,
            LoadContext& context,
            const fastgltf::Asset& asset
        ) {
            std::vector<Handle<Material>> result;
            result.reserve(asset.materials.size());
            std::unordered_map<std::string, Handle<Texture>> textureCache;

            for (const auto& source : asset.materials) {
                auto material = std::make_unique<Material>();
                material->name = std::string(source.name);
                material->blendMode = source.alphaMode == fastgltf::AlphaMode::Opaque
                    ? BlendMode::Opaque : BlendMode::Transparent;

                const auto& base = source.pbrData.baseColorFactor;
                material->set("diffuseColor", glm::vec4(base[0], base[1], base[2], base[3]));
                material->set("ambientColor", glm::vec4(glm::vec3(0.03f), 1.0f));
                material->set("specularColor", glm::vec4(glm::vec3(0.04f), 1.0f));
                material->set("metallicFactor", static_cast<float>(source.pbrData.metallicFactor));
                material->set("roughnessFactor", static_cast<float>(source.pbrData.roughnessFactor));
                material->set(
                    "emissionColor",
                    glm::vec4(
                        source.emissiveFactor[0] * source.emissiveStrength,
                        source.emissiveFactor[1] * source.emissiveStrength,
                        source.emissiveFactor[2] * source.emissiveStrength,
                        1.0f
                    )
                );

                const auto setTexture = [&](
                    size_t textureIndex,
                    std::string_view mapParam,
                    std::string_view flagParam,
                    TextureColorSpace colorSpace,
                    TextureChannel channel = TextureChannel::All
                ) {
                    const auto texture = loadTexture(
                        context,
                        asset,
                        textureIndex,
                        colorSpace,
                        channel,
                        textureCache
                    );
                    if (texture.isValid()) {
                        material->set(std::string(mapParam), texture);
                        material->set(std::string(flagParam), 1.0f);
                    }
                };

                if (source.pbrData.baseColorTexture) {
                    setTexture(
                        source.pbrData.baseColorTexture->textureIndex,
                        "diffuseMap",
                        "hasDiffuseMap",
                        TextureColorSpace::SRGB
                    );
                }
                if (source.emissiveTexture) {
                    setTexture(
                        source.emissiveTexture->textureIndex,
                        "emissionMap",
                        "hasEmissionMap",
                        TextureColorSpace::SRGB
                    );
                }
                if (source.normalTexture) {
                    setTexture(
                        source.normalTexture->textureIndex,
                        "normalMap",
                        "hasNormalMap",
                        TextureColorSpace::Linear
                    );
                    material->set("normalScale", static_cast<float>(source.normalTexture->scale));
                }
                if (source.pbrData.metallicRoughnessTexture) {
                    const size_t textureIndex = source.pbrData.metallicRoughnessTexture->textureIndex;
                    setTexture(
                        textureIndex,
                        "metallicMap",
                        "hasMetallicMap",
                        TextureColorSpace::Linear,
                        TextureChannel::Blue
                    );
                    setTexture(
                        textureIndex,
                        "roughnessMap",
                        "hasRoughnessMap",
                        TextureColorSpace::Linear,
                        TextureChannel::Green
                    );
                }

                const auto handle = scene.materials.add(std::move(material));
                context.resources.registerMaterial(handle, *scene.materials.get(handle));
                result.push_back(handle);
            }
            return result;
        }

        template<typename Vector>
        void readVectorAttribute(
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& primitive,
            std::string_view name,
            const auto& write
        ) {
            const auto attribute = primitive.findAttribute(name);
            if (attribute == primitive.attributes.end()
                || attribute->accessorIndex >= asset.accessors.size()) {
                return;
            }
            fastgltf::iterateAccessorWithIndex<Vector>(
                asset,
                asset.accessors[attribute->accessorIndex],
                [&](const Vector& value, size_t index) {
                    write(value, index);
                }
            );
        }

        std::vector<uint32_t> readPrimitiveIndices(
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& primitive,
            size_t vertexCount
        ) {
            std::vector<uint32_t> source;
            if (primitive.indicesAccessor && *primitive.indicesAccessor < asset.accessors.size()) {
                source.reserve(asset.accessors[*primitive.indicesAccessor].count);
                fastgltf::iterateAccessor<uint32_t>(
                    asset,
                    asset.accessors[*primitive.indicesAccessor],
                    [&](uint32_t index) { source.push_back(index); }
                );
            }
            else {
                source.resize(vertexCount);
                for (size_t index = 0; index < vertexCount; ++index) {
                    source[index] = static_cast<uint32_t>(index);
                }
            }

            if (primitive.type == fastgltf::PrimitiveType::Triangles) {
                source.resize(source.size() - source.size() % 3);
                return source;
            }

            std::vector<uint32_t> triangles;
            if (primitive.type == fastgltf::PrimitiveType::TriangleStrip) {
                if (source.size() < 3) {
                    return triangles;
                }
                triangles.reserve((source.size() - 2) * 3);
                for (size_t index = 2; index < source.size(); ++index) {
                    if (index % 2 == 0) {
                        triangles.insert(
                            triangles.end(),
                            { source[index - 2], source[index - 1], source[index] }
                        );
                    }
                    else {
                        triangles.insert(
                            triangles.end(),
                            { source[index - 1], source[index - 2], source[index] }
                        );
                    }
                }
            }
            else if (primitive.type == fastgltf::PrimitiveType::TriangleFan) {
                if (source.size() < 3) {
                    return triangles;
                }
                triangles.reserve((source.size() - 2) * 3);
                for (size_t index = 2; index < source.size(); ++index) {
                    triangles.insert(
                        triangles.end(),
                        { source[0], source[index - 1], source[index] }
                    );
                }
            }
            return triangles;
        }

        struct ImportedMesh {
            Handle<Mesh> mesh;
            std::vector<Handle<Material>> materials;
            MeshBounds bounds;
        };

        ImportedMesh createMesh(
            Scene& scene,
            LoadContext& context,
            const fastgltf::Asset& asset,
            const fastgltf::Mesh& sourceMesh,
            const std::vector<Handle<Material>>& materials,
            Handle<Material> defaultMaterial
        ) {
            Mesh mesh;
            mesh.name = std::string(sourceMesh.name);
            std::vector<Handle<Material>> meshMaterials;
            bool hasGeometry = false;

            for (const auto& primitive : sourceMesh.primitives) {
                const auto positionAttribute = primitive.findAttribute("POSITION");
                if (positionAttribute == primitive.attributes.end()
                    || positionAttribute->accessorIndex >= asset.accessors.size()) {
                    continue;
                }

                const auto& positionAccessor = asset.accessors[positionAttribute->accessorIndex];
                if (positionAccessor.count == 0 ||
                    positionAccessor.count > std::numeric_limits<uint32_t>::max() ||
                    mesh.vertices.size() > std::numeric_limits<uint32_t>::max() - positionAccessor.count
                ) {
                    continue;
                }

                const uint32_t vertexOffset = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.resize(mesh.vertices.size() + positionAccessor.count);
                bool hasNormals = false;

                readVectorAttribute<fastgltf::math::fvec3>(
                    asset,
                    primitive,
                    "POSITION",
                    [&](const auto& value, size_t index) {
                        mesh.vertices[vertexOffset + index].position = {
                            value[0], value[1], value[2]
                        };
                    }
                );
                readVectorAttribute<fastgltf::math::fvec3>(
                    asset,
                    primitive,
                    "NORMAL",
                    [&](const auto& value, size_t index) {
                        if (index < positionAccessor.count) {
                            mesh.vertices[vertexOffset + index].normal = {
                                value[0], value[1], value[2]
                            };
                            hasNormals = true;
                        }
                    }
                );
                readVectorAttribute<fastgltf::math::fvec2>(
                    asset,
                    primitive,
                    "TEXCOORD_0",
                    [&](const auto& value, size_t index) {
                        if (index < positionAccessor.count) {
                            mesh.vertices[vertexOffset + index].uv = { value[0], value[1] };
                        }
                    }
                );

                const auto colorAttribute = primitive.findAttribute("COLOR_0");
                if (colorAttribute != primitive.attributes.end()
                    && colorAttribute->accessorIndex < asset.accessors.size()) {
                    const auto& colorAccessor = asset.accessors[colorAttribute->accessorIndex];
                    if (colorAccessor.type == fastgltf::AccessorType::Vec3) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            asset,
                            colorAccessor,
                            [&](const auto& value, size_t index) {
                                if (index < positionAccessor.count) {
                                    mesh.vertices[vertexOffset + index].color = {
                                        value[0], value[1], value[2], 1.0f
                                    };
                                }
                            }
                        );
                    }
                    else if (colorAccessor.type == fastgltf::AccessorType::Vec4) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset,
                            colorAccessor,
                            [&](const auto& value, size_t index) {
                                if (index < positionAccessor.count) {
                                    mesh.vertices[vertexOffset + index].color = {
                                        value[0], value[1], value[2], value[3]
                                    };
                                }
                            }
                        );
                    }
                }

                auto primitiveIndices = readPrimitiveIndices(
                    asset,
                    primitive,
                    positionAccessor.count
                );
                primitiveIndices.resize(primitiveIndices.size() - primitiveIndices.size() % 3);
                std::vector<uint32_t> validIndices;
                validIndices.reserve(primitiveIndices.size());
                for (size_t index = 0; index < primitiveIndices.size(); index += 3) {
                    if (primitiveIndices[index] < positionAccessor.count
                        && primitiveIndices[index + 1] < positionAccessor.count
                        && primitiveIndices[index + 2] < positionAccessor.count) {
                        validIndices.insert(
                            validIndices.end(),
                            {
                                primitiveIndices[index],
                                primitiveIndices[index + 1],
                                primitiveIndices[index + 2]
                            }
                        );
                    }
                }
                primitiveIndices = std::move(validIndices);
                if (primitiveIndices.empty()) {
                    mesh.vertices.resize(vertexOffset);
                    continue;
                }
                for (uint32_t& index : primitiveIndices) {
                    index += vertexOffset;
                }
                mesh.indices.insert(
                    mesh.indices.end(),
                    primitiveIndices.begin(),
                    primitiveIndices.end()
                );
                mesh.subMeshData.push_back(static_cast<uint32_t>(primitiveIndices.size()));
                if (primitive.materialIndex && *primitive.materialIndex < materials.size()) {
                    meshMaterials.push_back(materials[*primitive.materialIndex]);
                }
                else {
                    meshMaterials.push_back(defaultMaterial);
                }
                hasGeometry = true;

                if (!hasNormals) {
                    const size_t firstIndex = mesh.indices.size() - primitiveIndices.size();
                    for (size_t index = firstIndex; index < mesh.indices.size(); index += 3) {
                        const uint32_t i0 = mesh.indices[index];
                        const uint32_t i1 = mesh.indices[index + 1];
                        const uint32_t i2 = mesh.indices[index + 2];
                        const glm::vec3 edgeA = mesh.vertices[i1].position - mesh.vertices[i0].position;
                        const glm::vec3 edgeB = mesh.vertices[i2].position - mesh.vertices[i0].position;
                        const glm::vec3 normal = glm::normalize(glm::cross(edgeA, edgeB));
                        mesh.vertices[i0].normal = normal;
                        mesh.vertices[i1].normal = normal;
                        mesh.vertices[i2].normal = normal;
                    }
                }
            }

            if (!hasGeometry) {
                return {};
            }
            const auto bounds = scene.registerMesh(mesh);
            return {
                context.resources.createMesh(mesh),
                std::move(meshMaterials),
                bounds
            };
        }

        void loadGltf(
            Scene& scene,
            LoadContext& context,
            const std::filesystem::path& path
        ) {
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            if (data.error() != fastgltf::Error::None) {
                throw ImportError(fmt::format(
                    "Couldn't open glTF file: {}",
                    fastgltf::getErrorMessage(data.error())
                ));
            }
            constexpr auto options =
                fastgltf::Options::LoadExternalBuffers |
                fastgltf::Options::LoadExternalImages;

            fastgltf::Parser parser(
                fastgltf::Extensions::KHR_mesh_quantization |
                fastgltf::Extensions::KHR_materials_emissive_strength |
                fastgltf::Extensions::KHR_lights_punctual
            );
            auto loaded = parser.loadGltf(data.get(), path.parent_path(), options);
            if (loaded.error() != fastgltf::Error::None) {
                throw ImportError(fmt::format(
                    "Couldn't load glTF file: {}",
                    fastgltf::getErrorMessage(loaded.error())
                ));
            }

            auto asset = std::move(loaded.get());
            const auto materials = createMaterials(scene, context, asset);
            const auto defaultMaterial = createDefaultMaterial(scene, context);
            std::vector<ImportedMesh> meshes(asset.meshes.size());
            for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
                meshes[meshIndex] = createMesh(
                    scene,
                    context,
                    asset,
                    asset.meshes[meshIndex],
                    materials,
                    defaultMaterial
                );
            }
            const auto visitNode = [&](
                size_t nodeIndex,
                const fastgltf::math::fmat4x4& parent,
                const auto& self
            ) -> void {
                if (nodeIndex >= asset.nodes.size()) {
                    return;
                }
                const auto& node = asset.nodes[nodeIndex];
                const auto transform = fastgltf::getTransformMatrix(node, parent);
                if (node.meshIndex && *node.meshIndex < meshes.size()) {
                    const auto& importedMesh = meshes[*node.meshIndex];
                    if (importedMesh.mesh.isValid()) {
                        auto object = std::make_unique<GameObject>();
                        object->name = node.name.empty()
                            ? std::string(asset.meshes[*node.meshIndex].name)
                            : std::string(node.name);
                        object->mesh = importedMesh.mesh;
                        object->materials = importedMesh.materials;
                        object->transform = Transform::fromTransform(toGlm(transform));
                        scene.includeBounds(importedMesh.bounds, object->transform.getMatrix());
                        scene.getObjects().push_back(std::move(object));
                    }
                }
                for (size_t child : node.children) {
                    self(child, transform, self);
                }
            };

            if (!asset.scenes.empty()) {
                const size_t sceneIndex = asset.defaultScene.value_or(0);
                if (sceneIndex < asset.scenes.size()) {
                    for (size_t nodeIndex : asset.scenes[sceneIndex].nodeIndices) {
                        visitNode(nodeIndex, fastgltf::math::fmat4x4(), visitNode);
                    }
                }
            }
            else {
                std::vector<bool> isChild(asset.nodes.size(), false);
                for (const auto& node : asset.nodes) {
                    for (size_t child : node.children) {
                        if (child < isChild.size()) {
                            isChild[child] = true;
                        }
                    }
                }
                for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex) {
                    if (!isChild[nodeIndex]) {
                        visitNode(nodeIndex, fastgltf::math::fmat4x4(), visitNode);
                    }
                }
            }
        }
    }

    void GLTFImporter::load(
        Scene& scene,
        LoadContext& context,
        const std::filesystem::path& path
    ) const {
        const std::string extension = path.extension().string();
        if (equalsIgnoreCase(extension, ".gltf") || equalsIgnoreCase(extension, ".glb")) {
            loadGltf(scene, context, path);
        }
    }
}
