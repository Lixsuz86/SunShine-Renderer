#pragma once

#include "rhi_types.hpp"
#include "rhi_buffer.hpp"
#include "rhi_pipeline.hpp"
#include "rhi_texture.hpp"
#include "rhi_sampler.hpp"

namespace Sunshine::RHI {

class IRHICommandList {
public:
    virtual ~IRHICommandList() = default;

    virtual void SetPipeline(IRHIPipeline& pipeline) = 0;
    virtual void SetVertexBuffer(uint32_t binding, IRHIBuffer& buffer, uint64_t offset = 0) = 0;
    virtual void SetIndexBuffer(IRHIBuffer& buffer, uint64_t offset, IndexType indexType) = 0;
    virtual void SetUniformBuffer(uint32_t binding, IRHIBuffer& buffer, size_t size, size_t offset = 0) = 0;
    virtual void SetTexture(uint32_t binding, IRHITexture& texture, IRHISampler& sampler) = 0;

    virtual void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;
};

} // namespace Sunshine::RHI
