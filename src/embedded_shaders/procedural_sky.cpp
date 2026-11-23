#include "procedural_sky.h"

namespace tr::Data::EmbeddedShaders {
    ProceduralSky::ProceduralSky() : Shader("procedural_sky", getCode()) {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
