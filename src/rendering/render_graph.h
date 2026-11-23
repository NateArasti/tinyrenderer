#pragma once

#include "frame_context.h"
#include "rendering_resources.h"
#include "handle.h"
#include "rendering_structs.h"
#include "command_buffer.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>

using namespace tr::Data;

namespace tr::Rendering {
    struct RGResource {};
    struct RGHandle : public Handle<RGResource> {};

    class RenderGraph;
    class RenderGraphBuilder;
    class RenderGraphContext;
    class RenderPass;

    class RenderGraph {
    private:
        friend class RenderGraphBuilder;
        friend class RenderGraphContext;

        enum class AccessType : uint8_t {
            Read = 1,
            Write = 2,
            ReadWrite = 3,
        };

        struct Dependency {
            RGHandle handle;
            TextureState state;
            AccessType accessType;
        };

        struct Resource {
            RGHandle handle;
            std::string name;
            std::optional<TextureDesc> desc;

            TextureState state = TextureState::Undefined;
            Handle<GPUTexture> physicalHandle;
            std::optional<AccessType> lastAccess;
            std::optional<TextureState> finalState;
            int32_t firstUse = -1;
            int32_t lastUse = -1;
        };

        struct PassWrapper {
            bool included = false;
            RenderPass&  passSource;
            std::vector<Dependency> dependencies;
        };

        RenderingResources& _deviceResources;
        std::vector<PassWrapper> _passes;
        std::vector<Resource> _resources;

        RGHandle addResource(std::string name, const TextureDesc& desc);

    public:
        explicit RenderGraph(RenderingResources& resources) : _deviceResources(resources) {}
        
        RGHandle importTexture(
            std::string name,
            Handle<GPUTexture> physicalHandle,
            TextureState state
        );
        void setFinalState(RGHandle handle, TextureState state);

        void addPass(RenderPass& pass);
        void build();
        void execute(FrameContext& frame);
    };

    class RenderGraphBuilder {
    private:
        friend class RenderGraph;

        RenderGraph& _graph;
        std::vector<RenderGraph::Dependency> _dependencies;

        void access(RGHandle handle, TextureState state, RenderGraph::AccessType accessType);
        
    public:
        explicit RenderGraphBuilder(RenderGraph& graph) : _graph(graph) { }
        
        [[nodiscard]] RGHandle create(std::string name, const TextureDesc& desc);
        void read(RGHandle handle, TextureState state);
        void write(RGHandle handle, TextureState state);
    };

    class RenderGraphContext {
    private:
        RenderGraph& _graph;

    public:
        RenderGraphContext(RenderGraph& graph) : _graph(graph) { }
        
        Handle<GPUTexture> texture(RGHandle handle);
    };

    class RenderPass {
    private:
        const std::string _name;

    protected:
        RenderingResources& _resources;

    public:
        explicit RenderPass(const std::string& name, RenderingResources& resources)
            : _name(name), _resources(resources) {}
        virtual ~RenderPass() = default;

        const std::string& getName() const { return _name; }

        virtual void setup(RenderGraphBuilder& builder) = 0;
        virtual void execute(CommandBuffer& cmd, RenderGraphContext& context, FrameContext& frame) = 0;
    };
}
