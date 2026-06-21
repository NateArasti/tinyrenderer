#pragma once

#include <cstdint>

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
