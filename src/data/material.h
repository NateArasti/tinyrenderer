#pragma once

#include <string>
#include <unordered_map>

#include "handle.h"
#include "shader.h"
#include "texture.h"

namespace tr::Data {
    struct Material {
        std::string name;
        BlendMode blendMode = BlendMode::Opaque;
        std::unordered_map<std::string, ShaderParamValue> params;

        Material& set(const std::string& paramName, ShaderParamValue value) {
            params[paramName] = std::move(value);
            return *this;
        }

        const ShaderParamValue& get(const std::string& paramName, const Shader& shaderDef) const {
            auto it = params.find(paramName);
            if (it != params.end()) {
                return it->second;
            }
            for (const auto& desc : shaderDef.params) {
                if (desc.name == paramName) {
                    return desc.defaultValue;
                }
            }
            throw std::runtime_error("Unknown shader param: " + paramName);
        }
    };
}
