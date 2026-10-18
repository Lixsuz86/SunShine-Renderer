#pragma once

#include "sunshine/rhi/rhi_pipeline.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHIPipeline : public RHI::IRHIPipeline {
public:
    VulkanRHIPipeline(VulkanContext& context, VkRenderPass compatibleRenderPass, const RHI::PipelineDesc& desc);
    ~VulkanRHIPipeline() override;

    VulkanRHIPipeline(const VulkanRHIPipeline&) = delete;
    VulkanRHIPipeline& operator=(const VulkanRHIPipeline&) = delete;

    const RHI::PipelineDesc& GetDesc() const noexcept override { return m_desc; }

    VkPipeline GetPipeline() const noexcept { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const noexcept { return m_pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return m_descriptorSetLayout; }

    bool HasDescriptorSet() const noexcept { return !m_descriptorSets.empty(); }
    VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const;

    void UpdateUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex);
    void UpdateTexture(uint32_t binding, VkImageView imageView, VkSampler sampler, uint32_t frameIndex);

private:
    static VkFormat ConvertVertexFormat(RHI::VertexFormat format);
    static VkPrimitiveTopology ConvertTopology(RHI::PrimitiveTopology topology);
    static VkCullModeFlags ConvertCullMode(RHI::CullMode mode);
    static VkFrontFace ConvertFrontFace(RHI::FrontFace face);
    static VkPolygonMode ConvertPolygonMode(RHI::PolygonMode mode);
    static VkCompareOp ConvertCompareOp(RHI::CompareOp op);

    VulkanContext& m_context;
    RHI::PipelineDesc m_desc;
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan
