#include "system_info.h"

#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <intrin.h>

namespace tr::App {
    std::string getCpuName() {
        int registers[4]{};
        __cpuid(registers, static_cast<int>(0x80000000u));
        if (static_cast<unsigned>(registers[0]) < 0x80000004u) {
            return "Unknown CPU";
        }

        char brand[49]{};
        for (unsigned i = 0; i < 3; ++i) {
            __cpuid(registers, static_cast<int>(0x80000002u + i));
            std::memcpy(brand + i * 16, registers, 16);
        }

        const std::string name(brand);
        const auto first = name.find_first_not_of(' ');
        if (first == std::string::npos) {
            return "Unknown CPU";
        }
        return name.substr(first, name.find_last_not_of(' ') - first + 1);
    }

    std::optional<MemoryUsage> getRamUsage() {
        PROCESS_MEMORY_COUNTERS process{};
        process.cb = sizeof(process);
        MEMORYSTATUSEX system{};
        system.dwLength = sizeof(system);
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &process, sizeof(process)) || !GlobalMemoryStatusEx(&system)) {
            return std::nullopt;
        }
        return MemoryUsage{
            .usedBytes = static_cast<uint64_t>(process.WorkingSetSize),
            .totalBytes = system.ullTotalPhys,
        };
    }
}
