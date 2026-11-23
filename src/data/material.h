#pragma once

#include <string>
#include <unordered_map>

#include "shader.h"

namespace tr::Data {
    struct Material {
        std::string name;
        BlendMode blendMode = BlendMode::Opaque;
        std::unordered_map<std::string, ShaderParamValue> params;

        Material& set(const std::string& paramName, ShaderParamValue value) {
            params[paramName] = std::move(value);
            return *this;
        }

        const ShaderParamValue& get(const std::string& paramName, const Shader& shader) const {
            auto it = params.find(paramName);
            if (it != params.end()) {
                return it->second;
            }
            for (const auto& desc : shader.params) {
                if (desc.name == paramName) {
                    return desc.defaultValue;
                }
            }
            throw std::runtime_error("Unknown shader param: " + paramName);
        }

        std::vector<std::byte> packMaterial(const Shader& shader) const {
            std::vector<std::byte> bytes;
            for (const auto& desc : shader.params) {
                const auto& value = get(desc.name, shader);
                const auto info = shader.getParamTypeInfo(value);
                if (!info.size) {
                    continue;
                }
                const auto offset = (bytes.size() + info.alignment - 1) & ~(size_t(info.alignment) - 1);
                bytes.resize(offset + info.size);
                std::visit(
                    [&](const auto& parameter) {
                        using T = std::decay_t<decltype(parameter)>;
                        if constexpr (!std::is_same_v<T, Handle<Texture>>) {
                            std::memcpy(bytes.data() + offset, &parameter, sizeof(parameter));
                        }
                    },
                    value);
            }
            bytes.resize(std::max(size_t(16), (bytes.size() + 15) & ~size_t(15)));
            return bytes;
        }
    };
}
