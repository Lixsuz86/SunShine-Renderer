#pragma once

#include "rhi_types.hpp"
#include "rhi_buffer.hpp"
#include "rhi_shader.hpp"
#include "rhi_pipeline.hpp"
#include "rhi_command_list.hpp"
#include "rhi_texture.hpp"
#include "rhi_sampler.hpp"
#include <memory>

namespace Sunshine::RHI {

class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    virtual std::shared_ptr<IRHIBuffer> CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
    virtual std::shared_ptr<IRHIShader> CreateShader(const ShaderDesc& desc) = 0;
    virtual std::shared_ptr<IRHIPipeline> CreatePipeline(const PipelineDesc& desc) = 0;
    virtual std::shared_ptr<IRHITexture> CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;
    virtual std::shared_ptr<IRHISampler> CreateSampler(const SamplerDesc& desc) = 0;
    virtual void WaitIdle() = 0;
};

} // namespace Sunshine::RHI
