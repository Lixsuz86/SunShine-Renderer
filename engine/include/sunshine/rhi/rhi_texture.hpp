#pragma once

#include "rhi_types.hpp"

namespace Sunshine::RHI {

class IRHITexture {
public:
    virtual ~IRHITexture() = default;

    virtual uint32_t GetWidth() const noexcept = 0;
    virtual uint32_t GetHeight() const noexcept = 0;
    virtual TextureFormat GetFormat() const noexcept = 0;
};

} // namespace Sunshine::RHI
