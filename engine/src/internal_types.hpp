#pragma once

#include "backend/vulkan/vulkan_common.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "backend/vulkan/vulkan_swapchain.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_device.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_command_list.hpp"
#include "backend/vulkan/rhi/vulkan_rhi_compute_pipeline.hpp"
#include "sunshine/buffer.hpp"
#include "sunshine/pipeline.hpp"
#include "sunshine/compute_pipeline.hpp"
#include "sunshine/secondary_command_context.hpp"
#include "sunshine/thread_pool.hpp"
#include "sunshine/render_target.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>

namespace Sunshine::Internal {

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct SecondaryCommandContextImpl {
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    Vulkan::VulkanRHICommandList rhiCommandList;
    uint32_t frameIndex{0};

    void Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, uint32_t frameIdx, VkExtent2D ext);
    void End();
};

struct CommandContextImpl {
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkExtent2D extent{0, 0};
    uint32_t frameIndex{0};
    struct DeviceImpl* deviceImpl{nullptr};
    bool passActive{false};
    bool isSecondaryPass{false};
    bool isRecording{false};

    Vulkan::VulkanRHICommandList rhiCommandList;
    Vulkan::VulkanRHIComputePipeline* currentComputePipeline{nullptr};

    void Begin(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, VkExtent2D ext, uint32_t frameIndex, struct DeviceImpl* dev = nullptr);
    void Clear(const Color& color);
    void BeginShadowPass(struct ShadowMapImpl& shadowMap);
    void EndShadowPass();
    void BeginPass(struct RenderTargetImpl& target, const Color& clearColor);
    void BeginSecondaryPass(const Color& clearColor);
    void EndPass();
    void EnsurePassActive();

    // Compute
    void SetComputePipeline(const ComputePipeline& pipeline);
    void SetStorageBufferRaw(uint32_t binding, std::shared_ptr<RHI::IRHIBuffer> buffer, size_t size, size_t offset);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void ComputeBarrier();

    // Graphics
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

    // Secondary & Parallel Recording
    void RecordParallel(WorkerThreadPool& pool, size_t batchCount, std::function<void(size_t batchIndex, size_t threadId, SecondaryCommandContext& secCmd)> recordFunc);
    void ExecuteSecondaryCommands(std::span<const SecondaryCommandContext> secondaryContexts);

    void End();
};

struct ShadowMapImpl {
    Vulkan::VulkanContext& context;
    Vulkan::VulkanRHIDevice& rhiDevice;
    uint32_t width{2048};
    uint32_t height{2048};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    std::shared_ptr<RHI::IRHITexture> rhiTexture;
    std::shared_ptr<RHI::IRHISampler> rhiSampler;

    ShadowMapImpl(Vulkan::VulkanContext& ctx, Vulkan::VulkanRHIDevice& rhiDev, uint32_t w, uint32_t h);
    ~ShadowMapImpl();

    void CreateRenderPass();
    void CreateFramebuffer();
};

struct RenderTargetImpl {
    Vulkan::VulkanContext& context;
    Vulkan::VulkanRHIDevice& rhiDevice;
    RenderTargetDesc desc;
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    std::shared_ptr<RHI::IRHITexture> colorTexture;
    std::shared_ptr<RHI::IRHITexture> depthTexture;
    std::shared_ptr<RHI::IRHISampler> rhiSampler;

    RenderTargetImpl(Vulkan::VulkanContext& ctx, Vulkan::VulkanRHIDevice& rhiDev, const RenderTargetDesc& d);
    ~RenderTargetImpl();

    void CreateRenderPass();
    void CreateFramebuffer();
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

    std::mutex poolMutex;
    std::unordered_map<uint32_t, VkCommandPool> threadCommandPools;

    explicit DeviceImpl(bool enableValidation);
    ~DeviceImpl();

    void CreateSyncObjects();
    VkCommandPool GetThreadCommandPool(uint32_t threadId, uint32_t frameIndex);
};

} // namespace Sunshine::Internal
