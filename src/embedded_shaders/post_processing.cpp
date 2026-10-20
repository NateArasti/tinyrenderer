#include "post_processing.h"

namespace tr::Data::EmbeddedShaders {
    PostProcessing::PostProcessing() : Shader("post_processing", getCode()) {
        vertName = "vertMain";
        fragName = "fragMain";
    }
}
