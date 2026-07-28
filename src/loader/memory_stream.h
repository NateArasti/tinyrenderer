#pragma once

#include <cstddef>
#include <istream>
#include <span>
#include <streambuf>

namespace tr::Loading {
    class MemoryStream final : private std::streambuf, public std::istream {
    public:
        explicit MemoryStream(std::span<const std::byte> content)
            : std::istream(this) {
            const auto* begin = reinterpret_cast<const char*>(content.data());
            const auto* end = begin + content.size();

            setg(
                const_cast<char*>(begin),
                const_cast<char*>(begin),
                const_cast<char*>(end)
            );
        }
    };
}
