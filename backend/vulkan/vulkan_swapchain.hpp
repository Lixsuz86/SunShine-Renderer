#pragma once

#include "vulkan_common.hpp"
#include "vulkan_context.hpp"
#include <vector>

namespace Sunshine::Vulkan {

class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanContext& context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    void Recreate(uint32_t width, uint32_t height);
    void Cleanup();

    VkSwapchainKHR GetHandle() const { return m_swapchain; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkFormat GetDepthFormat() const { return m_depthFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    const std::vector<VkImage>& GetImages() const { return m_images; }
    VkImageView GetDepthImageView() const { return m_depthImageView; }

    VkResult AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex, uint64_t timeout = UINT64_MAX);
    VkResult Present(VkQueue presentQueue, VkSemaphore renderFinishedSemaphore, uint32_t imageIndex);

private:
    void Create(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void CreateDepthResources();
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext& m_context;
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_imageFormat{VK_FORMAT_UNDEFINED};
    VkColorSpaceKHR m_colorSpace{};
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    // Depth Resources
    VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
    VkImage m_depthImage{VK_NULL_HANDLE};
    VkDeviceMemory m_depthImageMemory{VK_NULL_HANDLE};
    VkImageView m_depthImageView{VK_NULL_HANDLE};
};

} // namespace Sunshine::Vulkan
