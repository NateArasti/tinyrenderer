#include "file_picker.h"

#include <filesystem>
#include <utility>

#include <windows.h>
#include <shobjidl.h>

namespace tr::App {
    namespace {
        std::wstring widen(const std::string& value) {
            if (value.empty()) return {};
            const int size = MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0
            );
            std::wstring result(size, L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size
            );
            return result;
        }

        class WindowsFilePicker final : public FilePicker {
        private:
            std::optional<std::filesystem::path> _result;

        public:
            void requestFile(const FilePickerOptions& options) override {
                _result.reset();

                const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE) return;

                IFileOpenDialog* dialog = nullptr;
                const HRESULT createResult = CoCreateInstance(
                    CLSID_FileOpenDialog,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&dialog)
                );
                if (SUCCEEDED(createResult)) {
                    const std::wstring title = widen(options.title);
                    if (!title.empty()) dialog->SetTitle(title.c_str());

                    std::vector<std::wstring> names;
                    std::vector<std::wstring> patterns;
                    names.reserve(options.filters.size());
                    patterns.reserve(options.filters.size());
                    for (const auto& filter : options.filters) {
                        names.push_back(widen(filter.name));
                        patterns.push_back(widen(filter.pattern));
                    }
                    std::vector<COMDLG_FILTERSPEC> filters;
                    filters.reserve(options.filters.size());
                    for (size_t i = 0; i < options.filters.size(); ++i) {
                        filters.push_back({ names[i].c_str(), patterns[i].c_str() });
                    }
                    if (!filters.empty()) {
                        dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
                    }

                    if (SUCCEEDED(dialog->Show(nullptr))) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(dialog->GetResult(&item))) {
                            PWSTR path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                                _result = std::filesystem::path(path);
                                CoTaskMemFree(path);
                            }
                            item->Release();
                        }
                    }
                    dialog->Release();
                }

                if (SUCCEEDED(initializeResult)) {
                    CoUninitialize();
                }
            }

            std::optional<std::filesystem::path> pollResult() override {
                return std::exchange(_result, std::nullopt);
            }
        };
    }

    std::unique_ptr<FilePicker> createFilePicker() {
        return std::make_unique<WindowsFilePicker>();
    }
}
