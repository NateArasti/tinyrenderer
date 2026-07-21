#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tr::App {
    struct SelectedFile {
        std::string name;
        std::vector<std::byte> content;
    };

    class FilePicker {
    public:
        virtual ~FilePicker() = default;

        virtual void requestModelFile() = 0;
        virtual std::optional<SelectedFile> pollResult() = 0;
    };

    std::unique_ptr<FilePicker> createFilePicker();
}
