#include "swapchain.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tr::Rendering::Vulkan {
    Swapchain::Swapchain(VulkanContext& context, uint32_t width, uint32_t height)
        : _context(context)
    {
        recreate(width, height);
    }

    void Swapchain::recreate(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            throw std::invalid_argument("Cannot create a zero-sized swapchain");
        }

        const auto capabilities = _context.physicalDevice.getSurfaceCapabilitiesKHR(*_context.surface);
        const auto formats = _context.physicalDevice.getSurfaceFormatsKHR(*_context.surface);
        const auto presentModes =  _context.physicalDevice.getSurfacePresentModesKHR(*_context.surface);

        vk::SurfaceFormatKHR surfaceFormat = formats.front();
        for (const auto& candidate : formats) {
            if (candidate.format == vk::Format::eB8G8R8A8Srgb &&
                candidate.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                surfaceFormat = candidate;
                break;
            }
        }

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        for (const auto candidate : presentModes) {
            if (candidate == vk::PresentModeKHR::eMailbox) {
                presentMode = candidate;
                break;
            }
        }

        vk::Extent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<uint32_t>::max()) {
            extent.width = std::clamp(
                width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );
            extent.height = std::clamp(
                height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            );
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        const vk::SwapchainKHR oldSwapchain = _swapchain != nullptr ? *_swapchain : VK_NULL_HANDLE;
        vk::SwapchainCreateInfoKHR createInfo{
            .surface = *_context.surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = vk::True,
            .oldSwapchain = oldSwapchain
        };

        vk::raii::SwapchainKHR newSwapchain(_context.device, createInfo);
        const std::vector<vk::Image> swapchainImages = newSwapchain.getImages();

        std::vector<SwapchainImage> newImages;
        newImages.reserve(swapchainImages.size());
        for (const vk::Image image : swapchainImages) {
            vk::ImageViewCreateInfo viewInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = surfaceFormat.format,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            newImages.push_back(SwapchainImage{
                .image = image,
                .view = vk::raii::ImageView(_context.device, viewInfo),
                .renderFinished = vk::raii::Semaphore(
                    _context.device,
                    vk::SemaphoreCreateInfo{}
                )
            });
        }

        _images = std::move(newImages);
        _swapchain = std::move(newSwapchain);
        _format = surfaceFormat.format;
        _extent = extent;
    }
}
