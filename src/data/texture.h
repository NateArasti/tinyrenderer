#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace tr::Data {
    struct Texture {
        std::string name;
        std::vector<uint8_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
    };
}
