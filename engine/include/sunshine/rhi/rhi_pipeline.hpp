#pragma once

#include "rhi_types.hpp"
#include "rhi_shader.hpp"
#include <memory>
#include <vector>

namespace Sunshine::RHI {

struct PipelineDesc {
    std::shared_ptr<IRHIShader> vertexShader;
    std::shared_ptr<IRHIShader> fragmentShader;
    VertexLayout vertexLayout;
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    RasterizerDesc rasterizer{};
    BlendDesc blend{};
    DepthStencilDesc depthStencil{};
    std::vector<DescriptorBindingDesc> descriptorBindings;
    uint32_t colorAttachmentCount{1};
    std::string debugName;
};

class IRHIPipeline {
public:
    virtual ~IRHIPipeline() = default;

    virtual const PipelineDesc& GetDesc() const noexcept = 0;
};

} // namespace Sunshine::RHI
