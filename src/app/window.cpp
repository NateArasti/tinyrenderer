#include "window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace tr::App {
    Window::Window(int width, int height, std::string_view title)
        : _logicalWidth(width),
          _logicalHeight(height),
          _framebufferWidth(width),
          _framebufferHeight(height)
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
