#pragma once

#include "types.hpp"
#include "texture.hpp"
#include <memory>

namespace Sunshine {

class Device;

namespace Internal {
    struct ShadowMapImpl;
}

/**
 * @brief Manages offscreen depth resources for directional/spot shadow mapping.
 * 
 * Encapsulates a high-resolution 2D depth texture (e.g. 2048x2048 D32_SFLOAT),
 * dedicated depth-only VkRenderPass with automatic layout transitions for shader sampling,
 * framebuffer, and border-clamped shadow sampler.
 */
class ShadowMap {
public:
    /**
     * @brief Allocates an offscreen shadow depth map.
     * @param device Parent graphics Device.
     * @param width Resolution width in pixels (typically 2048).
     * @param height Resolution height in pixels (typically 2048).
     */
    ShadowMap(Device& device, uint32_t width = 2048, uint32_t height = 2048);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) noexcept;
    ShadowMap& operator=(ShadowMap&&) noexcept;

    uint32_t GetWidth() const noexcept;
    uint32_t GetHeight() const noexcept;

    /**
     * @brief Returns the depth Texture for binding into scene shaders.
     */
    Texture GetTexture() const;

    /**
     * @brief Returns a linear sampler with clamp-to-border (border color white) for shadow sampling.
     */
    Sampler GetSampler() const;

private:
    friend class Device;
    friend class CommandContext;
    Internal::ShadowMapImpl* GetImpl() const noexcept { return m_impl.get(); }

    std::unique_ptr<Internal::ShadowMapImpl> m_impl;
};

} // namespace Sunshine
