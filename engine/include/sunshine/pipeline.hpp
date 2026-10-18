#pragma once

#include "rhi/rhi_pipeline.hpp"
#include <memory>

namespace Sunshine {

class Device;

class Pipeline {
public:
    Pipeline() = default;
    explicit Pipeline(std::shared_ptr<RHI::IRHIPipeline> rhiPipeline)
        : m_pipeline(std::move(rhiPipeline)) {}

    std::shared_ptr<RHI::IRHIPipeline> GetRHIPipeline() const noexcept {
        return m_pipeline;
    }

    void ReplaceRHIPipeline(std::shared_ptr<RHI::IRHIPipeline> newPipeline) noexcept {
        m_pipeline = std::move(newPipeline);
    }

    const RHI::PipelineDesc* GetDesc() const noexcept {
        return m_pipeline ? &m_pipeline->GetDesc() : nullptr;
    }

private:
    std::shared_ptr<RHI::IRHIPipeline> m_pipeline;
};

} // namespace Sunshine
