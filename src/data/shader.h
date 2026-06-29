#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "handle.h"
#include "texture.h"

namespace tr::Data {
    using ShaderParamValue = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        Resources::Handle<Data::Texture>
    >;

    struct ShaderParamDesc {
        std::string name;
        ShaderParamValue defaultValue;
    };

    enum class BlendMode { Opaque, Transparent };
    enum class CullMode { None, Front, Back, Both };

    class Shader {
    private:
        const std::string _name;
        const std::filesystem::path _shaderPath;

        std::vector<char> _code;

    public:
        std::string vertName;
        std::string fragName;
        std::vector<ShaderParamDesc> params;

        BlendMode blendMode = BlendMode::Opaque;
        CullMode cullMode = CullMode::Back;

        Shader(std::string name, std::filesystem::path path)
            : _name(name), _shaderPath(path) {
            
            std::ifstream file(_shaderPath, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open shader file " + name);
            }

            _code.resize(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(_code.data(), static_cast<std::streamsize>(_code.size()));
        }

        const auto& getCode() const { return _code; }

        struct TypeInfo {
            uint32_t size;
            uint32_t alignment;
        };

        inline TypeInfo getParamTypeInfo(const ShaderParamValue& v) const {
            return std::visit([](auto&& val) -> TypeInfo {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, float>) return { 4,  4 };
                if constexpr (std::is_same_v<T, glm::vec2>) return { 8,  8 };
                if constexpr (std::is_same_v<T, glm::vec3>) return { 12, 16 };
                if constexpr (std::is_same_v<T, glm::vec4>) return { 16, 16 };
                if constexpr (std::is_same_v<T, Resources::Handle<Data::Texture>>) return { 0, 0 };
            }, v);
        }

        inline bool isTextureParam(const ShaderParamValue& v) const {
            return std::holds_alternative<Resources::Handle<Data::Texture>>(v);
        }
    };
}
