#pragma once

#include "rhi.h"

namespace tr::Rendering::Vulkan {
    class VulkanRenderer : public RHI {
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        void init() override;
        void shutdown() override;
        void clearResources() override;

        void createShader(
            tr::Resources::Handle<tr::Data::Shader> handle,
            const tr::Data::Shader& shader) override;
        void createTexture(
            tr::Resources::Handle<tr::Data::Texture> handle,
            const tr::Data::Texture& texture) override;
        void createMaterial(
            tr::Resources::Handle<tr::Data::Material> handle,
            const tr::Data::Material& material) override;
        void createMesh(
            tr::Resources::Handle<tr::Data::Mesh> handle,
            const tr::Data::Mesh& mesh) override;

        void startFrame(const tr::Data::Camera& camera) override;
        void draw(const DrawCommand& command) override;
        void endFrame() override;
    };
}
