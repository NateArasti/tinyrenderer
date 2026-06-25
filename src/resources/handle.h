#pragma once

#include <cstdint>
#include <functional>

namespace tr::Resources {
    template<typename T>
    struct Handle {
        static constexpr uint32_t kInvalid = 0xFFFFFFFF;

        uint32_t index = kInvalid;
        uint32_t generation = 0;

        bool isValid() const { return index != kInvalid; }
        bool operator==(const Handle<T>&) const = default;
    };
}

namespace std {
    template<typename T>
    struct hash<tr::Resources::Handle<T>> {
        size_t operator()(const tr::Resources::Handle<T>& handle) const noexcept {
            uint64_t packed = (static_cast<uint64_t>(handle.generation) << 32) | handle.index;
            return std::hash<uint64_t>{}(packed);
        }
    };
}
