#pragma once

#include "handle.h"
#include "pool.h"
#include "shader.h"
#include "texture.h"
#include "material.h"
#include "mesh.h"

namespace tr::Data {
    struct ResourceManager {
        tr::Resources::Pool<tr::Data::Shader> shadersPool;
        tr::Resources::Pool<tr::Data::Texture> texturesPool;
        tr::Resources::Pool<tr::Data::Material> materialsPool;
        tr::Resources::Pool<tr::Data::Mesh> meshesPool;

        ResourceManager() = default;
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = default;
        ResourceManager& operator=(ResourceManager&&) = default;

        void clear() {
            shadersPool.clear();
            texturesPool.clear();
            materialsPool.clear();
            meshesPool.clear();
        }
    };
}
