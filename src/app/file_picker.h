#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tr::App {
    struct FilePickerFilter {
        std::string name;
        std::string pattern;
    };

    struct FilePickerOptions {
        std::string title;
        std::vector<FilePickerFilter> filters;
    };

    class FilePicker {
    public:
        virtual ~FilePicker() = default;

        virtual void requestFile(const FilePickerOptions& options) = 0;
        virtual std::optional<std::filesystem::path> pollResult() = 0;
    };

    std::unique_ptr<FilePicker> createFilePicker();
}
