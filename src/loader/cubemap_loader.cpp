#include "cubemap_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>

#include <stb_image.h>
#include <tinyexr.h>

#include "import_error.h"

namespace tr::Loading {
    namespace {
        std::unique_ptr<tr::Data::Cubemap> loadSTB(
            const std::filesystem::path& path,
            tr::Rendering::RenderingResources&
        ) {
            int width = 0;
            int height = 0;
            const bool hdr = stbi_is_hdr(path.string().c_str()) != 0;
            float* pixels = stbi_loadf(path.string().c_str(), &width, &height, nullptr, STBI_rgb_alpha);
            if (pixels == nullptr) {
                throw ImportError(
                    "Failed to load cubemap '" + path.string() + "': " + stbi_failure_reason()
                );
            }
            auto cubemap = std::make_unique<tr::Data::Cubemap>();
            cubemap->name = path.stem().string();
            cubemap->width = width;
            cubemap->height = height;
            cubemap->hdr = hdr;
            size_t totalPixelSize = static_cast<size_t>(width) * static_cast<size_t>(height) * STBI_rgb_alpha;
            cubemap->pixels.assign(pixels, pixels + totalPixelSize);
            stbi_image_free(pixels);
            return cubemap;
        }

        std::unique_ptr<tr::Data::Cubemap> loadEXR(
            const std::filesystem::path& path,
            tr::Rendering::RenderingResources&
        ) {
            float* rawPixels = nullptr;
            int width = 0;
            int height = 0;
            const char* error = nullptr;
            const int result = LoadEXR(
                &rawPixels,
                &width,
                &height,
                path.string().c_str(),
                &error
            );
            if (result != TINYEXR_SUCCESS) {
                const std::string message = error != nullptr ? error : "Unknown TinyEXR error";
                if (error != nullptr) {
                    FreeEXRErrorMessage(error);
                }
                throw ImportError(
                    "Failed to load cubemap '" + path.string() + "': " + message
                );
            }

            const std::unique_ptr<float, decltype(&std::free)> pixels(rawPixels, &std::free);
            auto cubemap = std::make_unique<tr::Data::Cubemap>();
            cubemap->name = path.stem().string();
            cubemap->width = width;
            cubemap->height = height;
            cubemap->hdr = true;
            const size_t totalPixelSize = static_cast<size_t>(width) * height * 4;
            cubemap->pixels.assign(pixels.get(), pixels.get() + totalPixelSize);
            return cubemap;
        }
    }

    std::unique_ptr<tr::Data::Cubemap> CubemapLoader::loadCubemap(
        const std::filesystem::path& path,
        tr::Rendering::RenderingResources& resources
    ) {
        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        if (extension == ".png" ||
            extension == ".jpg" ||
            extension == ".jpeg" ||
            extension == ".hdr"
        ) {
            return loadSTB(path, resources);
        }
        else if (extension == ".exr") {
            return loadEXR(path, resources);
        }
        throw ImportError("Unsupported cubemap format: " + extension);
    }
}
