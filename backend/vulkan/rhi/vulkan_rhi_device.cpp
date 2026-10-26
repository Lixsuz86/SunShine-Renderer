#include "vulkan_rhi_device.hpp"
#include "vulkan_rhi_buffer.hpp"
#include "vulkan_rhi_shader.hpp"
#include "vulkan_rhi_pipeline.hpp"
#include "vulkan_rhi_texture.hpp"
#include "vulkan_rhi_sampler.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"
#include <iostream>

namespace Sunshine::Vulkan {

VulkanRHIDevice::VulkanRHIDevice(VulkanContext& context)
    : m_context(context) {}

std::shared_ptr<RHI::IRHIBuffer> VulkanRHIDevice::CreateBuffer(const RHI::BufferDesc& desc, const void* initialData) {
    return std::make_shared<VulkanRHIBuffer>(m_context, desc, initialData);
}

std::shared_ptr<RHI::IRHIShader> VulkanRHIDevice::CreateShader(const RHI::ShaderDesc& desc) {
    return std::make_shared<VulkanRHIShader>(m_context, desc);
}

std::shared_ptr<RHI::IRHITexture> VulkanRHIDevice::CreateTexture(const RHI::TextureDesc& desc, const void* initialData) {
    return std::make_shared<VulkanRHITexture>(m_context, desc, initialData);
}

std::shared_ptr<RHI::IRHISampler> VulkanRHIDevice::CreateSampler(const RHI::SamplerDesc& desc) {
    return std::make_shared<VulkanRHISampler>(m_context, desc);
}

size_t VulkanRHIDevice::HashPipelineDesc(const RHI::PipelineDesc& desc) {
    size_t hash = 17;
    auto hashCombine = [&hash](size_t val) {
        hash ^= val + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    };

    hashCombine(reinterpret_cast<size_t>(desc.vertexShader.get()));
    hashCombine(reinterpret_cast<size_t>(desc.fragmentShader.get()));
    hashCombine(desc.vertexLayout.stride);
    for (const auto& attr : desc.vertexLayout.attributes) {
        hashCombine(attr.location);
        hashCombine(static_cast<size_t>(attr.format));
        hashCombine(attr.offset);
    }
    hashCombine(static_cast<size_t>(desc.topology));
    hashCombine(static_cast<size_t>(desc.rasterizer.cullMode));
    hashCombine(static_cast<size_t>(desc.rasterizer.frontFace));
    hashCombine(static_cast<size_t>(desc.rasterizer.polygonMode));
    hashCombine(desc.blend.blendEnable ? 1 : 0);
    hashCombine(desc.depthStencil.depthTestEnable ? 1 : 0);
    hashCombine(desc.depthStencil.depthWriteEnable ? 1 : 0);
    hashCombine(static_cast<size_t>(desc.depthStencil.depthCompareOp));
    for (const auto& b : desc.descriptorBindings) {
        hashCombine(b.binding);
        hashCombine(static_cast<size_t>(b.type));
        hashCombine(static_cast<size_t>(b.stageFlags));
    }

    return hash;
}

std::shared_ptr<RHI::IRHIPipeline> VulkanRHIDevice::CreatePipeline(const RHI::PipelineDesc& desc) {
    return CreatePipeline(desc, m_compatibleRenderPass);
}

std::shared_ptr<RHI::IRHIPipeline> VulkanRHIDevice::CreatePipeline(const RHI::PipelineDesc& desc, VkRenderPass compatibleRenderPass) {
    VkRenderPass rp = (compatibleRenderPass != VK_NULL_HANDLE) ? compatibleRenderPass : m_compatibleRenderPass;
    if (rp == VK_NULL_HANDLE) {
        throw GraphicsException("Cannot create Vulkan pipeline without a compatible render pass configured on RHI device!");
    }

    size_t hash = HashPipelineDesc(desc);
    hash ^= std::hash<uint64_t>{}(reinterpret_cast<uint64_t>(rp)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

    auto it = m_pipelineCache.find(hash);
    if (it != m_pipelineCache.end()) {
        std::cout << "[Sunshine RHI] Pipeline cache HIT for pipeline: " 
                  << (desc.debugName.empty() ? "unnamed" : desc.debugName) << std::endl;
        return it->second;
    }

    std::cout << "[Sunshine RHI] Creating and caching new pipeline: " 
                  << (desc.debugName.empty() ? "unnamed" : desc.debugName) << std::endl;
    auto pipeline = std::make_shared<VulkanRHIPipeline>(m_context, rp, desc);
    m_pipelineCache[hash] = pipeline;
    return pipeline;
}

void VulkanRHIDevice::WaitIdle() {
    m_context.WaitIdle();
}

} // namespace Sunshine::Vulkan
