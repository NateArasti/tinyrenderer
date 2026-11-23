#include "brdf_integration.h"

namespace tr::Data::EmbeddedShaders {
    BRDFIntegration::BRDFIntegration()
        : Shader("brdf_integration", getCode())
    {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
