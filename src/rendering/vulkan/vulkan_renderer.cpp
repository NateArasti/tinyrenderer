#include "vulkan_renderer.h"

namespace tr::Rendering::Vulkan {
    VulkanRenderer::VulkanRenderer() {
    }

    VulkanRenderer::~VulkanRenderer() {
    }

    void VulkanRenderer::init() {
    }

    void VulkanRenderer::shutdown() {
    }

    void VulkanRenderer::clearResources() {
    }

    void VulkanRenderer::createShader(
        tr::Resources::Handle<tr::Data::Shader> handle,
        const tr::Data::Shader& shader) {
    }
    
    void VulkanRenderer::createTexture(
        tr::Resources::Handle<tr::Data::Texture> handle,
        const tr::Data::Texture& texture) {
    }
    
    void VulkanRenderer::createMaterial(
        tr::Resources::Handle<tr::Data::Material> handle,
        const tr::Data::Material& material) {
    }
    
    void VulkanRenderer::createMesh(
        tr::Resources::Handle<tr::Data::Mesh> handle,
        const tr::Data::Mesh& mesh) {
    }

    void VulkanRenderer::startFrame(const tr::Data::Camera& camera) {
    }

    void VulkanRenderer::draw(const DrawCommand& command) {
    }
    
    void VulkanRenderer::endFrame() {
    }
}
