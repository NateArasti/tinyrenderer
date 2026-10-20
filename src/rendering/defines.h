#pragma once

#include "rendering_structs.h"

namespace tr::Rendering {
    inline constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
    inline constexpr Format HDR_FORMAT = Format::R16G16B16A16SFloat;
}
