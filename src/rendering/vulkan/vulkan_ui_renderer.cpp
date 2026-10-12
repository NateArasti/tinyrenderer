#include "vulkan_ui_renderer.h"

#include <imgui_impl_vulkan.h>

#include "vulkan_command_buffer.h"

namespace tr::Rendering::Vulkan {
    VulkanUIRenderer::VulkanUIRenderer(VulkanContext& context, const Swapchain& swapchain)
        : _context(context), _swapchain(swapchain) {
        initialize();
    }

    VulkanUIRenderer::~VulkanUIRenderer() {
        shutdown();
    }

    void VulkanUIRenderer::initialize() {
        VkFormat colorFormat = static_cast<VkFormat>(_swapchain.format());

        VkPipelineRenderingCreateInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat,
        };

        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_3;
        info.Instance = static_cast<VkInstance>(*_context.instance);
        info.PhysicalDevice = static_cast<VkPhysicalDevice>(*_context.physicalDevice);
        info.Device = static_cast<VkDevice>(*_context.device);
        info.QueueFamily = _context.queueIndex;
        info.Queue = static_cast<VkQueue>(*_context.queue);
        info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
        info.MinImageCount = 2;
        info.ImageCount = static_cast<uint32_t>(_swapchain.imageCount());
        info.UseDynamicRendering = true;
        info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

        ImGui_ImplVulkan_Init(&info);

        _initialized = true;
    }

    void VulkanUIRenderer::shutdown() {
        if (_initialized) {
            ImGui_ImplVulkan_Shutdown();
            _initialized = false;
        }
    }

    void VulkanUIRenderer::recreate() {
        shutdown();
        initialize();
    }

    void VulkanUIRenderer::beginFrame() {
        ImGui_ImplVulkan_NewFrame();
    }

    void VulkanUIRenderer::draw(CommandBuffer& cmd, ImDrawData* drawData) {
        auto& vulkanCmd = static_cast<VulkanCommandBuffer&>(cmd);
        ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(*vulkanCmd.handle()));
    }
}
