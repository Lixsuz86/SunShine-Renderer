#pragma once

#include "rhi/rhi_texture.hpp"
#include "rhi/rhi_sampler.hpp"
#include <memory>
#include <string>

namespace Sunshine {

class Texture {
public:
    Texture() = default;
    explicit Texture(std::shared_ptr<RHI::IRHITexture> rhiTexture)
        : m_texture(std::move(rhiTexture)) {}

    uint32_t GetWidth() const noexcept { return m_texture ? m_texture->GetWidth() : 0; }
    uint32_t GetHeight() const noexcept { return m_texture ? m_texture->GetHeight() : 0; }
    RHI::TextureFormat GetFormat() const noexcept { return m_texture ? m_texture->GetFormat() : RHI::TextureFormat::RGBA8_UNORM; }
    std::shared_ptr<RHI::IRHITexture> GetRHITexture() const noexcept { return m_texture; }
    explicit operator bool() const noexcept { return m_texture != nullptr; }

private:
    std::shared_ptr<RHI::IRHITexture> m_texture;
};

class Sampler {
public:
    Sampler() = default;
    explicit Sampler(std::shared_ptr<RHI::IRHISampler> rhiSampler)
        : m_sampler(std::move(rhiSampler)) {}

    std::shared_ptr<RHI::IRHISampler> GetRHISampler() const noexcept { return m_sampler; }
    explicit operator bool() const noexcept { return m_sampler != nullptr; }

private:
    std::shared_ptr<RHI::IRHISampler> m_sampler;
};

} // namespace Sunshine
