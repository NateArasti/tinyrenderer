#pragma once

#include "gameobject.h"

namespace tr::Data {
    struct Camera : public GameObject {
        float fov;
        float near;
        float far;
    };
}
