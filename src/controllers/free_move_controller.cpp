#include "free_move_controller.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

#include "camera.h"
#include "input.h"

namespace tr::Controllers {
    FreeMoveController::FreeMoveController(tr::Data::Camera& camera) {}

    void FreeMoveController::update(tr::Data::Camera& camera, tr::App::Input& input, float deltaTime) {
        glm::vec2 mousePos = input.mousePosition();
        if (_firstFrame) {
            _prevMousePos = mousePos;
            _firstFrame = false;
        }

        bool rmb = input.isMouseButtonDown(tr::App::MouseButton::Right);

        if (rmb && !_captured) {
            input.captureCursor(true);
            _captured = true;
            _prevMousePos = mousePos;
        } else if (!rmb && _captured) {
            input.captureCursor(false);
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

        if (input.isKeyPressed(tr::App::Key::W)) move += forward;
        if (input.isKeyPressed(tr::App::Key::S)) move -= forward;
        if (input.isKeyPressed(tr::App::Key::D)) move += right;
        if (input.isKeyPressed(tr::App::Key::A)) move -= right;
        if (input.isKeyPressed(tr::App::Key::E)) move += glm::vec3(0, 1, 0);
        if (input.isKeyPressed(tr::App::Key::Q)) move -= glm::vec3(0, 1, 0);

        if (glm::length(move) > 0.0f) {
            camera.transform.position += glm::normalize(move) * speed * deltaTime;
        }
    }
}
