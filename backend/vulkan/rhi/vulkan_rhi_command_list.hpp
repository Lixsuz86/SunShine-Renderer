#pragma once

#include "sunshine/rhi/rhi_command_list.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanRHICommandList : public RHI::IRHICommandList {
public:
    explicit VulkanRHICommandList(VkCommandBuffer cmd = VK_NULL_HANDLE);
    ~VulkanRHICommandList() override = default;

    void SetCommandBuffer(VkCommandBuffer cmd) noexcept { m_cmd = cmd; }
    VkCommandBuffer GetCommandBuffer() const noexcept { return m_cmd; }
    void SetFrameIndex(uint32_t frameIndex) noexcept { m_frameIndex = frameIndex; }

    void SetPipeline(RHI::IRHIPipeline& pipeline) override;
    void SetVertexBuffer(uint32_t binding, RHI::IRHIBuffer& buffer, uint64_t offset = 0) override;
    void SetIndexBuffer(RHI::IRHIBuffer& buffer, uint64_t offset, RHI::IndexType indexType) override;
    void SetUniformBuffer(uint32_t binding, RHI::IRHIBuffer& buffer, size_t size, size_t offset = 0) override;
    void SetTexture(uint32_t binding, RHI::IRHITexture& texture, RHI::IRHISampler& sampler) override;

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;

private:
    void BindDescriptorsIfNeeded();

    VkCommandBuffer m_cmd{VK_NULL_HANDLE};
    uint32_t m_frameIndex{0};
    class VulkanRHIPipeline* m_currentPipeline{nullptr};
};

} // namespace Sunshine::Vulkan
