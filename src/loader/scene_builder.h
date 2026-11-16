#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "scene.h"

namespace tr::Rendering {
    class RenderingResources;
}

namespace tr::Loading {
    class SceneBuilder {
    private:
        struct MeshData {
            Data::Bounds bounds;
            size_t vertCount;
            size_t trisCount;
        };
        
        Rendering::RenderingResources& _resources;
        std::unique_ptr<Data::Scene> _scene;

        std::unordered_map<Data::Handle<Data::Mesh>, MeshData> _meshesData;

    public:
        explicit SceneBuilder(Rendering::RenderingResources& resources);
        ~SceneBuilder();

        SceneBuilder(const SceneBuilder&) = delete;
        SceneBuilder& operator=(const SceneBuilder&) = delete;

        std::unique_ptr<Data::Scene> build();
        
        Data::Handle<Data::Mesh> registerMesh(const Data::Mesh& mesh);
        Data::Handle<Data::Material> registerMaterial(const Data::Material& material);

        uint32_t registerTexture(const Data::Texture& texture);

        void addObject(Data::GameObject object);
    };
}
