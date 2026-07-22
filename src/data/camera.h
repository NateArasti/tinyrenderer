#pragma once

#include "gameobject.h"

namespace tr::Data {
    enum class CameraProjection {
        Perspective,
        Orthographic
    };

    struct Camera {
        Transform transform;
        CameraProjection projection = CameraProjection::Perspective;
        float fov = 60.0f;
        float orthographicSize = 10.0f;
        float near = 0.1f;
        float far = 50.0f;
    };
}
