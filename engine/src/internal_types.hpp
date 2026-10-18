#pragma once

#include "backend/vulkan/vulkan_common.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "backend/vulkan/vulkan_swapchain.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_device.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_command_list.hpp"
#include "sunshine/buffer.hpp"
#include "sunshine/pipeline.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vector>
#include <memory>
#include <string>

namespace Sunshine::Internal {

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct CommandContextImpl {
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkExtent2D extent{0, 0};
    bool passActive{false};
    bool isRecording{false};

    Vulkan::VulkanRHICommandList rhiCommandList;

    void Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, VkExtent2D ext, uint32_t frameIndex);
    void Clear(const Color& color);
    void EnsurePassActive();
    void SetPipeline(const Pipeline& pipeline);
    void SetVertexBuffer(uint32_t binding, const VertexBuffer& vertexBuffer, uint64_t offset);
    void SetIndexBuffer(const IndexBuffer& indexBuffer, uint64_t offset);
    void SetUniformBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset);
    void SetTexture(uint32_t binding, const Texture& texture, const Sampler& sampler);
    void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
    void Draw(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, uint32_t vertexCount);
    void DrawIndexed(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, const IndexBuffer& indexBuffer, uint32_t indexCount);
    void End();
};

struct SwapchainContextImpl {
    Vulkan::VulkanContext& context;
    GLFWwindow* window{nullptr};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    std::unique_ptr<Vulkan::VulkanSwapchain> swapchain;
    VkRenderPass renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers;
    bool needsResize{false};

    SwapchainContextImpl(Vulkan::VulkanContext& ctx, GLFWwindow* win);
    ~SwapchainContextImpl();

    void CreateRenderPass();
    void CreateFramebuffers();
    void DestroyFramebuffers();
    void Recreate();
};

struct DeviceImpl {
    std::unique_ptr<Vulkan::VulkanContext> context;
    std::unique_ptr<Vulkan::VulkanRHIDevice> rhiDevice;
    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<std::unique_ptr<CommandContextImpl>> commandContexts;

    uint32_t currentFrame{0};
    SwapchainContextImpl* activeSwapchain{nullptr};

    explicit DeviceImpl(bool enableValidation);
    ~DeviceImpl();

    void CreateSyncObjects();
};

} // namespace Sunshine::Internal
