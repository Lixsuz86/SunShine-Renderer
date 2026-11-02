#pragma once

#include "types.hpp"
#include "texture.hpp"
#include <memory>
#include <string>

namespace Sunshine {

class Device;

namespace Internal {
    struct RenderTargetImpl;
}

/**
 * @brief Configuration descriptor for an offscreen RenderTarget.
 */
struct RenderTargetDesc {
    uint32_t width{1280};
    uint32_t height{720};
    RHI::TextureFormat colorFormat{RHI::TextureFormat::RGBA16_FLOAT};
    bool hasDepth{false};
    RHI::TextureFormat depthFormat{RHI::TextureFormat::D32_FLOAT};
    RHI::FilterMode filter{RHI::FilterMode::Linear};
    RHI::AddressMode addressMode{RHI::AddressMode::ClampToEdge};
    std::string debugName;
};

/**
 * @brief Represents an offscreen HDR / LDR render target with color and optional depth attachments.
 * 
 * Encapsulates color and depth GPU textures, a dedicated compatible VkRenderPass with automatic
 * layout transitions (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL on pass completion),
 * a VkFramebuffer, and a sampling filter.
 */
class RenderTarget {
public:
    RenderTarget(Device& device, const RenderTargetDesc& desc);
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&&) noexcept;
    RenderTarget& operator=(RenderTarget&&) noexcept;

    uint32_t GetWidth() const noexcept;
    uint32_t GetHeight() const noexcept;
    RHI::TextureFormat GetColorFormat() const noexcept;
    bool HasDepth() const noexcept;

    /**
     * @brief Returns the rendered color texture for subsequent shader sampling.
     */
    Texture GetColorTexture() const;

    /**
     * @brief Returns the depth texture if hasDepth was true, or an empty texture.
     */
    Texture GetDepthTexture() const;

    /**
     * @brief Returns the sampler configured for this render target.
     */
    Sampler GetSampler() const;

private:
    friend class Device;
    friend class CommandContext;
    friend class RenderGraph;
    Internal::RenderTargetImpl* GetImpl() const noexcept { return m_impl.get(); }

    std::unique_ptr<Internal::RenderTargetImpl> m_impl;
};

} // namespace Sunshine
