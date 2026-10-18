#include "vulkan_rhi_command_list.hpp"
#include "vulkan_rhi_pipeline.hpp"
#include "vulkan_rhi_buffer.hpp"
#include "vulkan_rhi_texture.hpp"
#include "vulkan_rhi_sampler.hpp"
#include "sunshine/error.hpp"
#include <iostream>

namespace Sunshine::Vulkan {

VulkanRHICommandList::VulkanRHICommandList(VkCommandBuffer cmd)
    : m_cmd(cmd) {}

void VulkanRHICommandList::SetPipeline(RHI::IRHIPipeline& pipeline) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot set pipeline on an inactive command list!");
    }
    auto* vkPipe = dynamic_cast<VulkanRHIPipeline*>(&pipeline);
    if (!vkPipe) {
        throw GraphicsException("Invalid pipeline object passed to VulkanRHICommandList!");
    }
    m_currentPipeline = vkPipe;
    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipe->GetPipeline());
}

void VulkanRHICommandList::SetVertexBuffer(uint32_t binding, RHI::IRHIBuffer& buffer, uint64_t offset) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot bind vertex buffer on an inactive command list!");
    }
    auto* vkBuf = dynamic_cast<VulkanRHIBuffer*>(&buffer);
    if (!vkBuf) {
        throw GraphicsException("Invalid buffer object passed to VulkanRHICommandList!");
    }
    VkBuffer bufferHandle = vkBuf->GetBuffer();
    VkDeviceSize vkOffset = offset;
    vkCmdBindVertexBuffers(m_cmd, binding, 1, &bufferHandle, &vkOffset);
}

void VulkanRHICommandList::SetIndexBuffer(RHI::IRHIBuffer& buffer, uint64_t offset, RHI::IndexType indexType) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot bind index buffer on an inactive command list!");
    }
    auto* vkBuf = dynamic_cast<VulkanRHIBuffer*>(&buffer);
    if (!vkBuf) {
        throw GraphicsException("Invalid buffer object passed to VulkanRHICommandList!");
    }
    VkIndexType vkIndexType = (indexType == RHI::IndexType::Uint16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(m_cmd, vkBuf->GetBuffer(), offset, vkIndexType);
}

void VulkanRHICommandList::SetUniformBuffer(uint32_t binding, RHI::IRHIBuffer& buffer, size_t size, size_t offset) {
    if (!m_currentPipeline) {
        throw GraphicsException("Cannot set uniform buffer without an active pipeline bound!");
    }
    auto* vkBuf = dynamic_cast<VulkanRHIBuffer*>(&buffer);
    if (!vkBuf) {
        throw GraphicsException("Invalid buffer object passed to SetUniformBuffer!");
    }
    m_currentPipeline->UpdateUniformBuffer(binding, vkBuf->GetBuffer(), offset, size, m_frameIndex);
}

void VulkanRHICommandList::SetTexture(uint32_t binding, RHI::IRHITexture& texture, RHI::IRHISampler& sampler) {
    if (!m_currentPipeline) {
        throw GraphicsException("Cannot set texture without an active pipeline bound!");
    }
    auto* vkTex = dynamic_cast<VulkanRHITexture*>(&texture);
    auto* vkSampler = dynamic_cast<VulkanRHISampler*>(&sampler);
    if (!vkTex || !vkSampler) {
        throw GraphicsException("Invalid texture or sampler object passed to SetTexture!");
    }
    m_currentPipeline->UpdateTexture(binding, vkTex->GetImageView(), vkSampler->GetSampler(), m_frameIndex);
}

void VulkanRHICommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot set viewport on an inactive command list!");
    }
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    vkCmdSetViewport(m_cmd, 0, 1, &viewport);
}

void VulkanRHICommandList::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot set scissor on an inactive command list!");
    }
    VkRect2D scissor{};
    scissor.offset = { x, y };
    scissor.extent = { width, height };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void VulkanRHICommandList::BindDescriptorsIfNeeded() {
    if (m_currentPipeline && m_currentPipeline->HasDescriptorSet()) {
        VkDescriptorSet ds = m_currentPipeline->GetDescriptorSet(m_frameIndex);
        if (ds != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPipeline->GetPipelineLayout(), 0, 1, &ds, 0, nullptr);
        }
    }
}

void VulkanRHICommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot draw on an inactive command list!");
    }
    BindDescriptorsIfNeeded();
    vkCmdDraw(m_cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanRHICommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (m_cmd == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot draw indexed on an inactive command list!");
    }
    BindDescriptorsIfNeeded();
    vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

} // namespace Sunshine::Vulkan
