#pragma once

#include "camera_controller.h"
#include "camera.h"
#include "window.h"

namespace tr::Controllers {
    class FreeMoveController : public CameraController {
    private:
        glm::vec2 _prevMousePos{0.0f};
        bool _firstFrame = true;
        bool _captured = false;

    public:
        float speed = 5.0f;
        float sensitivity = 0.1f;

        explicit FreeMoveController(tr::Data::Camera& camera);
        void update(tr::Data::Camera& camera, tr::App::Window& window) override;
    };
}
