#pragma once

#include <vector>

#include "shader.h"

namespace tr::Data::EmbeddedShaders {
    class PrefilterEnvironment final : public Shader {
    private:
        static std::vector<char> getCode();

    public:
        PrefilterEnvironment();
    };
}
