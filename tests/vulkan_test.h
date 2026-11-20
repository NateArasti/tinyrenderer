#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "application.h"
#include "input.h"
#include "ui_provider.h"
#include "vulkan_rhi.h"

namespace tr::Tests {
    class VulkanTest : public testing::Test {
    private:
        std::mutex _validationMutex;
        std::vector<std::string> _validationMessages;
        std::unique_ptr<UI::UIProvider> _uiProvider;

    protected:
        std::unique_ptr<App::Application> _application;
        std::unique_ptr<App::Input> _input;
        std::unique_ptr<Rendering::Vulkan::VulkanRHI> _rhi;

        void setUpVulkan(std::string_view name, std::string_view title) {
            _application = std::make_unique<App::Application>(App::Application{
                .name = name,
                .version = "0.0.1",
                .window = std::make_unique<App::Window>(640, 480, title, false)
            });
            _input = std::make_unique<App::Input>(*_application->window);
            _uiProvider = std::make_unique<UI::UIProvider>(*_application->window, *_input);

            Rendering::Vulkan::ValidationOptions validation{
                .enabled = true,
                .synchronizationValidation = true,
                .onMessage = [this](auto, std::string_view message) {
                    std::lock_guard lock(_validationMutex);
                    _validationMessages.emplace_back(message);
                }
            };
            _rhi = std::make_unique<Rendering::Vulkan::VulkanRHI>(*_application, std::move(validation));
        }

        void TearDown() override {
            _rhi.reset();
            _uiProvider.reset();
            _input.reset();
            _application.reset();

            std::lock_guard lock(_validationMutex);
            for (const auto& message : _validationMessages) {
                ADD_FAILURE() << message;
            }
        }
    };
}
