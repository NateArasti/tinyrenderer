#pragma once

#include <stdexcept>

namespace tr::Loading {
    class ImportError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
}
