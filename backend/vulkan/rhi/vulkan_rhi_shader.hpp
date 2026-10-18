#pragma once

#include "sunshine/rhi/rhi_shader.hpp"
#include "backend/vulkan/vulkan_common.hpp"

namespace Sunshine::Vulkan {

class VulkanContext;

class VulkanRHIShader : public RHI::IRHIShader {
public:
    VulkanRHIShader(VulkanContext& context, const RHI::ShaderDesc& desc);
    ~VulkanRHIShader() override;

    VulkanRHIShader(const VulkanRHIShader&) = delete;
    VulkanRHIShader& operator=(const VulkanRHIShader&) = delete;

    RHI::ShaderStage GetStage() const noexcept override { return m_stage; }
    const std::string& GetEntryPoint() const noexcept override { return m_entryPoint; }

    VkShaderModule GetModule() const noexcept { return m_module; }

private:
    VulkanContext& m_context;
    RHI::ShaderStage m_stage;
    std::string m_entryPoint;
    VkShaderModule m_module{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan
