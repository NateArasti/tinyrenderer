#pragma once

#include <memory>

#include "window.h"

namespace tr::App {
    struct Application {
        std::string_view name;
        std::string_view version;
        std::unique_ptr<Window> window;
    };
}
