#include "tiny_renderer.h"

#include <filesystem>
#include <utility>

#include <fmt/base.h>

#include "cubemap_loader.h"
#include "file_picker.h"
#include "import_error.h"
#include "model_loader.h"
#include "renderer.h"
#include "scene_builder.h"

namespace tr {
    void TinyRenderer::setScene(std::unique_ptr<const Data::Scene> scene) {
        if (currentScene) {
            renderer->unloadSceneResources(*currentScene);
        }
        currentScene = std::move(scene);
        sceneScale = 1.0f;
        renderer->prepareScene(*currentScene);
        needToUpdateSceneMatrix = true;
    }

    void TinyRenderer::loadScene(SceneLoader load) {
        Loading::SceneBuilder builder(renderer->getResources());
        load(builder);
        setScene(builder.build());
    }

    void TinyRenderer::loadSkybox() {
        auto filePicker = App::createFilePicker();
        filePicker->requestFile({
            .title = "Load Skybox",
            .filters = {
                { "Images", "*.png;*.jpg;*.jpeg;*.hdr;*.exr" },
                { "All files", "*.*" }
            }
        });
        auto path = filePicker->pollResult();

        if (!path) {
            fmt::println("No file was selected");
            return;
        }

        fmt::println("Loading file {}", path->string());

        renderer->getResources().destroyCubemap(environment.skyboxHandle);
        environment.skyboxHandle = {};

        try {
            environment.skyboxHandle = tr::Loading::loadCubemap(path.value(), renderer->getResources());
        }
        catch (const Loading::ImportError& error) {
            importError = error.what();
            fmt::println("Skybox import failed: {}", importError);
            importErrorPopupPending = true;
            showUI = true;
            syncSceneUI();
            return;
        }
    }

    void TinyRenderer::loadSelectedModel() {
        auto filePicker = App::createFilePicker();
        filePicker->requestFile({
            .title = "Load Model",
            .filters = {
                { "3D models", "*.obj;*.gltf;*.glb;*.fbx" },
                { "All files", "*.*" }
            }
        });
        auto path = filePicker->pollResult();

        if (!path) {
            fmt::println("No file was selected");
            return;
        }

        fmt::println("Loading file {}", path->string());
        try {
            Loading::SceneBuilder builder(renderer->getResources());
            Loading::loadModel(*path, builder);
            setScene(builder.build());
        }
        catch (const Loading::ImportError& error) {
            importError = error.what();
            fmt::println("Model import failed: {}", importError);
            importErrorPopupPending = true;
            showUI = true;
            syncSceneUI();
            return;
        }

        modelName = path->filename().string();
        syncSceneUI();
    }
}

