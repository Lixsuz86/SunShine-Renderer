#pragma once

#include "rhi_types.hpp"

namespace Sunshine::RHI {

class IRHIShader {
public:
    virtual ~IRHIShader() = default;

    virtual ShaderStage GetStage() const noexcept = 0;
    virtual const std::string& GetEntryPoint() const noexcept = 0;
};

} // namespace Sunshine::RHI
