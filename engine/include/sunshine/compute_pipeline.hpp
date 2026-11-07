#pragma once

#include "rhi/rhi_compute_pipeline.hpp"
#include <memory>

namespace Sunshine {

class Device;

class ComputePipeline {
public:
    ComputePipeline() = default;
    explicit ComputePipeline(std::shared_ptr<RHI::IRHIComputePipeline> rhiPipeline)
        : m_pipeline(std::move(rhiPipeline)) {}

    std::shared_ptr<RHI::IRHIComputePipeline> GetRHIComputePipeline() const noexcept {
        return m_pipeline;
    }

    void ReplaceRHIComputePipeline(std::shared_ptr<RHI::IRHIComputePipeline> newPipeline) noexcept {
        m_pipeline = std::move(newPipeline);
    }

    const RHI::ComputePipelineDesc* GetDesc() const noexcept {
        return m_pipeline ? &m_pipeline->GetDesc() : nullptr;
    }

private:
    std::shared_ptr<RHI::IRHIComputePipeline> m_pipeline;
};

} // namespace Sunshine

