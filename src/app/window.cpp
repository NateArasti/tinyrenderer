#include "window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace tr::App {
    Window::Window(int width, int height, std::string_view title)
        : _width(width), _height(height)
    {
        if (!glfwInit()) throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

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
        glfwSetFramebufferSizeCallback(
            static_cast<GLFWwindow*>(_window),
            [](GLFWwindow* glfwWindow, int width, int height) {
                auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
                window->_width = static_cast<uint32_t>(width);
                window->_height = static_cast<uint32_t>(height);
                window->_resized = true;
            }
        );
        glfwSetScrollCallback(
            static_cast<GLFWwindow*>(_window),
            [](GLFWwindow* w, double, double y) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                self->_scrollDelta += static_cast<float>(y);
            }
        );
    }

    Window::~Window() {
        if (_window) glfwDestroyWindow(static_cast<GLFWwindow*>(_window));
        glfwTerminate();
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(static_cast<GLFWwindow*>(_window));
    }

    void Window::pollEvents() {
        glfwPollEvents();
    }

    glm::vec2 Window::mousePos() const {
        double x, y;
        glfwGetCursorPos(static_cast<GLFWwindow*>(_window), &x, &y);
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    bool Window::isMouseButtonDown(int button) const {
        return glfwGetMouseButton(static_cast<GLFWwindow*>(_window), button) == GLFW_PRESS;
    }

    float Window::scrollDelta() {
        float val = _scrollDelta;
        _scrollDelta = 0.0f;
        return val;
    }
    
    bool Window::isKeyDown(int key) const {
        return glfwGetKey(static_cast<GLFWwindow*>(_window), key) == GLFW_PRESS;
    }

    void Window::captureCursor(bool captured) {
        glfwSetInputMode(static_cast<GLFWwindow*>(_window), GLFW_CURSOR,
            captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    float Window::deltaTime() {
        float now = static_cast<float>(glfwGetTime());
        _deltaTime = now - _lastTime;
        _lastTime = now;
        return _deltaTime;
    }
}
