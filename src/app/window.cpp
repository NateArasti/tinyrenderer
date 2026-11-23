#include "window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace tr::App {
    Window::Window(int width, int height, std::string_view title, bool visible)
        : _logicalWidth(width),
          _logicalHeight(height),
          _framebufferWidth(width),
          _framebufferHeight(height)
    {
        if (!glfwInit()) throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

        _window = glfwCreateWindow(
            static_cast<int>(width),
            static_cast<int>(height),
            title.data(),
            nullptr,
            nullptr
        );

        if (!_window) {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwSetWindowUserPointer(static_cast<GLFWwindow*>(_window), this);
        glfwSetWindowSizeCallback(
            static_cast<GLFWwindow*>(_window),
            [](GLFWwindow* glfwWindow, int width, int height) {
                auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
                window->_logicalWidth = static_cast<uint32_t>(width);
                window->_logicalHeight = static_cast<uint32_t>(height);
            }
        );
        glfwSetFramebufferSizeCallback(
            static_cast<GLFWwindow*>(_window),
            [](GLFWwindow* glfwWindow, int width, int height) {
                auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
                window->_framebufferWidth = static_cast<uint32_t>(width);
                window->_framebufferHeight = static_cast<uint32_t>(height);
                window->_resized = true;
            }
        );
        int logicalWidth;
        int logicalHeight;
        glfwGetWindowSize(static_cast<GLFWwindow*>(_window), &logicalWidth, &logicalHeight);
        _logicalWidth = static_cast<uint32_t>(logicalWidth);
        _logicalHeight = static_cast<uint32_t>(logicalHeight);

        int framebufferWidth;
        int framebufferHeight;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(_window), &framebufferWidth, &framebufferHeight);
        _framebufferWidth = static_cast<uint32_t>(framebufferWidth);
        _framebufferHeight = static_cast<uint32_t>(framebufferHeight);

        _lastTime = static_cast<float>(glfwGetTime());
    }

    Window::~Window() {
        if (_window) glfwDestroyWindow(static_cast<GLFWwindow*>(_window));
        glfwTerminate();
    }

    void Window::setMode(WindowMode mode, glm::ivec2 fullscreenSize) {
        if (_currentMode == mode) {
            return;
        }

        auto* window = static_cast<GLFWwindow*>(_window);
        auto* attachedMonitor = glfwGetWindowMonitor(window);
        auto* monitor = attachedMonitor ? attachedMonitor : glfwGetPrimaryMonitor();
        if (mode != WindowMode::Windowed && !monitor) {
            return;
        }

        if (_currentMode == WindowMode::Windowed) {
            _windowedMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
            if (_windowedMaximized) {
                glfwRestoreWindow(window);
            }

            glfwGetWindowPos(window, &_windowedPos.x, &_windowedPos.y);
            glfwGetWindowSize(window, &_windowedSize.x, &_windowedSize.y);
        }

        const auto restoreWindowedBounds = [&] {
            glfwSetWindowMonitor(
                window,
                nullptr,
                _windowedPos.x, _windowedPos.y,
                _windowedSize.x, _windowedSize.y,
                GLFW_DONT_CARE
            );
        };

        if (mode == WindowMode::Fullscreen) {
            const auto* desktop = glfwGetVideoMode(monitor);
            if (!desktop) {
                return;
            }

            const bool useDesktopSize = fullscreenSize.x <= 0 || fullscreenSize.y <= 0;
            glfwSetWindowAttrib(window, GLFW_AUTO_ICONIFY, GLFW_FALSE);
            glfwSetWindowMonitor(
                window,
                monitor,
                0, 0,
                useDesktopSize ? desktop->width : fullscreenSize.x, useDesktopSize ? desktop->height : fullscreenSize.y,
                useDesktopSize ? desktop->refreshRate : GLFW_DONT_CARE
            );
            _currentMode = mode;
            return;
        }

        if (mode == WindowMode::WindowedFullscreen) {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
            if (attachedMonitor) {
                restoreWindowedBounds();
            }

            if (const auto* desktop = glfwGetVideoMode(monitor)) {
                int x, y;
                glfwGetMonitorPos(monitor, &x, &y);
                glfwSetWindowMonitor(
                    window,
                    nullptr, x, y,
                    desktop->width, desktop->height,
                    GLFW_DONT_CARE
                );
                _currentMode = mode;
                return;
            }
        }

        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
        restoreWindowedBounds();
        if (_windowedMaximized) {
            glfwMaximizeWindow(window);
        }
        _currentMode = WindowMode::Windowed;
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(static_cast<GLFWwindow*>(_window));
    }

    void Window::pollEvents() {
        glfwPollEvents();

        const float now = static_cast<float>(glfwGetTime());
        _deltaTime = now - _lastTime;
        _lastTime = now;
    }
}
