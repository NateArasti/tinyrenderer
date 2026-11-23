#include "prefilter_environment.h"

namespace tr::Data::EmbeddedShaders {
    PrefilterEnvironment::PrefilterEnvironment()
        : Shader("prefilter_environment", getCode())
    {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
