#include "procedural_sky_render.h"

namespace tr::Data::EmbeddedShaders {
    ProceduralSkyRender::ProceduralSkyRender() : Shader("procedural_sky_render", getCode()) {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
