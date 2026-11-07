#include "sunshine/command_context.hpp"
#include "sunshine/shadow_map.hpp"
#include "sunshine/render_target.hpp"
#include "internal_types.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_buffer.hpp"
#include "sunshine/error.hpp"
#include <array>

namespace Sunshine {

namespace Internal {

void CommandContextImpl::Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, VkExtent2D ext, uint32_t frameIdx, DeviceImpl* dev) {
    commandBuffer = cmd;
    renderPass = rp;
    framebuffer = fb;
    extent = ext;
    frameIndex = frameIdx;
    deviceImpl = dev;
    passActive = false;
    isSecondaryPass = false;
    currentComputePipeline = nullptr;

    rhiCommandList.SetCommandBuffer(cmd);
    rhiCommandList.SetFrameIndex(frameIdx);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    isRecording = true;
}

void CommandContextImpl::BeginSecondaryPass(const Color& clearColor) {
    if (!isRecording) {
        throw GraphicsException("Cannot begin secondary pass outside of an active command buffer recording!");
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
    clearValues[0].color.float32[0] = clearColor.r;
    clearValues[0].color.float32[1] = clearColor.g;
    clearValues[0].color.float32[2] = clearColor.b;
    clearValues[0].color.float32[3] = clearColor.a;
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    passActive = true;
    isSecondaryPass = true;
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

void CommandContextImpl::BeginShadowPass(ShadowMapImpl& shadowMap) {
    if (!isRecording) {
        throw GraphicsException("Cannot begin shadow pass outside of an active command buffer recording!");
    }

    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowMap.renderPass;
    renderPassInfo.framebuffer = shadowMap.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {shadowMap.width, shadowMap.height};

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    passActive = true;

    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(shadowMap.width), static_cast<float>(shadowMap.height));
    rhiCommandList.SetScissor(0, 0, shadowMap.width, shadowMap.height);
}

void CommandContextImpl::EndShadowPass() {
    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
    }
}

void CommandContextImpl::BeginPass(RenderTargetImpl& target, const Color& clearColor) {
    if (!isRecording) {
        throw GraphicsException("Cannot begin render pass outside of an active command buffer recording!");
    }

    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = target.renderPass;
    renderPassInfo.framebuffer = target.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {target.desc.width, target.desc.height};

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.float32[0] = clearColor.r;
    clearValues[0].color.float32[1] = clearColor.g;
    clearValues[0].color.float32[2] = clearColor.b;
    clearValues[0].color.float32[3] = clearColor.a;
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = target.desc.hasDepth ? 2 : 1;
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    passActive = true;

    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(target.desc.width), static_cast<float>(target.desc.height));
    rhiCommandList.SetScissor(0, 0, target.desc.width, target.desc.height);
}

void CommandContextImpl::EndPass() {
    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
        isSecondaryPass = false;
    }
}

void CommandContextImpl::SetComputePipeline(const ComputePipeline& pipeline) {
    if (!isRecording) {
        throw GraphicsException("Cannot set compute pipeline outside active recording!");
    }
    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
        isSecondaryPass = false;
    }
    auto rhiPipe = pipeline.GetRHIComputePipeline();
    if (!rhiPipe) {
        throw GraphicsException("Invalid compute pipeline passed to SetComputePipeline!");
    }
    auto* vkPipe = dynamic_cast<Vulkan::VulkanRHIComputePipeline*>(rhiPipe.get());
    if (!vkPipe) {
        throw GraphicsException("Non-Vulkan compute pipeline passed to SetComputePipeline!");
    }
    currentComputePipeline = vkPipe;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipe->GetPipeline());
}

void CommandContextImpl::SetStorageBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    if (!buffer) return;
    auto* vkBuf = dynamic_cast<Vulkan::VulkanRHIBuffer*>(buffer.get());
    if (!vkBuf) return;

    if (currentComputePipeline) {
        currentComputePipeline->UpdateStorageBuffer(binding, vkBuf->GetBuffer(), offset, size, frameIndex);
    } else {
        rhiCommandList.SetStorageBuffer(binding, *buffer, size, offset);
    }
}

void CommandContextImpl::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (!isRecording) {
        throw GraphicsException("Cannot dispatch compute outside active recording!");
    }
    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
        isSecondaryPass = false;
    }
    if (!currentComputePipeline) {
        throw GraphicsException("Cannot dispatch without an active compute pipeline!");
    }
    if (currentComputePipeline->HasDescriptorSet()) {
        VkDescriptorSet ds = currentComputePipeline->GetDescriptorSet(frameIndex);
        if (ds != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, currentComputePipeline->GetPipelineLayout(), 0, 1, &ds, 0, nullptr);
        }
    }
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void CommandContextImpl::ComputeBarrier() {
    if (!isRecording) {
        throw GraphicsException("Cannot insert barrier outside active recording!");
    }
    if (passActive) {
        vkCmdEndRenderPass(commandBuffer);
        passActive = false;
        isSecondaryPass = false;
    }
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr
    );
}

void SecondaryCommandContextImpl::Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, uint32_t frameIdx, VkExtent2D ext) {
    commandBuffer = cmd;
    frameIndex = frameIdx;
    rhiCommandList.SetCommandBuffer(cmd);
    rhiCommandList.SetFrameIndex(frameIdx);

    VkCommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass = rp;
    inheritanceInfo.subpass = 0;
    inheritanceInfo.framebuffer = fb;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    rhiCommandList.SetViewport(0.0f, 0.0f, static_cast<float>(ext.width), static_cast<float>(ext.height));
    rhiCommandList.SetScissor(0, 0, ext.width, ext.height);
}

