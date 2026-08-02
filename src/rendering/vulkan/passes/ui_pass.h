#pragma once

#include <imgui.h>

#include "vulkan_context.h"
#include "swapchain.h"

namespace tr::Rendering::Vulkan {
    class UIPass {
    private:
        VulkanContext& _vulkanContext;
        bool _initialized = false;

        void initialize(const Swapchain& swapchain);
        void shutdown();

    public:
        explicit UIPass(VulkanContext& context, Swapchain& swapchain);
        ~UIPass();

        UIPass(const UIPass&) = delete;
        UIPass& operator=(const UIPass&) = delete;
        UIPass(UIPass&&) = delete;
        UIPass& operator=(UIPass&&) = delete;

        void recreate(const Swapchain& swapchain);
        void beginFrame();
        void draw(
            const vk::raii::CommandBuffer& commands,
            vk::ImageView target,
            vk::Extent2D extent,
            ImDrawData* drawData
        );
    };
}
