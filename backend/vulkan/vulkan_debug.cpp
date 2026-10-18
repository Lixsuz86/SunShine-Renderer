#include "vulkan_debug.hpp"
#include <cstring>
#include <iostream>

namespace Sunshine::Vulkan {

bool VulkanDebug::CheckValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layerProps : availableLayers) {
        if (std::strcmp(VALIDATION_LAYER_NAME, layerProps.layerName) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<const char*> VulkanDebug::GetRequiredLayers() {
    std::vector<const char*> layers;
    if (CheckValidationLayerSupport()) {
        layers.push_back(VALIDATION_LAYER_NAME);
    } else {
        std::cout << "[Sunshine Vulkan Info] Validation layer '" << VALIDATION_LAYER_NAME 
                  << "' is not installed on this machine. Running in portable mode without validation overhead.\n"
                  << "                       (To enable validation, install the LunarG Vulkan SDK: https://vulkan.lunarg.com/)\n";
    }
    return layers;
}

void VulkanDebug::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;
}

VkResult VulkanDebug::CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
    if (vkCreateDebugUtilsMessengerEXT) {
        return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanDebug::DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) 
{
    if (vkDestroyDebugUtilsMessengerEXT && debugMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebug::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) 
{
    const char* typeStr = "[GENERAL]";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        typeStr = "[VALIDATION]";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        typeStr = "[PERFORMANCE]";
    }

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << "\033[1;31m[Vulkan Validation ERROR] " << typeStr << " " 
                  << pCallbackData->pMessage << "\033[0m\n";
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "\033[1;33m[Vulkan Validation WARN]  " << typeStr << " " 
                  << pCallbackData->pMessage << "\033[0m\n";
    }

    return VK_FALSE;
}

} // namespace Sunshine::Vulkan
