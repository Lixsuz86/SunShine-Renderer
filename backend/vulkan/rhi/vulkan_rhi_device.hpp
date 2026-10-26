#pragma once

#include "sunshine/rhi/rhi_device.hpp"
#include "backend/vulkan/vulkan_common.hpp"
#include <unordered_map>

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHIDevice : public RHI::IRHIDevice {
public:
    explicit VulkanRHIDevice(VulkanContext& context);
    ~VulkanRHIDevice() override = default;

    void SetCompatibleRenderPass(VkRenderPass renderPass) noexcept { m_compatibleRenderPass = renderPass; }
    VkRenderPass GetCompatibleRenderPass() const noexcept { return m_compatibleRenderPass; }

    std::shared_ptr<RHI::IRHIBuffer> CreateBuffer(const RHI::BufferDesc& desc, const void* initialData = nullptr) override;
    std::shared_ptr<RHI::IRHIShader> CreateShader(const RHI::ShaderDesc& desc) override;
    std::shared_ptr<RHI::IRHIPipeline> CreatePipeline(const RHI::PipelineDesc& desc) override;
    std::shared_ptr<RHI::IRHIPipeline> CreatePipeline(const RHI::PipelineDesc& desc, VkRenderPass compatibleRenderPass);
    std::shared_ptr<RHI::IRHITexture> CreateTexture(const RHI::TextureDesc& desc, const void* initialData = nullptr) override;
    std::shared_ptr<RHI::IRHISampler> CreateSampler(const RHI::SamplerDesc& desc) override;
    void WaitIdle() override;

private:
    static size_t HashPipelineDesc(const RHI::PipelineDesc& desc);

    VulkanContext& m_context;
    VkRenderPass m_compatibleRenderPass{VK_NULL_HANDLE};
    std::unordered_map<size_t, std::shared_ptr<RHI::IRHIPipeline>> m_pipelineCache;
};

} // namespace Sunshine::Vulkan
