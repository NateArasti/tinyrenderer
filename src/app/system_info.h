#pragma once

#include <optional>
#include <string>

#include "memory_usage.h"

namespace tr::App {
    std::string getCpuName();
    std::optional<MemoryUsage> getRamUsage();
}
