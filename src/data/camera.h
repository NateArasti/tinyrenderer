#pragma once

#include "gameobject.h"

namespace tr::Data {
    struct Camera {
        Transform transform;
        float fov;
        float near;
        float far;
    };
}
