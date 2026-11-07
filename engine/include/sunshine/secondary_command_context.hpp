#pragma once

#include "types.hpp"
#include "buffer.hpp"
#include "pipeline.hpp"
#include "texture.hpp"
#include <memory>

namespace Sunshine {

namespace Internal {
    struct SecondaryCommandContextImpl;
    struct CommandContextImpl;
}

class SecondaryCommandContext {
public:
    ~SecondaryCommandContext();
    SecondaryCommandContext(const SecondaryCommandContext&) = delete;
    SecondaryCommandContext& operator=(const SecondaryCommandContext&) = delete;
    SecondaryCommandContext(SecondaryCommandContext&&) noexcept;
    SecondaryCommandContext& operator=(SecondaryCommandContext&&) noexcept;

    void SetPipeline(const Pipeline& pipeline);
    void SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset = 0);
    void SetVertexBuffer(const VertexBuffer& vertexBuffer) { SetVertexBuffer(0, vertexBuffer, 0); }
    void SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset = 0);

    void SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset = 0);
    template<typename T>
    void SetUniformBuffer(uint32_t binding, const UniformBuffer<T>& ubo) {
        SetUniformBufferRaw(binding, ubo.GetRHIBuffer(), sizeof(T), 0);
    }

    void SetStorageBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset = 0);
    template<typename T>
    void SetStorageBuffer(uint32_t binding, const StorageBuffer<T>& sbo) {
        SetStorageBufferRaw(binding, sbo.GetRHIBuffer(), sbo.GetSize(), 0);
    }

    void SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler);

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

private:
    friend class CommandContext;
    friend struct Internal::CommandContextImpl;
    explicit SecondaryCommandContext(std::unique_ptr<Internal::SecondaryCommandContextImpl> impl);

    std::unique_ptr<Internal::SecondaryCommandContextImpl> m_impl;
};

} // namespace Sunshine
