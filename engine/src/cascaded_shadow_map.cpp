#include "sunshine/cascaded_shadow_map.hpp"
#include "sunshine/device.hpp"
#include "sunshine/camera.hpp"
#include "internal_types.hpp"
#include "sunshine/error.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_texture.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <algorithm>

namespace Sunshine {

namespace Internal {

CascadedShadowMapImpl::CascadedShadowMapImpl(Vulkan::VulkanContext& ctx, Vulkan::VulkanRHIDevice& rhiDev, uint32_t res, uint32_t cascades)
    : context(ctx), rhiDevice(rhiDev), resolution(res), cascadeCount(std::max(1u, cascades)) 
{
    lightSpaceMatrices.resize(cascadeCount, glm::mat4(1.0f));
    cascadeSplits.resize(cascadeCount, 0.0f);

    CreateRenderPass();

    // Create 2D Texture Array D32_SFLOAT depth texture
    RHI::TextureDesc texDesc{};
    texDesc.width = resolution;
    texDesc.height = resolution;
    texDesc.format = RHI::TextureFormat::D32_FLOAT;
    texDesc.type = RHI::TextureType::Texture2DArray;
    texDesc.arrayLayers = cascadeCount;
    texDesc.debugName = "CSM_DepthTextureArray";
    rhiTexture = rhiDevice.CreateTexture(texDesc);

    // Create shadow sampler with clamp-to-border (white border) and linear filtering
    RHI::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = RHI::FilterMode::Linear;
    samplerDesc.magFilter = RHI::FilterMode::Linear;
    samplerDesc.addressModeU = RHI::AddressMode::ClampToBorder;
    samplerDesc.addressModeV = RHI::AddressMode::ClampToBorder;
    samplerDesc.addressModeW = RHI::AddressMode::ClampToBorder;
    samplerDesc.borderColor = RHI::BorderColor::FloatOpaqueWhite;
    rhiSampler = rhiDevice.CreateSampler(samplerDesc);

    CreateFramebuffers();
}

CascadedShadowMapImpl::~CascadedShadowMapImpl() {
    VkDevice device = context.GetDevice();
    for (VkFramebuffer fb : framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    framebuffers.clear();

    for (VkImageView view : layerViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    layerViews.clear();

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

void CascadedShadowMapImpl::CreateRenderPass() {
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

void CascadedShadowMapImpl::CreateFramebuffers() {
    VkDevice device = context.GetDevice();
    auto* vkTex = dynamic_cast<Vulkan::VulkanRHITexture*>(rhiTexture.get());
    if (!vkTex) {
        throw GraphicsException("Failed to access underlying Vulkan texture for CSM framebuffers!");
    }

    VkImage image = vkTex->GetImage();

    layerViews.resize(cascadeCount);
    framebuffers.resize(cascadeCount);

    for (uint32_t i = 0; i < cascadeCount; ++i) {
        // Individual layer 2D view for rendering each cascade slice
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &layerViews[i]));

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &layerViews[i];
        fbInfo.width = resolution;
        fbInfo.height = resolution;
        fbInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));
    }
}

void CascadedShadowMapImpl::UpdateCascades(const Camera& camera, const glm::vec3& lightDir, float maxShadowDistance, float lambda) {
    float nearClip = camera.GetNearPlane();
    float farClip = std::min(camera.GetFarPlane(), maxShadowDistance);
    float clipRange = farClip - nearClip;

    float ratio = farClip / nearClip;

    // 1. Calculate split depths using Practical Split Scheme (logarithmic + uniform)
    std::vector<float> splitDistances(cascadeCount);
    for (uint32_t i = 0; i < cascadeCount; ++i) {
        float p = static_cast<float>(i + 1) / static_cast<float>(cascadeCount);
        float logSplit = nearClip * std::pow(ratio, p);
        float uniformSplit = nearClip + clipRange * p;
        splitDistances[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
        cascadeSplits[i] = splitDistances[i];
    }

    // 2. Compute tight-fit orthographic projection and light view matrix with texel snapping
    glm::vec3 normLightDir = glm::normalize(lightDir);

    for (uint32_t i = 0; i < cascadeCount; ++i) {
        float prevSplit = (i == 0) ? nearClip : splitDistances[i - 1];
        float curSplit = splitDistances[i];

        // Frustum slice projection
        glm::mat4 sliceProj = glm::perspective(camera.GetFov(), camera.GetAspectRatio(), prevSplit, curSplit);
        sliceProj[1][1] *= -1.0f; // Vulkan Y-flip
        glm::mat4 invCamVP = glm::inverse(sliceProj * camera.GetViewMatrix());

        std::array<glm::vec3, 8> corners{};
        int cornerIdx = 0;
        for (unsigned int x = 0; x < 2; ++x) {
            for (unsigned int y = 0; y < 2; ++y) {
                for (unsigned int z = 0; z < 2; ++z) {
                    glm::vec4 pt = invCamVP * glm::vec4(
                        2.0f * static_cast<float>(x) - 1.0f,
                        2.0f * static_cast<float>(y) - 1.0f,
                        static_cast<float>(z), // Vulkan depth [0, 1]
                        1.0f
                    );
                    corners[cornerIdx++] = glm::vec3(pt) / pt.w;
                }
            }
        }

        // Bounding sphere center
        glm::vec3 center(0.0f);
        for (const auto& c : corners) {
            center += c;
        }
        center /= 8.0f;

        // Bounding sphere radius
        float radius = 0.0f;
        for (const auto& c : corners) {
            float dist = glm::length(c - center);
            radius = std::max(radius, dist);
        }
        // Round radius to prevent minor pulsation
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Texel snapping step size
        float texelSize = (2.0f * radius) / static_cast<float>(resolution);

        // Position light view camera behind the bounding sphere
        glm::vec3 lightCameraPos = center - normLightDir * (radius * 2.0f);
        glm::mat4 lightView = glm::lookAt(lightCameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

        // Sub-pixel texel snapping (eliminate shadow edge shimmering)
        glm::vec4 centerInLight = lightView * glm::vec4(center, 1.0f);
        float snapX = std::fmod(centerInLight.x, texelSize);
        float snapY = std::fmod(centerInLight.y, texelSize);

        glm::mat4 snapCorrection = glm::translate(glm::mat4(1.0f), glm::vec3(-snapX, -snapY, 0.0f));
        lightView = snapCorrection * lightView;

        // Tight-fit Orthographic Projection
        float minZ = 0.0f;
        float maxZ = radius * 4.0f;
        glm::mat4 lightOrtho = glm::ortho(-radius, radius, -radius, radius, minZ, maxZ);
        lightOrtho[1][1] *= -1.0f; // Vulkan Y-flip

        lightSpaceMatrices[i] = lightOrtho * lightView;
    }
}

} // namespace Internal

CascadedShadowMap::CascadedShadowMap(Device& device, uint32_t resolution, uint32_t cascadeCount)
    : m_impl(std::make_unique<Internal::CascadedShadowMapImpl>(
        *device.GetImpl()->context,
        *device.GetImpl()->rhiDevice,
        resolution,
        cascadeCount
    )) 
{}

CascadedShadowMap::~CascadedShadowMap() = default;
CascadedShadowMap::CascadedShadowMap(CascadedShadowMap&&) noexcept = default;
CascadedShadowMap& CascadedShadowMap::operator=(CascadedShadowMap&&) noexcept = default;

uint32_t CascadedShadowMap::GetResolution() const noexcept {
    return m_impl ? m_impl->resolution : 0;
}

uint32_t CascadedShadowMap::GetCascadeCount() const noexcept {
    return m_impl ? m_impl->cascadeCount : 0;
}

void CascadedShadowMap::UpdateCascades(const Camera& camera, const glm::vec3& lightDir, float maxShadowDistance, float lambda) {
    if (m_impl) {
        m_impl->UpdateCascades(camera, lightDir, maxShadowDistance, lambda);
    }
}

const glm::mat4& CascadedShadowMap::GetLightSpaceMatrix(uint32_t cascadeIndex) const {
    if (!m_impl || cascadeIndex >= m_impl->lightSpaceMatrices.size()) {
        throw GraphicsException("Cascade index out of range in GetLightSpaceMatrix!");
    }
    return m_impl->lightSpaceMatrices[cascadeIndex];
}

const std::vector<glm::mat4>& CascadedShadowMap::GetLightSpaceMatrices() const {
    static const std::vector<glm::mat4> empty;
    return m_impl ? m_impl->lightSpaceMatrices : empty;
}

const std::vector<float>& CascadedShadowMap::GetCascadeSplitDepths() const {
    static const std::vector<float> empty;
    return m_impl ? m_impl->cascadeSplits : empty;
}

glm::vec4 CascadedShadowMap::GetCascadeSplitDepthsVec4() const {
    glm::vec4 splits(0.0f);
    if (m_impl) {
        for (size_t i = 0; i < std::min<size_t>(4, m_impl->cascadeSplits.size()); ++i) {
            splits[static_cast<int>(i)] = m_impl->cascadeSplits[i];
        }
    }
    return splits;
}

Texture CascadedShadowMap::GetTexture() const {
    return m_impl ? Texture(m_impl->rhiTexture) : Texture{};
}

Sampler CascadedShadowMap::GetSampler() const {
    return m_impl ? Sampler(m_impl->rhiSampler) : Sampler{};
}

} // namespace Sunshine
