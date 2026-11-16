#pragma once

#include <filesystem>

namespace tr::Loading {
    class SceneBuilder;

    void loadGLTF(const std::filesystem::path& path, SceneBuilder& builder);
    void loadFBX(const std::filesystem::path& path, SceneBuilder& builder);
    void loadOBJ(const std::filesystem::path& path, SceneBuilder& builder);
    void loadModel(const std::filesystem::path& path, SceneBuilder& builder);
}
