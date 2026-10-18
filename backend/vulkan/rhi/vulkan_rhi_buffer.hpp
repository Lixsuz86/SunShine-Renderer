#pragma once

#include "sunshine/rhi/rhi_buffer.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHIBuffer : public RHI::IRHIBuffer {
public:
    VulkanRHIBuffer(VulkanContext& context, const RHI::BufferDesc& desc, const void* initialData = nullptr);
    ~VulkanRHIBuffer() override;

    VulkanRHIBuffer(const VulkanRHIBuffer&) = delete;
    VulkanRHIBuffer& operator=(const VulkanRHIBuffer&) = delete;

    size_t GetSize() const noexcept override { return m_desc.size; }
    RHI::BufferUsage GetUsage() const noexcept override { return m_desc.usage; }
    void UpdateData(const void* data, size_t size, size_t offset = 0) override;

    VkBuffer GetBuffer() const noexcept { return m_buffer; }

private:
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext& m_context;
    RHI::BufferDesc m_desc;
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    void* m_mappedData{nullptr};
};

} // namespace Sunshine::Vulkan
