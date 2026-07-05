#pragma once

#include <vector>

#include "shader.h"

namespace tr::Data::EmbeddedShaders {
    class Shadow final : public Shader {
    private:
        static std::vector<char> getCode();

    public:
        Shadow();
    };
}
