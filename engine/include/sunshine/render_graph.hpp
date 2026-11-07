#pragma once

#include "types.hpp"
#include "texture.hpp"
#include "render_target.hpp"
#include "command_context.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Sunshine {

class Device;

using RGTextureID = uint32_t;
constexpr RGTextureID RG_INVALID_ID = 0xFFFFFFFF;

/**
 * @brief Descriptor for a transient texture managed by the RenderGraph.
 */
struct RGTextureDesc {
    uint32_t width{0};
    uint32_t height{0};
    RHI::TextureFormat format{RHI::TextureFormat::RGBA16_FLOAT};
    bool hasDepth{false};
    std::string name;
};

/**
 * @brief Builder passed to pass setup callbacks to declare inputs and outputs.
 */
class RenderPassBuilder {
public:
    explicit RenderPassBuilder(uint32_t passIndex, struct RGPassData& passData)
        : m_passIndex(passIndex), m_data(passData) {}

    /**
     * @brief Declares that this pass reads (samples) from the specified texture.
     */
    void Read(RGTextureID textureID);

    /**
     * @brief Declares that this pass renders into the specified texture as a color attachment.
     */
    void SetColorTarget(RGTextureID textureID, const Color& clearColor = Color{0.0f, 0.0f, 0.0f, 1.0f});

    /**
     * @brief Declares that this pass renders with the depth buffer of the specified texture.
     */
    void SetDepthTarget(RGTextureID textureID);

    /**
     * @brief Declares that this pass outputs directly to the presentation swapchain.
     */
    void SetSwapchainTarget(const Color& clearColor = Color{0.0f, 0.0f, 0.0f, 1.0f});

private:
    uint32_t m_passIndex;
    struct RGPassData& m_data;
};

using RGPassExecuteFunc = std::function<void(CommandContext& cmd)>;

namespace Internal {
    struct RenderGraphImpl;
}

/**
 * @brief High-level Directed Acyclic Graph (DAG) for orchestrating multi-pass rendering.
 * 
 * Automatically resolves pass dependencies, manages offscreen RenderTarget allocation
 * and transient memory pooling, and executes passes in optimal order.
 */
class RenderGraph {
public:
    explicit RenderGraph(Device& device);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;

    /**
     * @brief Declares a transient virtual texture managed by the graph.
     */
    RGTextureID CreateTexture(const RGTextureDesc& desc);

    /**
     * @brief Imports an existing external texture into the graph.
     */
    RGTextureID ImportTexture(const std::string& name, Texture texture);

    /**
     * @brief Imports an existing external RenderTarget into the graph.
     */
    RGTextureID ImportRenderTarget(const std::string& name, RenderTarget& target);

    /**
     * @brief Adds a render pass to the graph with setup and execute callbacks.
     */
    void AddPass(const std::string& name,
                 std::function<void(RenderPassBuilder& builder)> setup,
                 RGPassExecuteFunc execute);

    /**
     * @brief Compiles the graph: validates DAG dependencies, sorts passes, and pools RenderTargets.
     */
    void Compile();

    /**
     * @brief Executes all compiled passes recorded into the given command context.
     */
    void Execute(CommandContext& cmd);

    /**
     * @brief Returns the physical Texture backing the virtual resource ID.
     */
    Texture GetPhysicalTexture(RGTextureID id) const;

    /**
     * @brief Returns the physical RenderTarget backing the virtual resource ID (if it is a render target).
     */
    RenderTarget* GetPhysicalRenderTarget(RGTextureID id) const;

    /**
     * @brief Resets the graph structure for a new frame, keeping the physical target pool cached.
     */
    void Reset();

    /**
     * @brief Returns telemetry information: number of passes executed in last frame.
     */
    size_t GetPassCount() const noexcept;

    /**
     * @brief Returns the number of physical RenderTargets currently pooled.
     */
    size_t GetPooledTargetCount() const noexcept;

private:
    std::unique_ptr<Internal::RenderGraphImpl> m_impl;
};

} // namespace Sunshine

