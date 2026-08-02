#include "equirectangular_conversion.h"

namespace tr::Data::EmbeddedShaders {
    EquirectangularConversion::EquirectangularConversion()
        : Shader("equirectangular_conversion", getCode())
    {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
