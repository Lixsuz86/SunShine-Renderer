#pragma once

#include "rhi_types.hpp"
#include "rhi_shader.hpp"
#include <memory>
#include <vector>
#include <string>

namespace Sunshine::RHI {

struct ComputePipelineDesc {
    std::shared_ptr<IRHIShader> computeShader;
    std::vector<DescriptorBindingDesc> descriptorBindings;
    std::string debugName;
};

class IRHIComputePipeline {
public:
    virtual ~IRHIComputePipeline() = default;

    virtual const ComputePipelineDesc& GetDesc() const noexcept = 0;
};

} // namespace Sunshine::RHI

