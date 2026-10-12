#pragma once

#include "ui_renderer.h"
#include "vulkan_context.h"
#include "swapchain.h"

namespace tr::Rendering::Vulkan {
    class VulkanUIRenderer : public UIRenderer {
    private:
        VulkanContext& _context;
        const Swapchain& _swapchain;
        bool _initialized = false;

        void initialize();
        void shutdown();

    public:
        VulkanUIRenderer(VulkanContext& context, const Swapchain& swapchain);
        ~VulkanUIRenderer() override;

        VulkanUIRenderer(const VulkanUIRenderer&) = delete;
        VulkanUIRenderer& operator=(const VulkanUIRenderer&) = delete;
        VulkanUIRenderer(VulkanUIRenderer&&) = delete;
        VulkanUIRenderer& operator=(VulkanUIRenderer&&) = delete;

        void recreate();
        void beginFrame() override;
        void draw(CommandBuffer& cmd, ImDrawData* drawData) override;
    };
}
