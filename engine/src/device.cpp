#include "sunshine/device.hpp"
#include "sunshine/shadow_map.hpp"
#include "sunshine/error.hpp"
#include "internal_types.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

namespace Sunshine {

namespace Internal {

DeviceImpl::DeviceImpl(bool enableValidation) {
    context = std::make_unique<Vulkan::VulkanContext>(enableValidation);

    // Initialize GLFW if not already initialized
    if (!glfwInit()) {
        throw GraphicsException("Failed to initialize GLFW before initializing Graphics Device!");
    }

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions;
    if (glfwExts && glfwExtCount > 0) {
        extensions.assign(glfwExts, glfwExts + glfwExtCount);
    }

    context->InitializeInstance(extensions);
    rhiDevice = std::make_unique<Vulkan::VulkanRHIDevice>(*context);
}

DeviceImpl::~DeviceImpl() {
    if (context && context->GetDevice() != VK_NULL_HANDLE) {
        context->WaitIdle();
        VkDevice device = context->GetDevice();

        rhiDevice.reset();

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (i < static_cast<int>(imageAvailableSemaphores.size()) && imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            }
            if (i < static_cast<int>(renderFinishedSemaphores.size()) && renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            }
            if (i < static_cast<int>(inFlightFences.size()) && inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device, inFlightFences[i], nullptr);
            }
        }

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
    }
}

void DeviceImpl::CreateSyncObjects() {
    VkDevice device = context->GetDevice();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->GetQueueFamilies().graphicsFamily.value();

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()));

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    commandContexts.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]));
        commandContexts[i] = std::make_unique<CommandContextImpl>();
    }
}

} // namespace Internal

Device::Device(const DeviceDesc& desc) {
    m_impl = std::make_unique<Internal::DeviceImpl>(desc.enableValidation);
}

Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

std::string_view Device::GetDeviceName() const noexcept {
    if (m_impl && m_impl->context) {
        return m_impl->context->GetDeviceProperties().deviceName;
    }
    return "Unknown Device";
}

void Device::WaitIdle() {
    if (m_impl && m_impl->context) {
        m_impl->context->WaitIdle();
    }
}

RHI::IRHIDevice& Device::GetRHIDevice() noexcept {
    return *m_impl->rhiDevice;
}

VertexBuffer Device::CreateVertexBuffer(size_t size, const void* initialData) {
    RHI::BufferDesc desc{};
    desc.size = size;
    desc.usage = RHI::BufferUsage::Vertex;
    desc.memoryType = RHI::BufferMemoryType::CPUVisible;
    desc.debugName = "VertexBuffer";

    auto rhiBuf = m_impl->rhiDevice->CreateBuffer(desc, initialData);
    return VertexBuffer(rhiBuf);
}

IndexBuffer Device::CreateIndexBuffer(size_t size, const void* initialData, RHI::IndexType indexType) {
    RHI::BufferDesc desc{};
    desc.size = size;
    desc.usage = RHI::BufferUsage::Index;
    desc.memoryType = RHI::BufferMemoryType::CPUVisible;
    desc.debugName = "IndexBuffer";

    auto rhiBuf = m_impl->rhiDevice->CreateBuffer(desc, initialData);
    return IndexBuffer(rhiBuf, indexType);
}

Texture Device::CreateTexture2D(uint32_t width, uint32_t height, const void* pixels, RHI::TextureFormat format) {
    RHI::TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.debugName = "Texture2D";

    auto rhiTex = m_impl->rhiDevice->CreateTexture(desc, pixels);
    return Texture(rhiTex);
}

Texture Device::LoadTexture2D(const std::string& filepath) {
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw GraphicsException("Failed to load texture image from file: " + filepath);
    }

    Texture tex = CreateTexture2D(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels, RHI::TextureFormat::RGBA8_UNORM);
    stbi_image_free(pixels);
    return tex;
}

Sampler Device::CreateSampler(const RHI::SamplerDesc& desc) {
    auto rhiSampler = m_impl->rhiDevice->CreateSampler(desc);
    return Sampler(rhiSampler);
}

std::shared_ptr<RHI::IRHIShader> Device::CreateShader(const RHI::ShaderDesc& desc) {
    return m_impl->rhiDevice->CreateShader(desc);
}

