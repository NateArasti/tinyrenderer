#include "ui_pass.h"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <imgui_impl_vulkan.h>

namespace tr::Rendering::Vulkan {

    UIPass::UIPass(VulkanContext& context, Swapchain& swapchain) : _vulkanContext(context) {
        initialize(swapchain);
    }

    UIPass::~UIPass() {
        shutdown();
    }

    void UIPass::initialize(const Swapchain& swapchain) {
        VkFormat colorFormat = static_cast<VkFormat>(swapchain.format());

        VkPipelineRenderingCreateInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat
        };

        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_3;
        info.Instance = static_cast<VkInstance>(*_vulkanContext.instance);
        info.PhysicalDevice = static_cast<VkPhysicalDevice>(*_vulkanContext.physicalDevice);
        info.Device = static_cast<VkDevice>(*_vulkanContext.device);
        info.QueueFamily = _vulkanContext.queueIndex;
        info.Queue = static_cast<VkQueue>(*_vulkanContext.queue);
        info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
        info.MinImageCount = 2;
        info.ImageCount = static_cast<uint32_t>(swapchain.imageCount());
        info.UseDynamicRendering = true;
        info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

        ImGui_ImplVulkan_Init(&info);

        _initialized = true;
    }

    void UIPass::shutdown() {
        if (_initialized) {
            ImGui_ImplVulkan_Shutdown();
        }
    }

    void UIPass::recreate(const Swapchain& swapchain) {
        shutdown();
        initialize(swapchain);
    }

    void UIPass::beginFrame() {
        ImGui_ImplVulkan_NewFrame();
    }
    
    void UIPass::draw(
        const vk::raii::CommandBuffer& commandBuffer,
        vk::ImageView target,
        vk::Extent2D extent,
        ImDrawData* drawData
    ) {
        vk::RenderingAttachmentInfo colorAttachment{
            .imageView = target,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore
        };
        vk::RenderingInfo renderingInfo{
            .renderArea = { { 0, 0 }, extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        commandBuffer.beginRendering(renderingInfo);
        ImGui_ImplVulkan_RenderDrawData(
            drawData,
            static_cast<VkCommandBuffer>(*commandBuffer)
        );
        commandBuffer.endRendering();
    }
}
