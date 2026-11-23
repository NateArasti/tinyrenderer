#include "model_loader.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "import_error.h"

namespace tr::Loading {
    void loadModel(const std::filesystem::path& path, SceneBuilder& builder) {
        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        if (extension == ".gltf" || extension == ".glb") {
            loadGLTF(path, builder);
        }
        else if (extension == ".fbx") {
            loadFBX(path, builder);
        }
        else if (extension == ".obj") {
            loadOBJ(path, builder);
        }
        else {
            throw ImportError("File format is not supported");
        }
    }
}
