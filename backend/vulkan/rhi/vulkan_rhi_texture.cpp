#include "vulkan_rhi_texture.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"
#include <cstring>

namespace Sunshine::Vulkan {

VulkanRHITexture::VulkanRHITexture(VulkanContext& context, const RHI::TextureDesc& desc, const void* initialData)
    : m_context(context), m_desc(desc) 
{
    VkDevice device = m_context.GetDevice();

    VkFormat vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    if (desc.format == RHI::TextureFormat::RGBA8_SRGB) {
        vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vkFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &m_image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_memory));
    VK_CHECK(vkBindImageMemory(device, m_image, m_memory, 0));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_imageView));

    if (initialData) {
        VkDeviceSize imageSize = desc.width * desc.height * 4;

        // Create CPU staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

        VkMemoryRequirements stagingMemReq;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemReq);

        VkMemoryAllocateInfo stagingAlloc{};
        stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stagingAlloc.allocationSize = stagingMemReq.size;
        stagingAlloc.memoryTypeIndex = FindMemoryType(stagingMemReq.memoryTypeBits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingBufferMemory));
        VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0));

        void* data = nullptr;
        VK_CHECK(vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data));
        std::memcpy(data, initialData, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // Transition image layout & copy from staging
        m_context.TransitionImageLayout(m_image, vkFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_context.CopyBufferToImage(stagingBuffer, m_image, desc.width, desc.height);
        m_context.TransitionImageLayout(m_image, vkFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
}

VulkanRHITexture::~VulkanRHITexture() {
    VkDevice device = m_context.GetDevice();
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

uint32_t VulkanRHITexture::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context.GetPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw GraphicsException("Failed to find suitable memory type for texture image!");
}

} // namespace Sunshine::Vulkan
