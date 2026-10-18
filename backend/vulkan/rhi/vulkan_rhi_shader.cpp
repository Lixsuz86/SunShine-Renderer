#include "vulkan_rhi_shader.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"

namespace Sunshine::Vulkan {

VulkanRHIShader::VulkanRHIShader(VulkanContext& context, const RHI::ShaderDesc& desc)
    : m_context(context), m_stage(desc.stage), m_entryPoint(desc.entryPoint) 
{
    if (desc.spirvCode.empty()) {
        throw GraphicsException("Cannot create Vulkan shader module from empty SPIR-V bytecode!");
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = desc.spirvCode.size_bytes();
    createInfo.pCode = desc.spirvCode.data();

    VK_CHECK(vkCreateShaderModule(m_context.GetDevice(), &createInfo, nullptr, &m_module));
}

VulkanRHIShader::~VulkanRHIShader() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_context.GetDevice(), m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

} // namespace Sunshine::Vulkan