void SecondaryCommandContextImpl::End() {
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void CommandContextImpl::RecordParallel(WorkerThreadPool& pool, size_t batchCount, std::function<void(size_t batchIndex, size_t threadId, SecondaryCommandContext& secCmd)> recordFunc) {
    if (!passActive || !isSecondaryPass) {
        BeginSecondaryPass(Color::DarkSlate());
    }
    if (batchCount == 0 || !deviceImpl) return;

    VkDevice device = deviceImpl->context->GetDevice();

    struct BatchItem {
        VkCommandPool pool{VK_NULL_HANDLE};
        VkCommandBuffer cmd{VK_NULL_HANDLE};
        std::unique_ptr<SecondaryCommandContextImpl> impl;
    };
    std::vector<BatchItem> batches(batchCount);

    size_t threadCount = pool.GetThreadCount();
    for (size_t i = 0; i < batchCount; ++i) {
        size_t threadId = (threadCount > 0) ? (i % threadCount) : 0;
        VkCommandPool cmdPool = deviceImpl->GetThreadCommandPool(static_cast<uint32_t>(threadId), frameIndex);
        batches[i].pool = cmdPool;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &batches[i].cmd));

        batches[i].impl = std::make_unique<SecondaryCommandContextImpl>();
        batches[i].impl->Begin(batches[i].cmd, renderPass, framebuffer, frameIndex, extent);
    }

    pool.ParallelFor(batchCount, [&](size_t batchIndex, size_t threadId) {
        SecondaryCommandContext secContext(std::move(batches[batchIndex].impl));
        recordFunc(batchIndex, threadId, secContext);
        batches[batchIndex].impl = std::move(secContext.m_impl);
        batches[batchIndex].impl->End();
    });

    std::vector<VkCommandBuffer> secondaryCmds(batchCount);
    for (size_t i = 0; i < batchCount; ++i) {
        secondaryCmds[i] = batches[i].cmd;
    }

    vkCmdExecuteCommands(commandBuffer, static_cast<uint32_t>(secondaryCmds.size()), secondaryCmds.data());

    for (size_t i = 0; i < batchCount; ++i) {
        vkFreeCommandBuffers(device, batches[i].pool, 1, &batches[i].cmd);
    }
}

void CommandContextImpl::ExecuteSecondaryCommands(std::span<const SecondaryCommandContext> secondaryContexts) {
    if (secondaryContexts.empty()) return;
    std::vector<VkCommandBuffer> cmdBuffers;
    cmdBuffers.reserve(secondaryContexts.size());
    for (const auto& sec : secondaryContexts) {
        if (sec.m_impl && sec.m_impl->commandBuffer != VK_NULL_HANDLE) {
            cmdBuffers.push_back(sec.m_impl->commandBuffer);
        }
    }
    if (!cmdBuffers.empty()) {
        vkCmdExecuteCommands(commandBuffer, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    }
}

void CommandContextImpl::EnsurePassActive() {
    if (!passActive) {
        Clear(Color::DarkSlate());
    }
}

void CommandContextImpl::SetPipeline(const Pipeline& pipeline) {
    currentComputePipeline = nullptr;
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
    if (!buffer) throw GraphicsException("Invalid buffer passed to SetUniformBufferRaw!");

    if (currentComputePipeline) {
        auto* vkBuf = dynamic_cast<Vulkan::VulkanRHIBuffer*>(buffer.get());
        if (vkBuf) {
            currentComputePipeline->UpdateUniformBuffer(binding, vkBuf->GetBuffer(), offset, size, frameIndex);
        }
        return;
    }

    EnsurePassActive();
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

void CommandContext::BeginShadowPass(ShadowMap& shadowMap) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    auto* smImpl = shadowMap.GetImpl();
    if (!smImpl) throw GraphicsException("Invalid ShadowMap passed to BeginShadowPass!");
    m_impl->BeginShadowPass(*smImpl);
}

void CommandContext::EndShadowPass() {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->EndShadowPass();
}

void CommandContext::BeginPass(RenderTarget& target, const Color& clearColor) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    auto* rtImpl = target.GetImpl();
    if (!rtImpl) throw GraphicsException("Invalid RenderTarget passed to BeginPass!");
    m_impl->BeginPass(*rtImpl, clearColor);
}

void CommandContext::BeginSecondaryPass(const Color& clearColor) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->BeginSecondaryPass(clearColor);
}

void CommandContext::EndPass() {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->EndPass();
}

void CommandContext::SetComputePipeline(const ComputePipeline& pipeline) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetComputePipeline(pipeline);
}

void CommandContext::SetStorageBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->SetStorageBufferRaw(binding, buffer, size, offset);
}

void CommandContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandContext::ComputeToGraphicsBarrier() {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->ComputeBarrier();
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

void CommandContext::RecordParallel(WorkerThreadPool& pool, size_t batchCount, std::function<void(size_t batchIndex, size_t threadId, SecondaryCommandContext& secCmd)> recordFunc) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->RecordParallel(pool, batchCount, std::move(recordFunc));
}

void CommandContext::ExecuteSecondaryCommands(std::span<const SecondaryCommandContext> secondaryContexts) {
    if (!m_impl) throw GraphicsException("Invalid or moved CommandContext called!");
    m_impl->ExecuteSecondaryCommands(secondaryContexts);
}

} // namespace Sunshine
