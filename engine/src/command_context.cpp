#include "sunshine/command_context.hpp"
#include "internal_types.hpp"
#include "sunshine/error.hpp"
#include <array>

namespace Sunshine {

namespace Internal {

void CommandContextImpl::Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, VkExtent2D ext, uint32_t frameIndex) {
    commandBuffer = cmd;
    renderPass = rp;
    framebuffer = fb;
    extent = ext;
    passActive = false;

    rhiCommandList.SetCommandBuffer(cmd);
    rhiCommandList.SetFrameIndex(frameIndex);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    isRecording = true;
}

void CommandContextImpl::Clear(const Color& color) {
    if (!isRecording) {
        throw GraphicsException("Cannot clear frame outside of an active command buffer recording!");
    }

    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.float32[0] = color.r;
    clearValues[0].color.float32[1] = color.g;
    clearValues[0].color.float32[2] = color.b;
    clearValues[0].color.float32[3] = color.a;
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    passActive = true;

    // Automatically set default dynamic viewport and scissor matching the current extent
    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
    rhiCommandList.SetScissor(0, 0, extent.width, extent.height);
}

void CommandContextImpl::EnsurePassActive() {
    if (!passActive) {
        Clear(Color::DarkSlate());
    }
}

void CommandContextImpl::SetPipeline(const Pipeline& pipeline) {
    EnsurePassActive();
    auto rhiPipe = pipeline.GetRHIPipeline();
    if (!rhiPipe) throw GraphicsException("Invalid Pipeline passed to SetPipeline!");
    rhiCommandList.SetPipeline(*rhiPipe);
}

void CommandContextImpl::SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset) {
    EnsurePassActive();
    auto rhiBuf = vertexBuffer.GetRHIBuffer();
    if (!rhiBuf) throw GraphicsException("Invalid VertexBuffer passed to SetVertexBuffer!");
    rhiCommandList.SetVertexBuffer(binding, *rhiBuf, offset);
}

void CommandContextImpl::SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset) {
    EnsurePassActive();
    auto rhiBuf = indexBuffer.GetRHIBuffer();
    if (!rhiBuf) throw GraphicsException("Invalid IndexBuffer passed to SetIndexBuffer!");
    rhiCommandList.SetIndexBuffer(*rhiBuf, offset, indexBuffer.GetIndexType());
}

void CommandContextImpl::SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    EnsurePassActive();
    if (!buffer) throw GraphicsException("Invalid buffer passed to SetUniformBufferRaw!");
    rhiCommandList.SetUniformBuffer(binding, *buffer, size, offset);
}

void CommandContextImpl::SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler) {
    EnsurePassActive();
    auto rhiTex = texture.GetRHITexture();
    auto rhiSampler = sampler.GetRHISampler();
    if (!rhiTex || !rhiSampler) throw GraphicsException("Invalid Texture or Sampler passed to SetTexture!");
    rhiCommandList.SetTexture(binding, *rhiTex, *rhiSampler);
}

void CommandContextImpl::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    EnsurePassActive();
    rhiCommandList.SetViewport(x, y, width, height, minDepth, maxDepth);
}

void CommandContextImpl::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    EnsurePassActive();
    rhiCommandList.SetScissor(x, y, width, height);
}

void CommandContextImpl::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    EnsurePassActive();
    rhiCommandList.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandContextImpl::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    EnsurePassActive();
    rhiCommandList.DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandContextImpl::Draw(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, uint32_t vertexCount) {
    if (!isRecording) {
        throw GraphicsException("Cannot draw outside of an active frame!");
    }

    EnsurePassActive();

    auto rhiPipe = pipeline.GetRHIPipeline();
    auto rhiBuf = vertexBuffer.GetRHIBuffer();
    if (!rhiPipe || !rhiBuf) {
        throw GraphicsException("Cannot draw with invalid Pipeline or VertexBuffer!");
    }

    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
    rhiCommandList.SetScissor(0, 0, extent.width, extent.height);
    rhiCommandList.SetPipeline(*rhiPipe);
    rhiCommandList.SetVertexBuffer(0, *rhiBuf, 0);
    rhiCommandList.Draw(vertexCount);
}

void CommandContextImpl::DrawIndexed(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, const IndexBuffer& indexBuffer, uint32_t indexCount) {
    if (!isRecording) {
        throw GraphicsException("Cannot draw indexed outside of an active frame!");
    }

    EnsurePassActive();

    auto rhiPipe = pipeline.GetRHIPipeline();
    auto rhiVBuf = vertexBuffer.GetRHIBuffer();
    auto rhiIBuf = indexBuffer.GetRHIBuffer();
    if (!rhiPipe || !rhiVBuf || !rhiIBuf) {
        throw GraphicsException("Cannot draw with invalid Pipeline, VertexBuffer or IndexBuffer!");
    }

    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
    rhiCommandList.SetScissor(0, 0, extent.width, extent.height);
    rhiCommandList.SetPipeline(*rhiPipe);
    rhiCommandList.SetVertexBuffer(0, *rhiVBuf, 0);
    rhiCommandList.SetIndexBuffer(*rhiIBuf, 0, indexBuffer.GetIndexType());
    rhiCommandList.DrawIndexed(indexCount);
}

void CommandContextImpl::End() {
    if (isRecording) {
        if (passActive) {
            vkCmdEndRenderPass(commandBuffer);
            passActive = false;
        }
        VK_CHECK(vkEndCommandBuffer(commandBuffer));
        isRecording = false;
    }
}

} // namespace Internal

CommandContext::CommandContext(Internal::CommandContextImpl* impl)
    : m_impl(impl) {}

CommandContext::~CommandContext() = default;

CommandContext::CommandContext(CommandContext&& other) noexcept
    : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

CommandContext& CommandContext::operator=(CommandContext&& other) noexcept {
    if (this != &other) {
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

void CommandContext::Clear(const Color& color) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->Clear(color);
}

void CommandContext::SetPipeline(const Pipeline& pipeline) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetPipeline(pipeline);
}

void CommandContext::SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetVertexBuffer(binding, vertexBuffer, offset);
}

void CommandContext::SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetIndexBuffer(indexBuffer, offset);
}

void CommandContext::SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetUniformBufferRaw(binding, buffer, size, offset);
}

void CommandContext::SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetTexture(binding, texture, sampler);
}

void CommandContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetViewport(x, y, width, height, minDepth, maxDepth);
}

void CommandContext::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetScissor(x, y, width, height);
}

void CommandContext::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandContext::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandContext::Draw(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, uint32_t vertexCount) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->Draw(pipeline, vertexBuffer, vertexCount);
}

void CommandContext::DrawIndexed(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, const IndexBuffer& indexBuffer, uint32_t indexCount) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->DrawIndexed(pipeline, vertexBuffer, indexBuffer, indexCount);
}

} // namespace Sunshine
