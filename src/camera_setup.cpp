#include "tiny_renderer.h"

#include <cmath>
#include <utility>

#include "free_move_controller.h"
#include "orbit_controller.h"

namespace tr {
    void TinyRenderer::setupCamera() {
        camera.transform.position = glm::vec3(-12.0f, 15.0f, -12.0f);
        camera.transform.eulerAngles = glm::vec3(40.0f, 45.0f, 0.0f);
        camera.fov = 60;
        camera.near = 0.1f;
        camera.far = 150;
        setCameraController(CameraControllerType::FreeMove);
    }

    void TinyRenderer::setCameraController(CameraControllerType type) {
        if (cameraController) {
            if (cameraControllerType == CameraControllerType::FreeMove) {
                const auto& freeMove = static_cast<const Controllers::FreeMoveController&>(*cameraController);
                freeMoveSpeed = freeMove.speed;
                freeMoveShiftMultiplier = freeMove.shiftMultiplier;
                freeMoveSensitivity = freeMove.sensitivity;
            }
            else {
                const auto& orbit = static_cast<const Controllers::OrbitController&>(*cameraController);
                orbitMaxRadius = orbit.maxRadius;
                orbitSensitivity = orbit.sensitivity;
                orbitZoomSensitivity = orbit.zoomSensitivity;
            }
        }

        cameraControllerType = type;
        if (type == CameraControllerType::FreeMove) {
            auto controller = std::make_unique<Controllers::FreeMoveController>(camera);
            controller->speed = freeMoveSpeed;
            controller->shiftMultiplier = freeMoveShiftMultiplier;
            controller->sensitivity = freeMoveSensitivity;
            cameraController = std::move(controller);
        }
        else {
            auto controller = std::make_unique<Controllers::OrbitController>(camera);
            controller->maxRadius = orbitMaxRadius;
            controller->sensitivity = orbitSensitivity;
            controller->zoomSensitivity = orbitZoomSensitivity;
            cameraController = std::move(controller);
        }
    }

    void TinyRenderer::updateLightDirection() {
        const float yaw = glm::radians(lightYaw);
        const float pitch = glm::radians(lightPitch);
        environment.directionalLight.direction = glm::vec3(
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::cos(yaw)
        );
    }
}

