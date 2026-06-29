#pragma once

#include <string>
#include <cstdint>

#include <glm/glm.hpp>

namespace tr::App {
    class Window {
    private:
        uint32_t _width;
        uint32_t _height;
        void* _window = nullptr;
        
        float _lastTime = 0.0f;
        float _deltaTime = 0.0f;

        bool _resized = false;
        float _scrollDelta = 0.0f;

    public:
        Window(int width, int height, std::string_view title);
        ~Window();

        uint32_t width() const { return _width; }
        uint32_t height() const { return _height; }
        void* nativeHandle() const { return _window; }

        bool wasResized() const { return _resized; }
        void clearResizedFlag() { _resized = false; }

        float deltaTime();

        glm::vec2 mousePos() const;
        bool isMouseButtonDown(int button) const;
        float scrollDelta();
        bool isKeyDown(int key) const;
        void captureCursor(bool captured);

        bool shouldClose() const;
        void pollEvents();
    };
}
