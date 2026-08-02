#pragma once

#include <string>
#include <vector>

namespace tr::Data {
    struct Cubemap {
        std::string name;
        std::vector<float> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        bool hdr = false;
    };
}
