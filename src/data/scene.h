#pragma once

#include <cstddef>
#include <vector>
#include <span>

#include "gameobject.h"
#include "mesh.h"
#include "texture.h"
#include "material.h"
#include "bounds.h"

namespace tr::Loading {
    class SceneBuilder;
}

namespace tr::Data {
    class Scene {
    private:
        friend class tr::Loading::SceneBuilder;

        Scene() = default;
        
        std::vector<GameObject> _objects;

        std::vector<Handle<Mesh>> _ownedMeshes;
        std::vector<Handle<Texture>> _ownedTextures;
        std::vector<Handle<Material>> _ownedMaterials;

        Bounds _bounds;
        size_t _vertCount = 0;
        size_t _trisCount = 0;

    public:
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        std::span<const GameObject> getObjects() const { return _objects; }

        std::span<const Handle<Mesh>> getOwnedMeshes() const { return _ownedMeshes; }
        std::span<const Handle<Texture>> getOwnedTextures() const { return _ownedTextures; }
        std::span<const Handle<Material>> getOwnedMaterials() const { return _ownedMaterials; }

        Bounds getBounds() const {
            if (!_bounds.valid) {
                return Bounds::zero();
            }
            return _bounds;
        }

        size_t getVertCount() const { return _vertCount; }
        size_t getTrisCount() const { return _trisCount; }
    };
}
