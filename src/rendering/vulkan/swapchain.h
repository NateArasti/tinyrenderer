#pragma once

#include <cstddef>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"

namespace tr::Rendering::Vulkan {
    class Swapchain {
    private:
        struct SwapchainImage {
            vk::Image image;
            vk::raii::ImageView view = nullptr;
            vk::raii::Semaphore renderFinished = nullptr;
        };

        VulkanContext& _context;
        vk::raii::SwapchainKHR _swapchain = nullptr;
        std::vector<SwapchainImage> _images;
        vk::Format _format = vk::Format::eUndefined;
        vk::Extent2D _extent{};

    public:
        Swapchain(VulkanContext& context, uint32_t width, uint32_t height);

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        void recreate(uint32_t width, uint32_t height);

        const vk::raii::SwapchainKHR& handle() const { return _swapchain; }
        vk::Format format() const { return _format; }
        vk::Extent2D extent() const { return _extent; }
        size_t imageCount() const { return _images.size(); }

        vk::Image image(uint32_t index) const { return _images.at(index).image; }
        const vk::raii::ImageView& imageView(uint32_t index) const { return _images.at(index).view; }
        const vk::raii::Semaphore& renderFinished(uint32_t index) const {
            return _images.at(index).renderFinished;
        }
    };
}
