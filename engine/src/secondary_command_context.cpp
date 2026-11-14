#include "sunshine/secondary_command_context.hpp"
#include "internal_types.hpp"
#include "sunshine/error.hpp"

namespace Sunshine {

SecondaryCommandContext::SecondaryCommandContext(std::unique_ptr<Internal::SecondaryCommandContextImpl> impl)
    : m_impl(std::move(impl)) {}

SecondaryCommandContext::~SecondaryCommandContext() = default;
SecondaryCommandContext::SecondaryCommandContext(SecondaryCommandContext&&) noexcept = default;
SecondaryCommandContext& SecondaryCommandContext::operator=(SecondaryCommandContext&&) noexcept = default;

void SecondaryCommandContext::SetPipeline(const Pipeline& pipeline) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    auto rhiPipe = pipeline.GetRHIPipeline();
    if (rhiPipe) {
        m_impl->rhiCommandList.SetPipeline(*rhiPipe);
    }
}

void SecondaryCommandContext::SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    auto rhiBuf = vertexBuffer.GetRHIBuffer();
    if (rhiBuf) {
        m_impl->rhiCommandList.SetVertexBuffer(binding, *rhiBuf, offset);
    }
}

void SecondaryCommandContext::SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    auto rhiBuf = indexBuffer.GetRHIBuffer();
    if (rhiBuf) {
        m_impl->rhiCommandList.SetIndexBuffer(*rhiBuf, offset, indexBuffer.GetIndexType());
    }
}

void SecondaryCommandContext::SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    if (buffer) {
        m_impl->rhiCommandList.SetUniformBuffer(binding, *buffer, size, offset);
    }
}

void SecondaryCommandContext::SetStorageBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    if (buffer) {
        m_impl->rhiCommandList.SetStorageBuffer(binding, *buffer, size, offset);
    }
}

void SecondaryCommandContext::SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    auto rhiTex = texture.GetRHITexture();
    auto rhiSamp = sampler.GetRHISampler();
    if (rhiTex && rhiSamp) {
        m_impl->rhiCommandList.SetTexture(binding, *rhiTex, *rhiSamp);
    }
}

void SecondaryCommandContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    m_impl->rhiCommandList.SetViewport(x, y, width, height, minDepth, maxDepth);
}

void SecondaryCommandContext::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    m_impl->rhiCommandList.SetScissor(x, y, width, height);
}

void SecondaryCommandContext::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    m_impl->rhiCommandList.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void SecondaryCommandContext::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (!m_impl) throw GraphicsException("Invalid SecondaryCommandContext!");
    m_impl->rhiCommandList.DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

} // namespace Sunshine

