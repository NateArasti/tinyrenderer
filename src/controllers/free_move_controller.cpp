#include "free_move_controller.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

#include "camera.h"
#include "window.h"

namespace tr::Controllers {
    FreeMoveController::FreeMoveController(tr::Data::Camera& camera) {}

    void FreeMoveController::update(tr::Data::Camera& camera, tr::App::Window& window) {
        float dt = window.deltaTime();
        
        glm::vec2 mousePos = window.mousePos();
        if (_firstFrame) {
            _prevMousePos = mousePos;
            _firstFrame = false;
        }

        bool rmb = window.isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);

        if (rmb && !_captured) {
            window.captureCursor(true);
            _captured = true;
            _prevMousePos = mousePos;
        } else if (!rmb && _captured) {
            window.captureCursor(false);
            _captured = false;
        }

        if (_captured) {
            glm::vec2 delta = mousePos - _prevMousePos;
            camera.transform.eulerAngles.x = std::clamp(
                camera.transform.eulerAngles.x - delta.y * sensitivity, -89.0f, 89.0f
            );
            camera.transform.eulerAngles.y -= delta.x * sensitivity;
        }

        _prevMousePos = mousePos;

        float yawRad = glm::radians(camera.transform.eulerAngles.y);
        float pitchRad = glm::radians(-camera.transform.eulerAngles.x);

        glm::vec3 forward = glm::normalize(glm::vec3(
            -std::cos(pitchRad) * std::sin(yawRad),
            -std::sin(pitchRad),
            -std::cos(pitchRad) * std::cos(yawRad)
        ));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

        glm::vec3 move{0.0f};

        if (window.isKeyDown(GLFW_KEY_W)) move += forward;
        if (window.isKeyDown(GLFW_KEY_S)) move -= forward;
        if (window.isKeyDown(GLFW_KEY_D)) move += right;
        if (window.isKeyDown(GLFW_KEY_A)) move -= right;
        if (window.isKeyDown(GLFW_KEY_E)) move += glm::vec3(0, 1, 0);
        if (window.isKeyDown(GLFW_KEY_Q)) move -= glm::vec3(0, 1, 0);

        if (glm::length(move) > 0.0f) {
            camera.transform.position += glm::normalize(move) * speed * dt;
        }
    }
}
