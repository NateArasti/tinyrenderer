#include "shadow.h"

namespace tr::Data::EmbeddedShaders {
    Shadow::Shadow() : Shader("shadow", getCode()) {
        vertName = "main";
    }
}
