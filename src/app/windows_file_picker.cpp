#include "file_picker.h"

#include <filesystem>
#include <utility>

#include <windows.h>
#include <shobjidl.h>

namespace tr::App {
    namespace {
        class WindowsFilePicker final : public FilePicker {
        private:
            std::optional<std::filesystem::path> _result;

        public:
            void requestModelFile() override {
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
                    const COMDLG_FILTERSPEC filters[] = {
                        { L"3D models", L"*.obj;*.gltf;*.glb;*.fbx" },
                        { L"All files", L"*.*" }
                    };
                    dialog->SetFileTypes(std::size(filters), filters);

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
