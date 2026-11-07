#include "sunshine/render_target.hpp"
#include "sunshine/device.hpp"
#include "internal_types.hpp"
#include "sunshine/error.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_texture.hpp"
#include <array>

namespace Sunshine {

namespace Internal {

static VkFormat ConvertColorFormat(RHI::TextureFormat format) {
    switch (format) {
        case RHI::TextureFormat::RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHI::TextureFormat::RGBA8_SRGB:   return VK_FORMAT_R8G8B8A8_SRGB;
        case RHI::TextureFormat::RGBA8_UNORM:  return VK_FORMAT_R8G8B8A8_UNORM;
        default:                               return VK_FORMAT_R16G16B16A16_SFLOAT;
    }
}

RenderTargetImpl::RenderTargetImpl(Vulkan::VulkanContext& ctx, Vulkan::VulkanRHIDevice& rhiDev, const RenderTargetDesc& d)
    : context(ctx), rhiDevice(rhiDev), desc(d)
{
    // 1. Create color attachment texture
    RHI::TextureDesc colorDesc{};
    colorDesc.width = desc.width;
    colorDesc.height = desc.height;
    colorDesc.format = desc.colorFormat;
    colorDesc.isRenderTarget = true;
    colorDesc.debugName = desc.debugName.empty() ? "RenderTarget_Color" : (desc.debugName + "_Color");
    colorTexture = rhiDevice.CreateTexture(colorDesc);

    // 2. Create depth attachment texture if requested
    if (desc.hasDepth) {
        RHI::TextureDesc depthDesc{};
        depthDesc.width = desc.width;
        depthDesc.height = desc.height;
        depthDesc.format = desc.depthFormat;
        depthDesc.isRenderTarget = true;
        depthDesc.debugName = desc.debugName.empty() ? "RenderTarget_Depth" : (desc.debugName + "_Depth");
        depthTexture = rhiDevice.CreateTexture(depthDesc);
    }

    // 3. Create linear/nearest sampler
    RHI::SamplerDesc sDesc{};
    sDesc.minFilter = desc.filter;
    sDesc.magFilter = desc.filter;
    sDesc.addressModeU = desc.addressMode;
    sDesc.addressModeV = desc.addressMode;
    sDesc.addressModeW = desc.addressMode;
    rhiSampler = rhiDevice.CreateSampler(sDesc);

    // 4. Create RenderPass & Framebuffer
    CreateRenderPass();
    CreateFramebuffer();
}

RenderTargetImpl::~RenderTargetImpl() {
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

void RenderTargetImpl::CreateRenderPass() {
    VkDevice device = context.GetDevice();

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ConvertColorFormat(desc.colorFormat);
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    VkAttachmentReference depthRef{};
    if (desc.hasDepth) {
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = desc.hasDepth ? &depthRef : nullptr;

    std::array<VkSubpassDependency, 2> dependencies{};
    // Dependency 0: External -> Subpass 0
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (desc.hasDepth) {
        dependencies[0].dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Dependency 1: Subpass 0 -> External
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    if (desc.hasDepth) {
        dependencies[1].srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::vector<VkAttachmentDescription> attachments;
    attachments.push_back(colorAttachment);
    if (desc.hasDepth) {
        attachments.push_back(depthAttachment);
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void RenderTargetImpl::CreateFramebuffer() {
    VkDevice device = context.GetDevice();

    auto* vkColor = dynamic_cast<Vulkan::VulkanRHITexture*>(colorTexture.get());
    if (!vkColor) {
        throw GraphicsException("Failed to get VulkanRHITexture for RenderTarget color attachment!");
    }

    std::vector<VkImageView> attachments;
    attachments.push_back(vkColor->GetImageView());

    if (desc.hasDepth && depthTexture) {
        auto* vkDepth = dynamic_cast<Vulkan::VulkanRHITexture*>(depthTexture.get());
        if (!vkDepth) {
            throw GraphicsException("Failed to get VulkanRHITexture for RenderTarget depth attachment!");
        }
        attachments.push_back(vkDepth->GetImageView());
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = desc.width;
    framebufferInfo.height = desc.height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer));
}

} // namespace Internal

RenderTarget::RenderTarget(Device& device, const RenderTargetDesc& desc) {
    auto* devImpl = device.GetImpl();
    if (!devImpl) {
        throw GraphicsException("Cannot construct RenderTarget with invalid Device!");
    }
    m_impl = std::make_unique<Internal::RenderTargetImpl>(*devImpl->context, *devImpl->rhiDevice, desc);
}

RenderTarget::~RenderTarget() = default;

RenderTarget::RenderTarget(RenderTarget&&) noexcept = default;
RenderTarget& RenderTarget::operator=(RenderTarget&&) noexcept = default;

uint32_t RenderTarget::GetWidth() const noexcept {
    return m_impl ? m_impl->desc.width : 0;
}

uint32_t RenderTarget::GetHeight() const noexcept {
    return m_impl ? m_impl->desc.height : 0;
}

RHI::TextureFormat RenderTarget::GetColorFormat() const noexcept {
    return m_impl ? m_impl->desc.colorFormat : RHI::TextureFormat::RGBA16_FLOAT;
}

bool RenderTarget::HasDepth() const noexcept {
    return m_impl ? m_impl->desc.hasDepth : false;
}

Texture RenderTarget::GetColorTexture() const {
    return Texture(m_impl ? m_impl->colorTexture : nullptr);
}

Texture RenderTarget::GetDepthTexture() const {
    return Texture(m_impl ? m_impl->depthTexture : nullptr);
}

Sampler RenderTarget::GetSampler() const {
    return Sampler(m_impl ? m_impl->rhiSampler : nullptr);
}

} // namespace Sunshine

