#include "scene_builder.h"

#include <utility>

#include "defines.h"
#include "import_error.h"
#include "rendering_resources.h"

namespace tr::Loading {
    using namespace tr::Data;
    using tr::Rendering::RenderingResources;

    SceneBuilder::SceneBuilder(RenderingResources& resources)
        : _resources(resources), _scene(new Scene) {
    }

    SceneBuilder::~SceneBuilder() {
        if (!_scene) {
            return;
        }
        for (auto handle : _scene->_ownedMaterials) {
            _resources.destroyMaterial(handle);
        }
        for (auto handle : _scene->_ownedMeshes) {
            _resources.destroyMesh(handle);
        }
        for (auto handle : _scene->_ownedTextures) {
            _resources.destroyTexture(handle);
        }
    }

    std::unique_ptr<Scene> SceneBuilder::build() {
        if (_scene->_objects.empty()) {
            throw ImportError("The scene contains no renderable meshes.");
        }
        return std::move(_scene);
    }

    Handle<Mesh> SceneBuilder::registerMesh(const Mesh& mesh) {
        const auto handle = _resources.uploadMesh(mesh);
        try {
            _scene->_ownedMeshes.push_back(handle);
        }
        catch (...) {
            _resources.destroyMesh(handle);
            throw;
        }
        _meshesData.emplace(
            handle,
            MeshData{
                .bounds = mesh.getBounds(),
                .vertCount = mesh.vertices.size(),
                .trisCount = mesh.indices.size() / 3
            }
        );
        return handle;
    }

    uint32_t SceneBuilder::registerTexture(const Texture& texture) {
        if (_scene->_ownedTextures.size() >= Rendering::MAX_TEXTURES - 1) {
            throw ImportError("The scene exceeds the texture limit.");
        }
        const auto handle = _resources.uploadTexture(texture);
        try {
            _scene->_ownedTextures.push_back(handle);
        }
        catch (...) {
            _resources.destroyTexture(handle);
            throw;
        }
        // Slot 0 is reserved for the fallback texture.
        return static_cast<uint32_t>(_scene->_ownedTextures.size());
    }

    Handle<Material> SceneBuilder::registerMaterial(const Material& material) {
        const auto handle = _resources.uploadMaterial(material);
        try {
            _scene->_ownedMaterials.push_back(handle);
        }
        catch (...) {
            _resources.destroyMaterial(handle);
            throw;
        }
        return handle;
    }

    void SceneBuilder::addObject(GameObject object) {
        const auto& meshData = _meshesData.at(object.mesh);
        const auto transform = object.transform.getMatrix();
        _scene->_objects.push_back(std::move(object));
        _scene->_bounds.encapsulate(meshData.bounds, transform);
        _scene->_vertCount += meshData.vertCount;
        _scene->_trisCount += meshData.trisCount;
    }
}
