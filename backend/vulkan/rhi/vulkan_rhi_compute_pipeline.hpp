#pragma once

#include "sunshine/rhi/rhi_compute_pipeline.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHIComputePipeline : public RHI::IRHIComputePipeline {
public:
    VulkanRHIComputePipeline(VulkanContext& context, const RHI::ComputePipelineDesc& desc);
    ~VulkanRHIComputePipeline() override;

    VulkanRHIComputePipeline(const VulkanRHIComputePipeline&) = delete;
    VulkanRHIComputePipeline& operator=(const VulkanRHIComputePipeline&) = delete;

    const RHI::ComputePipelineDesc& GetDesc() const noexcept override { return m_desc; }

    VkPipeline GetPipeline() const noexcept { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const noexcept { return m_pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return m_descriptorSetLayout; }

    bool HasDescriptorSet() const noexcept { return !m_descriptorSets.empty(); }
    VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const;

    void UpdateUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex);
    void UpdateStorageBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex);
    void UpdateTexture(uint32_t binding, VkImageView imageView, VkSampler sampler, uint32_t frameIndex);

private:
    VulkanContext& m_context;
    RHI::ComputePipelineDesc m_desc;
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan

