#pragma once

#include "sunshine/rhi/rhi_texture.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHITexture : public RHI::IRHITexture {
public:
    VulkanRHITexture(VulkanContext& context, const RHI::TextureDesc& desc, const void* initialData = nullptr);
    ~VulkanRHITexture() override;

    VulkanRHITexture(const VulkanRHITexture&) = delete;
    VulkanRHITexture& operator=(const VulkanRHITexture&) = delete;

    uint32_t GetWidth() const noexcept override { return m_desc.width; }
    uint32_t GetHeight() const noexcept override { return m_desc.height; }
    RHI::TextureFormat GetFormat() const noexcept override { return m_desc.format; }

    VkImage GetImage() const noexcept { return m_image; }
    VkImageView GetImageView() const noexcept { return m_imageView; }

private:
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext& m_context;
    RHI::TextureDesc m_desc;
    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan
