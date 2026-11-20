#pragma once

#include <string>
#include <cstdint>

#include <glm/glm.hpp>

namespace tr::App {
    enum class WindowMode {
        Windowed,
        WindowedFullscreen,
        Fullscreen
    };

    class Window {
    private:
        uint32_t _logicalWidth;
        uint32_t _logicalHeight;
        uint32_t _framebufferWidth;
        uint32_t _framebufferHeight;
        void* _window = nullptr;

        WindowMode _currentMode = WindowMode::Windowed;
        glm::ivec2 _windowedPos{};
        glm::ivec2 _windowedSize{};
        bool _windowedMaximized = false;
        
        float _lastTime = 0.0f;
        float _deltaTime = 0.0f;

        bool _resized = false;

    public:
        Window(int width, int height, std::string_view title, bool visible = true);
        ~Window();

        uint32_t width() const { return _framebufferWidth; }
        uint32_t height() const { return _framebufferHeight; }
        uint32_t logicalWidth() const { return _logicalWidth; }
        uint32_t logicalHeight() const { return _logicalHeight; }
        uint32_t framebufferWidth() const { return _framebufferWidth; }
        uint32_t framebufferHeight() const { return _framebufferHeight; }
        void* nativeHandle() const { return _window; }

        WindowMode mode() const { return _currentMode; }
        void setMode(WindowMode mode, glm::ivec2 fullscreenSize = {});

        bool wasResized() const { return _resized; }
        void clearResizedFlag() { _resized = false; }

        float deltaTime() const { return _deltaTime; }

        bool shouldClose() const;
        void pollEvents();
    };
}
