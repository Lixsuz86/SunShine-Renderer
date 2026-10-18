#include "sunshine/swapchain_context.hpp"
#include "sunshine/device.hpp"
#include "sunshine/error.hpp"
#include "internal_types.hpp"
#include <array>

namespace Sunshine {

namespace Internal {

SwapchainContextImpl::SwapchainContextImpl(Vulkan::VulkanContext& ctx, GLFWwindow* win)
    : context(ctx), window(win) 
{
    if (!window) {
        throw GraphicsException("GLFWwindow handle passed to SwapchainContext is null!");
    }

    // Create Surface
    VK_CHECK(glfwCreateWindowSurface(context.GetInstance(), window, nullptr, &surface));

    // Ensure physical and logical devices exist
    if (context.GetPhysicalDevice() == VK_NULL_HANDLE) {
        context.PickPhysicalDevice(surface);
        context.CreateLogicalDevice(surface);
    }

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    swapchain = std::make_unique<Vulkan::VulkanSwapchain>(
        context, surface, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    CreateRenderPass();
    CreateFramebuffers();
}

SwapchainContextImpl::~SwapchainContextImpl() {
    context.WaitIdle();
    DestroyFramebuffers();

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.GetDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    swapchain.reset();

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context.GetInstance(), surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

void SwapchainContextImpl::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain->GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = swapchain->GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(context.GetDevice(), &renderPassInfo, nullptr, &renderPass));
}

void SwapchainContextImpl::CreateFramebuffers() {
    const auto& imageViews = swapchain->GetImageViews();
    framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = { 
            imageViews[i], 
            swapchain->GetDepthImageView() 
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain->GetExtent().width;
        framebufferInfo.height = swapchain->GetExtent().height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(context.GetDevice(), &framebufferInfo, nullptr, &framebuffers[i]));
    }
}

void SwapchainContextImpl::DestroyFramebuffers() {
    VkDevice device = context.GetDevice();
    for (auto fb : framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    framebuffers.clear();
}

void SwapchainContextImpl::Recreate() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    if (width == 0 || height == 0) {
        // Window is currently minimized
        return;
    }

    context.WaitIdle();
    DestroyFramebuffers();

    swapchain->Recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    CreateFramebuffers();
    needsResize = false;
}

} // namespace Internal

SwapchainContext::SwapchainContext(Device& device, GLFWwindow* window) {
    auto* devImpl = device.GetImpl();
    if (!devImpl || !devImpl->context) {
        throw GraphicsException("Cannot create SwapchainContext with uninitialized Device!");
    }
    m_impl = std::make_unique<Internal::SwapchainContextImpl>(*devImpl->context, window);

    // If device sync objects haven't been created yet, create them now that logical device is ready
    if (devImpl->commandPool == VK_NULL_HANDLE) {
        devImpl->CreateSyncObjects();
    }

    if (devImpl->rhiDevice) {
        devImpl->rhiDevice->SetCompatibleRenderPass(m_impl->renderPass);
    }
}

SwapchainContext::~SwapchainContext() = default;

SwapchainContext::SwapchainContext(SwapchainContext&&) noexcept = default;
SwapchainContext& SwapchainContext::operator=(SwapchainContext&&) noexcept = default;

Extent2D SwapchainContext::GetExtent() const noexcept {
    if (!m_impl || !m_impl->swapchain) {
        return { 0, 0 };
    }
    auto vkExt = m_impl->swapchain->GetExtent();
    return { vkExt.width, vkExt.height };
}

void SwapchainContext::RequestResize() {
    if (m_impl) {
        m_impl->needsResize = true;
    }
}

} // namespace Sunshine
