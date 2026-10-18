#include "vulkan_rhi_buffer.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"
#include <cstring>

namespace Sunshine::Vulkan {

VulkanRHIBuffer::VulkanRHIBuffer(VulkanContext& context, const RHI::BufferDesc& desc, const void* initialData)
    : m_context(context), m_desc(desc) 
{
    VkDevice device = m_context.GetDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;

    VkBufferUsageFlags usageFlags = 0;
    if (desc.usage & RHI::BufferUsage::Vertex) usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (desc.usage & RHI::BufferUsage::Index)  usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (desc.usage & RHI::BufferUsage::Uniform) usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

    VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (desc.memoryType == RHI::BufferMemoryType::GPUOnly) {
        memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, memProperties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_memory));
    VK_CHECK(vkBindBufferMemory(device, m_buffer, m_memory, 0));

    if (desc.memoryType == RHI::BufferMemoryType::CPUVisible) {
        VK_CHECK(vkMapMemory(device, m_memory, 0, desc.size, 0, &m_mappedData));
    }

    if (initialData) {
        UpdateData(initialData, desc.size, 0);
    }
}

VulkanRHIBuffer::~VulkanRHIBuffer() {
    VkDevice device = m_context.GetDevice();
    if (m_mappedData) {
        vkUnmapMemory(device, m_memory);
        m_mappedData = nullptr;
    }
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

void VulkanRHIBuffer::UpdateData(const void* data, size_t size, size_t offset) {
    if (offset + size > m_desc.size) {
        throw GraphicsException("Buffer update out of bounds!");
    }

    if (m_mappedData) {
        std::memcpy(static_cast<char*>(m_mappedData) + offset, data, size);
    } else {
        throw GraphicsException("Updating GPUOnly buffer without staging buffer is not supported yet!");
    }
}

uint32_t VulkanRHIBuffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context.GetPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw GraphicsException("Failed to find suitable memory type for Vulkan buffer!");
}

} // namespace Sunshine::Vulkan
