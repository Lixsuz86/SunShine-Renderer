#pragma once

#include "sunshine/rhi/rhi_sampler.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHISampler : public RHI::IRHISampler {
public:
    VulkanRHISampler(VulkanContext& context, const RHI::SamplerDesc& desc);
    ~VulkanRHISampler() override;

    VulkanRHISampler(const VulkanRHISampler&) = delete;
    VulkanRHISampler& operator=(const VulkanRHISampler&) = delete;

    const RHI::SamplerDesc& GetDesc() const noexcept override { return m_desc; }
    VkSampler GetSampler() const noexcept { return m_sampler; }

private:
    static VkFilter ConvertFilter(RHI::FilterMode filter);
    static VkSamplerAddressMode ConvertAddressMode(RHI::AddressMode mode);

    VulkanContext& m_context;
    RHI::SamplerDesc m_desc;
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan
