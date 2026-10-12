#pragma once

#include <cstdint>
#include <functional>

namespace tr::Data {
    template<typename T>
    struct Handle {
        static constexpr uint32_t kInvalid = 0xFFFFFFFF;

        uint32_t index = kInvalid;

        bool isValid() const { return index != kInvalid; }
        bool operator==(const Handle<T>&) const = default;

        operator uint32_t() const {
            return index;
        }
    };
}

namespace std {
    template<typename T>
    struct hash<tr::Data::Handle<T>> {
        size_t operator()(const tr::Data::Handle<T>& handle) const noexcept {
            return std::hash<uint64_t>{}(handle.index);
        }
    };
}
