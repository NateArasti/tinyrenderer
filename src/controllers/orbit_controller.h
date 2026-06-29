#pragma once

#include <glm/glm.hpp>
#include "camera_controller.h"

namespace tr::Controllers {
    class OrbitController : public CameraController {
    private:
        glm::vec3 _target{0.0f};
        float _yaw;
        float _pitch;
        float _distance;
        glm::vec2 _prevMousePos{0.0f};
        bool _firstFrame = true;

        void applyToCamera(tr::Data::Camera& camera) const;

    public:
        float maxRadius = 50.0f;
        float sensitivity = 0.2f;
        float zoomSensitivity = 0.75f;

        explicit OrbitController(tr::Data::Camera& camera);
        void update(tr::Data::Camera& camera, tr::App::Window& window) override;
    };
}
