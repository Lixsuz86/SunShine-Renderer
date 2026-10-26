#include "vulkan_rhi_sampler.hpp"
#include "backend/vulkan/vulkan_context.hpp"

namespace Sunshine::Vulkan {

VkFilter VulkanRHISampler::ConvertFilter(RHI::FilterMode filter) {
    switch (filter) {
        case RHI::FilterMode::Nearest: return VK_FILTER_NEAREST;
        case RHI::FilterMode::Linear:  return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode VulkanRHISampler::ConvertAddressMode(RHI::AddressMode mode) {
    switch (mode) {
        case RHI::AddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RHI::AddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RHI::AddressMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RHI::AddressMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static VkBorderColor ConvertBorderColor(RHI::BorderColor color) {
    switch (color) {
        case RHI::BorderColor::FloatOpaqueBlack:      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case RHI::BorderColor::IntOpaqueBlack:        return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        case RHI::BorderColor::FloatOpaqueWhite:      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case RHI::BorderColor::IntOpaqueWhite:        return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        case RHI::BorderColor::FloatTransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case RHI::BorderColor::IntTransparentBlack:   return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        default: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
}

VulkanRHISampler::VulkanRHISampler(VulkanContext& context, const RHI::SamplerDesc& desc)
    : m_context(context), m_desc(desc) 
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = ConvertFilter(desc.magFilter);
    samplerInfo.minFilter = ConvertFilter(desc.minFilter);
    samplerInfo.addressModeU = ConvertAddressMode(desc.addressModeU);
    samplerInfo.addressModeV = ConvertAddressMode(desc.addressModeV);
    samplerInfo.addressModeW = ConvertAddressMode(desc.addressModeW);
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = ConvertBorderColor(desc.borderColor);
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VK_CHECK(vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_sampler));
}

VulkanRHISampler::~VulkanRHISampler() {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context.GetDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}

} // namespace Sunshine::Vulkan
