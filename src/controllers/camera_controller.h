#pragma once

#include "camera.h"
#include "input.h"

namespace tr::Controllers {
    class CameraController {
    public:
        virtual ~CameraController() = default;
        virtual void update(tr::Data::Camera& camera, tr::App::Input& input, float deltaTime) = 0;
    };
}
