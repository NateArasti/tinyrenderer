#pragma once

#include <string>
#include <filesystem>
#include <fstream>

namespace tr::Data {
    class Shader {
    private:
        const std::string _name;
        const std::filesystem::path _shaderPath;

        std::vector<char> _code;

    public:
        std::string vertName;
        std::string fragName;

        Shader(std::string name, std::filesystem::path path)
            : _name(name), _shaderPath(path) {
            
            std::ifstream file(_shaderPath, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open shader file " + name);
            }

            _code.resize(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(_code.data(), static_cast<std::streamsize>(_code.size()));
            file.close();
        }

        const auto& getCode() const { return _code; }
    };
}
