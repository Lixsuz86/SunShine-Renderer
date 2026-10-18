#pragma once

#include "rhi_types.hpp"

namespace Sunshine::RHI {

class IRHISampler {
public:
    virtual ~IRHISampler() = default;

    virtual const SamplerDesc& GetDesc() const noexcept = 0;
};

} // namespace Sunshine::RHI
