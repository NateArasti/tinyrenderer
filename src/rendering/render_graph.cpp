#include "render_graph.h"

#include <fmt/base.h>
#include <stdexcept>

namespace tr::Rendering {
    RGHandle RenderGraph::addResource(std::string name, const TextureDesc& desc) {
        RGHandle handle;
        handle.index = _resources.size();
        auto textureDesc = desc;
        textureDesc.name = name;
        _resources.push_back(
            RenderGraph::Resource{
                .handle = handle,
                .name = name,
                .desc = std::move(textureDesc),
            }
        );
        return handle;
    }

    RGHandle RenderGraph::importTexture(
        std::string name,
        Handle<GPUTexture> physicalHandle,
        TextureState state
    ) {
        RGHandle handle;
        handle.index = _resources.size();
        _resources.push_back({
            .handle = handle,
            .name = std::move(name),
            .state = state,
            .physicalHandle = physicalHandle,
        });
        return handle;
    }

    void RenderGraph::setFinalState(RGHandle handle, TextureState state) {
        _resources.at(handle.index).finalState = state;
    }

    void RenderGraph::addPass(RenderPass& pass) {
        RenderGraphBuilder builder(*this);
        pass.setup(builder);
        size_t passIndex = _passes.size();
        _passes.push_back({ .passSource = pass });
        for (auto& dependency : builder._dependencies) {
            _passes[passIndex].dependencies.push_back(dependency);
        }
    }

    void RenderGraph::build() {
        std::vector<bool> needed(_resources.size());
        for (int32_t i = 0; i < _resources.size(); ++i) {
            needed[i] = !_resources[i].desc.has_value();
        }

        for (int32_t i = static_cast<int32_t>(_passes.size()) - 1; i >= 0; --i) {
            auto& pass = _passes[i];

            for (const auto& dependency : pass.dependencies) {
                if (dependency.accessType != AccessType::Read && needed[dependency.handle]) {
                    pass.included = true;
                    break;
                }
            }

            if (!pass.included) continue;

            for (const auto& dependency : pass.dependencies) {
                if (dependency.accessType != AccessType::Write) needed[dependency.handle] = true;
            }
        }

        for (int32_t i = 0; i < _passes.size(); ++i) {
            if (!_passes[i].included) continue;
            
            for (const auto& dependency : _passes[i].dependencies) {
                auto& resource = _resources[dependency.handle];
                if (resource.firstUse < 0) resource.firstUse = i;
                resource.lastUse = i;
            }
        }
    }

    void RenderGraph::execute(FrameContext& frame) {
        RenderGraphContext context(*this);

        for (int32_t i = 0; i < _passes.size(); ++i) {
            auto& pass = _passes[i];
            if (!pass.included) {
                fmt::println("[RenderGraph]: Culled {}", pass.passSource.getName());
                continue;
            }

            frame.cmd.beginDebugLabel(pass.passSource.getName());

            for (const auto& dependency : pass.dependencies) {
                auto& resource = _resources[dependency.handle];
                if (!resource.physicalHandle.isValid()) {
                    resource.physicalHandle = _deviceResources.createTexture(*resource.desc);
                }
                const bool previousWrite = resource.lastAccess && *resource.lastAccess != AccessType::Read;
                const bool currentWrite = dependency.accessType != AccessType::Read;
                const bool accessHazard = resource.lastAccess && (previousWrite || currentWrite);
                if (resource.state != dependency.state || accessHazard) {
                    frame.cmd.transition(resource.physicalHandle, resource.state, dependency.state);
                    resource.state = dependency.state;
                }
            }

            pass.passSource.execute(frame.cmd, context, frame);

            for (const auto& dependency : pass.dependencies) {
                auto& resource = _resources[dependency.handle];
                resource.lastAccess = dependency.accessType;
                if (resource.desc && resource.physicalHandle.isValid() && resource.lastUse == i) {
                    _deviceResources.destroyTexture(resource.physicalHandle);
                    resource.physicalHandle = {};
                }
            }

            frame.cmd.endDebugLabel();
        }

        for (auto& resource : _resources) {
            if (resource.finalState && resource.physicalHandle.isValid() && resource.state != *resource.finalState) {
                frame.cmd.transition(resource.physicalHandle, resource.state, *resource.finalState);
                resource.state = *resource.finalState;
            }
        }
    }

    RGHandle RenderGraphBuilder::create(std::string name, const TextureDesc& desc) {
        return _graph.addResource(name, desc);
    }

    void RenderGraphBuilder::read(RGHandle handle, TextureState state) {
        access(handle, state, RenderGraph::AccessType::Read);
    }

    void RenderGraphBuilder::write(RGHandle handle, TextureState state) {
        access(handle, state, RenderGraph::AccessType::Write);
    }

    void RenderGraphBuilder::access(
        RGHandle handle,
        TextureState state,
        RenderGraph::AccessType accessType
    ) {
        if (!handle.isValid()) {
            return;
        }

        for (auto& dependency : _dependencies) {
            if (dependency.handle != handle) {
                continue;
            }
            if (dependency.state != state) {
                throw std::invalid_argument("A render pass cannot use one texture in multiple states");
            }
            if (dependency.accessType != accessType) {
                dependency.accessType = RenderGraph::AccessType::ReadWrite;
            }
            return;
        }

        _dependencies.push_back(RenderGraph::Dependency{
            .handle = handle,
            .state = state,
            .accessType = accessType,
        });
    }

    Handle<GPUTexture> RenderGraphContext::texture(RGHandle handle) {
        if (!handle.isValid() || _graph._resources.size() <= handle) {
            return {};
        }
        return _graph._resources[handle].physicalHandle;
    }
}
