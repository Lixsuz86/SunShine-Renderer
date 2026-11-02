#include "sunshine/render_graph.hpp"
#include "sunshine/device.hpp"
#include "sunshine/error.hpp"
#include <queue>
#include <unordered_set>
#include <iostream>

namespace Sunshine {

struct RGResourceEntry {
    RGTextureID id{RG_INVALID_ID};
    RGTextureDesc desc;
    bool isImported{false};
    Texture importedTexture;
    RenderTarget* importedTarget{nullptr};
    std::shared_ptr<RenderTarget> pooledTarget;
};

struct RGPassData {
    uint32_t index{0};
    std::string name;
    std::vector<RGTextureID> reads;
    RGTextureID colorTarget{RG_INVALID_ID};
    RGTextureID depthTarget{RG_INVALID_ID};
    bool targetsSwapchain{false};
    Color clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    RGPassExecuteFunc executeFunc;
};

void RenderPassBuilder::Read(RGTextureID textureID) {
    m_data.reads.push_back(textureID);
}

void RenderPassBuilder::SetColorTarget(RGTextureID textureID, const Color& clearColor) {
    m_data.colorTarget = textureID;
    m_data.clearColor = clearColor;
}

void RenderPassBuilder::SetDepthTarget(RGTextureID textureID) {
    m_data.depthTarget = textureID;
}

void RenderPassBuilder::SetSwapchainTarget(const Color& clearColor) {
    m_data.targetsSwapchain = true;
    m_data.clearColor = clearColor;
}

namespace Internal {

struct RenderGraphImpl {
    Device& device;
    std::vector<RGResourceEntry> resources;
    std::vector<RGPassData> passes;
    std::vector<uint32_t> executionOrder;

    // Physical RenderTarget pool for memory reuse / aliasing across frames
    std::vector<std::shared_ptr<RenderTarget>> targetPool;

    explicit RenderGraphImpl(Device& dev) : device(dev) {}

    RGTextureID CreateTexture(const RGTextureDesc& desc) {
        RGTextureID id = static_cast<RGTextureID>(resources.size());
        RGResourceEntry entry{};
        entry.id = id;
        entry.desc = desc;
        entry.isImported = false;
        resources.push_back(std::move(entry));
        return id;
    }

    RGTextureID ImportTexture(const std::string& name, Texture texture) {
        RGTextureID id = static_cast<RGTextureID>(resources.size());
        RGResourceEntry entry{};
        entry.id = id;
        entry.desc.name = name;
        entry.desc.width = texture.GetWidth();
        entry.desc.height = texture.GetHeight();
        entry.desc.format = texture.GetFormat();
        entry.isImported = true;
        entry.importedTexture = texture;
        resources.push_back(std::move(entry));
        return id;
    }

    RGTextureID ImportRenderTarget(const std::string& name, RenderTarget& target) {
        RGTextureID id = static_cast<RGTextureID>(resources.size());
        RGResourceEntry entry{};
        entry.id = id;
        entry.desc.name = name;
        entry.desc.width = target.GetWidth();
        entry.desc.height = target.GetHeight();
        entry.desc.format = target.GetColorFormat();
        entry.desc.hasDepth = target.HasDepth();
        entry.isImported = true;
        entry.importedTarget = &target;
        entry.importedTexture = target.GetColorTexture();
        resources.push_back(std::move(entry));
        return id;
    }

    void AddPass(const std::string& name,
                 std::function<void(RenderPassBuilder& builder)> setup,
                 RGPassExecuteFunc execute) 
    {
        uint32_t passIdx = static_cast<uint32_t>(passes.size());
        RGPassData passData{};
        passData.index = passIdx;
        passData.name = name;
        passData.executeFunc = std::move(execute);

        RenderPassBuilder builder(passIdx, passData);
        if (setup) {
            setup(builder);
        }

        passes.push_back(std::move(passData));
    }

