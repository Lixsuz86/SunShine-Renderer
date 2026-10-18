#pragma once

#include "types.hpp"
#include "buffer.hpp"
#include "pipeline.hpp"
#include "texture.hpp"

namespace Sunshine {

class Device;
class FrameContext;

namespace Internal {
    struct CommandContextImpl;
}

class CommandContext {
public:
    ~CommandContext();
    CommandContext(const CommandContext&) = delete;
    CommandContext& operator=(const CommandContext&) = delete;
    CommandContext(CommandContext&&) noexcept;
    CommandContext& operator=(CommandContext&&) noexcept;

    void Clear(const Color& color);

    void SetPipeline(const Pipeline& pipeline);
    void SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset = 0);
    void SetVertexBuffer(const VertexBuffer& vertexBuffer) { SetVertexBuffer(0, vertexBuffer, 0); }
    void SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset = 0);
    
    void SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset = 0);
    
    template<typename T>
    void SetUniformBuffer(uint32_t binding, const UniformBuffer<T>& ubo) {
        SetUniformBufferRaw(binding, ubo.GetRHIBuffer(), sizeof(T), 0);
    }

    void SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler);

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

    // Convenience draw helpers
    void Draw(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, uint32_t vertexCount);
    void DrawIndexed(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, const IndexBuffer& indexBuffer, uint32_t indexCount);

private:
    friend class Device;
    friend class FrameContext;
    explicit CommandContext(Internal::CommandContextImpl* impl);

    Internal::CommandContextImpl* m_impl{nullptr};
};

} // namespace Sunshine
