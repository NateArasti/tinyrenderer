#pragma once

#include <string>
#include <cstdint>

namespace tr::App {
    class Window {
    private:
        uint32_t _width;
        uint32_t _height;
        void* _window = nullptr;

        bool _resized = false;

    public:
        Window(int width, int height, std::string_view title);
        ~Window();

        uint32_t width() const { return _width; }
        uint32_t height() const { return _height; }
        void* nativeHandle() const { return _window; }

        bool wasResized() const { return _resized; }
        void clearResizedFlag() { _resized = false; }

        bool shouldClose() const;
        void pollEvents();
    };
}