    void Compile() {
        uint32_t numPasses = static_cast<uint32_t>(passes.size());
        if (numPasses == 0) {
            executionOrder.clear();
            return;
        }

        // 1. Dependency Analysis: Map each resource to its producer pass
        std::unordered_map<RGTextureID, uint32_t> resourceProducer;
        for (uint32_t i = 0; i < numPasses; ++i) {
            const auto& p = passes[i];
            if (p.colorTarget != RG_INVALID_ID) {
                resourceProducer[p.colorTarget] = i;
            }
            if (p.depthTarget != RG_INVALID_ID) {
                resourceProducer[p.depthTarget] = i;
            }
        }

        // Build adjacency list for DAG topological sort
        std::vector<std::vector<uint32_t>> adj(numPasses);
        std::vector<uint32_t> inDegree(numPasses, 0);

        for (uint32_t i = 0; i < numPasses; ++i) {
            const auto& p = passes[i];
            for (RGTextureID readRes : p.reads) {
                auto it = resourceProducer.find(readRes);
                if (it != resourceProducer.end()) {
                    uint32_t producerIdx = it->second;
                    if (producerIdx != i) {
                        adj[producerIdx].push_back(i);
                        inDegree[i]++;
                    }
                }
            }
        }

        // Topological Sort (Kahn's Algorithm)
        std::queue<uint32_t> zeroQueue;
        for (uint32_t i = 0; i < numPasses; ++i) {
            if (inDegree[i] == 0) {
                zeroQueue.push(i);
            }
        }

        executionOrder.clear();
        executionOrder.reserve(numPasses);

        while (!zeroQueue.empty()) {
            uint32_t u = zeroQueue.front();
            zeroQueue.pop();
            executionOrder.push_back(u);

            for (uint32_t v : adj[u]) {
                if (--inDegree[v] == 0) {
                    zeroQueue.push(v);
                }
            }
        }

        // If cycle detected, fallback to declaration order
        if (executionOrder.size() != numPasses) {
            std::cerr << "[RenderGraph Warning] Cycle or unresolvable dependency in graph passes! Falling back to declaration order." << std::endl;
            executionOrder.resize(numPasses);
            for (uint32_t i = 0; i < numPasses; ++i) {
                executionOrder[i] = i;
            }
        }

        // 2. Physical Resource Allocation & Transient Memory Pooling
        std::vector<bool> targetUsed(targetPool.size(), false);

        for (auto& res : resources) {
            if (res.isImported) continue;

            // Try to find a free matching target from the pool
            std::shared_ptr<RenderTarget> matchedTarget = nullptr;
            for (size_t i = 0; i < targetPool.size(); ++i) {
                if (!targetUsed[i]) {
                    const auto& target = targetPool[i];
                    if (target->GetWidth() == res.desc.width &&
                        target->GetHeight() == res.desc.height &&
                        target->GetColorFormat() == res.desc.format &&
                        target->HasDepth() == res.desc.hasDepth) 
                    {
                        matchedTarget = target;
                        targetUsed[i] = true;
                        break;
                    }
                }
            }

            // If not found in pool, allocate new RenderTarget and cache it in the pool
            if (!matchedTarget) {
                RenderTargetDesc desc{};
                desc.width = res.desc.width;
                desc.height = res.desc.height;
                desc.colorFormat = res.desc.format;
                desc.hasDepth = res.desc.hasDepth;
                desc.debugName = res.desc.name.empty() ? "RG_TransientTarget" : ("RG_" + res.desc.name);

                matchedTarget = std::make_shared<RenderTarget>(device, desc);
                targetPool.push_back(matchedTarget);
                targetUsed.push_back(true);
            }

            res.pooledTarget = matchedTarget;
        }
    }

    void Execute(CommandContext& cmd) {
        for (uint32_t passIdx : executionOrder) {
            auto& pass = passes[passIdx];

            if (pass.targetsSwapchain) {
                cmd.Clear(pass.clearColor);
                if (pass.executeFunc) {
                    pass.executeFunc(cmd);
                }
            } else if (pass.colorTarget != RG_INVALID_ID) {
                RenderTarget* target = GetPhysicalRenderTarget(pass.colorTarget);
                if (target) {
                    cmd.BeginPass(*target, pass.clearColor);
                    if (pass.executeFunc) {
                        pass.executeFunc(cmd);
                    }
                    cmd.EndPass();
                }
            } else {
                if (pass.executeFunc) {
                    pass.executeFunc(cmd);
                }
            }
        }
    }

    Texture GetPhysicalTexture(RGTextureID id) const {
        if (id >= resources.size()) return Texture{};
        const auto& res = resources[id];
        if (res.isImported) {
            if (res.importedTarget) {
                return res.importedTarget->GetColorTexture();
            }
            return res.importedTexture;
        }
        if (res.pooledTarget) {
            return res.pooledTarget->GetColorTexture();
        }
        return Texture{};
    }

    RenderTarget* GetPhysicalRenderTarget(RGTextureID id) const {
        if (id >= resources.size()) return nullptr;
        const auto& res = resources[id];
        if (res.isImported) {
            return res.importedTarget;
        }
        return res.pooledTarget.get();
    }

    void Reset() {
        resources.clear();
        passes.clear();
        executionOrder.clear();
    }
};

} // namespace Internal

RenderGraph::RenderGraph(Device& device)
    : m_impl(std::make_unique<Internal::RenderGraphImpl>(device)) {}

RenderGraph::~RenderGraph() = default;

RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

RGTextureID RenderGraph::CreateTexture(const RGTextureDesc& desc) {
    return m_impl->CreateTexture(desc);
}

RGTextureID RenderGraph::ImportTexture(const std::string& name, Texture texture) {
    return m_impl->ImportTexture(name, texture);
}

RGTextureID RenderGraph::ImportRenderTarget(const std::string& name, RenderTarget& target) {
    return m_impl->ImportRenderTarget(name, target);
}

void RenderGraph::AddPass(const std::string& name,
                         std::function<void(RenderPassBuilder& builder)> setup,
                         RGPassExecuteFunc execute) 
{
    m_impl->AddPass(name, std::move(setup), std::move(execute));
}

void RenderGraph::Compile() {
    m_impl->Compile();
}

void RenderGraph::Execute(CommandContext& cmd) {
    m_impl->Execute(cmd);
}

Texture RenderGraph::GetPhysicalTexture(RGTextureID id) const {
    return m_impl->GetPhysicalTexture(id);
}

RenderTarget* RenderGraph::GetPhysicalRenderTarget(RGTextureID id) const {
    return m_impl->GetPhysicalRenderTarget(id);
}

void RenderGraph::Reset() {
    m_impl->Reset();
}

size_t RenderGraph::GetPassCount() const noexcept {
    return m_impl ? m_impl->passes.size() : 0;
}

size_t RenderGraph::GetPooledTargetCount() const noexcept {
    return m_impl ? m_impl->targetPool.size() : 0;
}

} // namespace Sunshine
