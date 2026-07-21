#pragma once

#include <memory>

#include "ui_data.h"
#include "input.h"
#include "window.h"

namespace tr::UI {
    class UIProvider {
    private:
        struct Impl;

        tr::App::Window& _window;
        tr::App::Input& _input;
        std::unique_ptr<Impl> _impl;

    public:
        UIProvider(tr::App::Window& window, tr::App::Input& input);
        ~UIProvider();

        UIProvider(const UIProvider&) = delete;
        UIProvider& operator=(const UIProvider&) = delete;
        UIProvider(UIProvider&&) = delete;
        UIProvider& operator=(UIProvider&&) = delete;

        UIFrame update(UIState& state);
    };
}
