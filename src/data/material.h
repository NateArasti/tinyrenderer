#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include <glm/glm.hpp>

#include "handle.h"
#include "shader.h"
#include "texture.h"

namespace tr::Data {
    struct Material {
    private:
        using ParamValue = std::variant<
            float,
            int32_t,
            glm::vec2,
            glm::vec3,
            glm::vec4,
            glm::mat4
        >;
        
        Resources::Handle<Data::Shader> _shader;
        std::unordered_map<std::string, ParamValue> _params;
        std::unordered_map<std::string, Resources::Handle<Data::Texture>> _textures;

    public:
        std::string name;
        
        explicit Material(Resources::Handle<Data::Shader> shader) : _shader(shader) {}

        const auto getShader() const { return _shader; }
        const auto& getParams() const { return _params; }
        const auto& getTextures() const { return _textures; }

        void setParam(std::string_view name, ParamValue value) {
            _params[std::string(name)] = value;
        }

        const bool tryGetParam(std::string_view name, ParamValue& value) const {
            auto it = _params.find(std::string(name));
            if (it == _params.end()) {
                return false;
            }
            value = it->second;
            return true;
        }
        
        void setTexture(std::string_view name, Resources::Handle<Data::Texture> texture) {
            _textures[std::string(name)] = texture;
        }
    };
}
