#pragma once

#include <cstdint>

namespace tr::App {
    struct MemoryUsage {
        uint64_t usedBytes = 0;
        uint64_t totalBytes = 0;
    };
}
