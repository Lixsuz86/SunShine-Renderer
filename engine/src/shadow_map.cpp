#include "sunshine/shadow_map.hpp"
#include "sunshine/device.hpp"
#include "internal_types.hpp"
#include "sunshine/error.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_texture.hpp"
#include <array>

namespace Sunshine {

namespace Internal {

ShadowMapImpl::ShadowMapImpl(Vulkan::VulkanContext& ctx, Vulkan::VulkanRHIDevice& rhiDev, uint32_t w, uint32_t h)
    : context(ctx), rhiDevice(rhiDev), width(w), height(h) 
{
    CreateRenderPass();

    // Create D32_SFLOAT depth texture
    RHI::TextureDesc texDesc{};
    texDesc.width = width;
    texDesc.height = height;
    texDesc.format = RHI::TextureFormat::D32_FLOAT;
    texDesc.debugName = "ShadowMap_DepthTexture";
    rhiTexture = rhiDevice.CreateTexture(texDesc);

    // Create shadow sampler with clamp-to-border (white border)
    RHI::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = RHI::FilterMode::Linear;
    samplerDesc.magFilter = RHI::FilterMode::Linear;
    samplerDesc.addressModeU = RHI::AddressMode::ClampToBorder;
    samplerDesc.addressModeV = RHI::AddressMode::ClampToBorder;
    samplerDesc.addressModeW = RHI::AddressMode::ClampToBorder;
    samplerDesc.borderColor = RHI::BorderColor::FloatOpaqueWhite;
    rhiSampler = rhiDevice.CreateSampler(samplerDesc);

    CreateFramebuffer();
}

ShadowMapImpl::~ShadowMapImpl() {
    VkDevice device = context.GetDevice();
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

void ShadowMapImpl::CreateRenderPass() {
    VkDevice device = context.GetDevice();

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    // Dependency 0: External -> Subpass 0
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Dependency 1: Subpass 0 -> External
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void ShadowMapImpl::CreateFramebuffer() {
    VkDevice device = context.GetDevice();

    auto* vkTex = dynamic_cast<Vulkan::VulkanRHITexture*>(rhiTexture.get());
    if (!vkTex) {
        throw GraphicsException("Failed to get VulkanRHITexture for ShadowMap framebuffer!");
    }

    VkImageView imageView = vkTex->GetImageView();

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &imageView;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer));
}

} // namespace Internal

ShadowMap::ShadowMap(Device& device, uint32_t width, uint32_t height) {
    auto* devImpl = device.GetImpl();
    if (!devImpl) {
        throw GraphicsException("Cannot construct ShadowMap with invalid Device!");
    }
    m_impl = std::make_unique<Internal::ShadowMapImpl>(*devImpl->context, *devImpl->rhiDevice, width, height);
}

ShadowMap::~ShadowMap() = default;

ShadowMap::ShadowMap(ShadowMap&&) noexcept = default;
ShadowMap& ShadowMap::operator=(ShadowMap&&) noexcept = default;

uint32_t ShadowMap::GetWidth() const noexcept {
    return m_impl ? m_impl->width : 0;
}

uint32_t ShadowMap::GetHeight() const noexcept {
    return m_impl ? m_impl->height : 0;
}

Texture ShadowMap::GetTexture() const {
    return Texture(m_impl ? m_impl->rhiTexture : nullptr);
}

Sampler ShadowMap::GetSampler() const {
    return Sampler(m_impl ? m_impl->rhiSampler : nullptr);
}

} // namespace Sunshine
