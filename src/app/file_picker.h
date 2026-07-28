#pragma once

#include <filesystem>
#include <memory>
#include <optional>

namespace tr::App {
    class FilePicker {
    public:
        virtual ~FilePicker() = default;

        virtual void requestModelFile() = 0;
        virtual std::optional<std::filesystem::path> pollResult() = 0;
    };

    std::unique_ptr<FilePicker> createFilePicker();
}