Pipeline Device::CreatePipeline(const RHI::PipelineDesc& desc) {
    auto rhiPipe = m_impl->rhiDevice->CreatePipeline(desc);
    return Pipeline(rhiPipe);
}

Pipeline Device::CreatePipeline(const RHI::PipelineDesc& desc, const ShadowMap& shadowMap) {
    auto* smImpl = shadowMap.GetImpl();
    if (!smImpl) {
        throw GraphicsException("Cannot create shadow pipeline with invalid ShadowMap!");
    }
    auto rhiPipe = m_impl->rhiDevice->CreatePipeline(desc, smImpl->renderPass);
    return Pipeline(rhiPipe);
}

std::optional<FrameContext> Device::BeginFrame(SwapchainContext& swapchain) {
    auto* swapchainImpl = swapchain.GetImpl();
    if (!swapchainImpl || !swapchainImpl->swapchain) {
        return std::nullopt;
    }

    int width = 0, height = 0;
    glfwGetFramebufferSize(swapchainImpl->window, &width, &height);
    if (width == 0 || height == 0) {
        // Window is minimized; skip frame without error
        return std::nullopt;
    }

    if (swapchainImpl->needsResize) {
        swapchainImpl->Recreate();
    }

    m_impl->activeSwapchain = swapchainImpl;
    m_impl->rhiDevice->SetCompatibleRenderPass(swapchainImpl->renderPass);

    VkDevice device = m_impl->context->GetDevice();
    uint32_t frameIdx = m_impl->currentFrame;

    // Wait for the in-flight fence of this frame slot
    VK_CHECK(vkWaitForFences(device, 1, &m_impl->inFlightFences[frameIdx], VK_TRUE, UINT64_MAX));

    uint32_t imageIndex = 0;
    VkResult acquireRes = swapchainImpl->swapchain->AcquireNextImage(
        m_impl->imageAvailableSemaphores[frameIdx], &imageIndex);

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainImpl->Recreate();
        return std::nullopt;
    } else if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        throw GraphicsException("Failed to acquire next swapchain image!");
    }

    VK_CHECK(vkResetFences(device, 1, &m_impl->inFlightFences[frameIdx]));
    VK_CHECK(vkResetCommandBuffer(m_impl->commandBuffers[frameIdx], 0));

    auto* cmdImpl = m_impl->commandContexts[frameIdx].get();
    cmdImpl->Begin(
        m_impl->commandBuffers[frameIdx],
        swapchainImpl->renderPass,
        swapchainImpl->framebuffers[imageIndex],
        swapchainImpl->swapchain->GetExtent(),
        frameIdx
    );

    CommandContext cmdContext(cmdImpl);
    return FrameContext(std::move(cmdContext), frameIdx, imageIndex);
}

void Device::SubmitFrame(FrameContext& frame) {
    auto* swapchainImpl = m_impl->activeSwapchain;
    if (!swapchainImpl) {
        throw GraphicsException("Attempted to submit frame without an active swapchain!");
    }

    uint32_t frameIdx = frame.GetFrameIndex();
    uint32_t imageIndex = frame.GetImageIndex();
    auto* cmdImpl = m_impl->commandContexts[frameIdx].get();
    cmdImpl->End();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_impl->imageAvailableSemaphores[frameIdx] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_impl->commandBuffers[frameIdx];

    VkSemaphore signalSemaphores[] = { m_impl->renderFinishedSemaphores[frameIdx] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_impl->context->GetGraphicsQueue(), 1, &submitInfo, m_impl->inFlightFences[frameIdx]));

    VkResult presentRes = swapchainImpl->swapchain->Present(
        m_impl->context->GetPresentQueue(),
        m_impl->renderFinishedSemaphores[frameIdx],
        imageIndex
    );

    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        swapchainImpl->needsResize = true;
    } else if (presentRes != VK_SUCCESS) {
        throw GraphicsException("Failed to present rendered swapchain frame!");
    }

    m_impl->currentFrame = (m_impl->currentFrame + 1) % Internal::MAX_FRAMES_IN_FLIGHT;
}

} // namespace Sunshine
