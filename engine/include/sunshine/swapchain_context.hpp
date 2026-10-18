#pragma once

#include "types.hpp"
#include <memory>

struct GLFWwindow;

namespace Sunshine {

class Device;

namespace Internal {
    struct SwapchainContextImpl;
}

class SwapchainContext {
public:
    SwapchainContext(Device& device, GLFWwindow* window);
    ~SwapchainContext();

    SwapchainContext(const SwapchainContext&) = delete;
    SwapchainContext& operator=(const SwapchainContext&) = delete;
    SwapchainContext(SwapchainContext&&) noexcept;
    SwapchainContext& operator=(SwapchainContext&&) noexcept;

    Extent2D GetExtent() const noexcept;
    void RequestResize();

private:
    friend class Device;
    Internal::SwapchainContextImpl* GetImpl() const noexcept { return m_impl.get(); }

    std::unique_ptr<Internal::SwapchainContextImpl> m_impl;
};

} // namespace Sunshine
