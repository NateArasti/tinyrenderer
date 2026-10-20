#pragma once

namespace tr::Data {
    enum Tonemapping : int32_t {
        None = 0,
        Reinhard = 1,
        Filmic = 2,
        ACES = 3
    };

    inline const char* toneMappingNames[4] = {
        "None",
        "Reinhard",
        "Filmic",
        "ACES"
    };

    struct alignas(16) PostProcessingData {
        int32_t exposure = 0;
        Tonemapping tonemapping = Tonemapping::None;
    };
}
