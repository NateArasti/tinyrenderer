#include "orbit_controller.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace tr::Controllers {
    OrbitController::OrbitController(tr::Data::Camera& camera)
    {
        glm::vec3 offset = camera.transform.position - _target;
        _distance = glm::length(offset);
        _pitch = glm::degrees(std::asin(offset.y / _distance));
        _yaw = glm::degrees(std::atan2(offset.x, offset.z));
        applyToCamera(camera);
    }

    void OrbitController::applyToCamera(tr::Data::Camera& camera) const {
        float yawRad = glm::radians(_yaw);
        float pitchRad = glm::radians(_pitch);
        camera.transform.position = _target + glm::vec3(
            _distance * std::cos(pitchRad) * std::sin(yawRad),
            _distance * std::sin(pitchRad),
            _distance * std::cos(pitchRad) * std::cos(yawRad)
        );
        camera.transform.eulerAngles = glm::vec3(-_pitch, _yaw, 0.0f);
    }

    void OrbitController::update(tr::Data::Camera& camera, tr::App::Window& window) {
        glm::vec2 mousePos = window.mousePos();
        if (_firstFrame) {
            _prevMousePos = mousePos;
            _firstFrame = false;
        }

        glm::vec2 delta = mousePos - _prevMousePos;
        _prevMousePos = mousePos;

        bool dirty = false;

        if (window.isMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
            _yaw -= delta.x * sensitivity;
            _pitch = std::clamp(_pitch + delta.y * sensitivity, -89.0f, 89.0f);
            dirty = true;
        }

        float scroll = window.scrollDelta();
        if (scroll != 0.0f) {
            _distance = std::clamp(_distance - scroll * zoomSensitivity, 0.1f, maxRadius);
            dirty = true;
        }

        if (dirty) {
            applyToCamera(camera);
        }
    }
}
