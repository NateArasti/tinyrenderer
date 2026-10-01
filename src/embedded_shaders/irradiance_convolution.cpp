#include "irradiance_convolution.h"

namespace tr::Data::EmbeddedShaders {
    IrradianceConvolution::IrradianceConvolution()
        : Shader("irradiance_convolution", getCode())
    {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
