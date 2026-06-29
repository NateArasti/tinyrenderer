#pragma once

#include "camera.h"
#include "window.h"

namespace tr::Controllers {
    class CameraController {
    public:
        virtual ~CameraController() = default;
        virtual void update(tr::Data::Camera& camera, tr::App::Window& window) = 0;
    };
}
