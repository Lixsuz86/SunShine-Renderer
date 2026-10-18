#pragma once

#include <volk.h>
#include <string_view>
#include <stdexcept>
#include <iostream>
#include <source_location>
#include "sunshine/error.hpp"

namespace Sunshine::Vulkan {

inline std::string_view VkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS: Operation successfully completed";
        case VK_NOT_READY: return "VK_NOT_READY: Fence or query has not yet completed";
        case VK_TIMEOUT: return "VK_TIMEOUT: Wait operation has not completed in specified time";
        case VK_EVENT_SET: return "VK_EVENT_SET: Event is signaled";
        case VK_EVENT_RESET: return "VK_EVENT_RESET: Event is unsignaled";
        case VK_INCOMPLETE: return "VK_INCOMPLETE: Return array was too small for the result";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY: Host memory allocation failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY: Device memory allocation failed";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED: Initialization failed";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST: Logical or physical device lost";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED: Mapping memory object failed";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT: Requested layer not present or could not be loaded";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT: Requested extension not present";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT: Requested feature not present";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER: Vulkan driver is incompatible";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS: Too many objects created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED: Format not supported on device";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL: Memory pool allocation failed due to fragmentation";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR: Presentation surface is no longer available";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: Native window already in use";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR: Swapchain does not match surface properties exactly but can still present";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR: Swapchain has become out of date and must be recreated";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: Display used by swapchain is incompatible";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT: Validation layer detected an error";
        default: return "Unknown Vulkan Result Code";
    }
}

inline std::string_view VkResultToDiagnosis(VkResult result) {
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "System RAM is exhausted. Check for host memory leaks or excessive buffer allocations.";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "Video RAM (VRAM) is exhausted on this GPU. Reduce texture resolutions, vertex density, or free unused GPU buffers.";
        case VK_ERROR_DEVICE_LOST:
            return "GPU device was lost or reset by the operating system (TDR). Often caused by an infinite loop in a shader or illegal memory access.";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Vulkan failed to initialize. Verify that GPU drivers support Vulkan 1.3+ and the target display is active.";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "Requested Vulkan validation layer was not found. Install LunarG Vulkan SDK or disable validation in release builds.";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested Vulkan extension is not supported by the hardware or driver.";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "The swapchain has become incompatible with the window (usually due to resize). The swapchain must be recreated.";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "The presentation surface is no longer available. Window handle may have been closed or invalidated.";
        default:
            return "Check Vulkan driver logs and validation layers for further diagnostic information.";
    }
}

inline void CheckVkResult(VkResult result, 
                          const char* expr, 
                          const std::source_location location = std::source_location::current()) {
    if (result != VK_SUCCESS) {
        std::string detail = std::string("Vulkan call `") + expr + "` failed: " + std::string(VkResultToString(result));
        std::string diagnosis = std::string(VkResultToDiagnosis(result));

        std::cerr << "[Sunshine Vulkan Fatal] (" << location.file_name() << ":" 
                  << location.line() << " in " << location.function_name() << ")\n"
                  << "  Expression: " << expr << "\n"
                  << "  Result:     " << VkResultToString(result) << " (" << static_cast<int>(result) << ")\n"
                  << "  Diagnosis:  " << diagnosis << "\n";

        throw ::Sunshine::GraphicsException(detail, diagnosis, location);
    }
}

} // namespace Sunshine::Vulkan

#define VK_CHECK(expr) ::Sunshine::Vulkan::CheckVkResult((expr), #expr)
