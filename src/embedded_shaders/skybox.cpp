#include "skybox.h"

namespace tr::Data::EmbeddedShaders {
    Skybox::Skybox()
        : Shader("skybox", getCode())
    {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
