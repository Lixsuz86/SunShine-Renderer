#pragma once

#include "types.hpp"
#include "frame_context.hpp"
#include "swapchain_context.hpp"
#include "buffer.hpp"
#include "pipeline.hpp"
#include "texture.hpp"
#include "rhi/rhi_device.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <span>

namespace Sunshine {

/**
 * @brief Configuration descriptor for initializing a Sunshine graphics Device.
 */
struct DeviceDesc {
    bool enableValidation{true};            ///< Enable API validation and diagnostic messages in debug mode.
    const char* applicationName{"Sunshine Application"}; ///< Name of the application passed to the underlying driver.
};

namespace Internal {
    struct DeviceImpl;
}

class ShadowMap;

/**
 * @brief Core Sunshine graphics device representing the physical/logical GPU.
 * 
 * Manages GPU initialization, resource allocation (buffers, textures, pipelines),
 * synchronization objects, and frame lifecycle orchestration.
 */
class Device {
public:
    /**
     * @brief Initializes a new graphics Device with the given configuration.
     * @param desc Configuration options including validation and application name.
     */
    explicit Device(const DeviceDesc& desc = {});
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;

    /**
     * @brief Begins a new rendering frame on the specified swapchain.
     * @param swapchain Target SwapchainContext for presentation.
     * @return FrameContext if the frame was successfully acquired, or std::nullopt if skipped (e.g. minimized window).
     */
    std::optional<FrameContext> BeginFrame(SwapchainContext& swapchain);

    /**
     * @brief Submits the recorded frame commands to the GPU queue and presents to the swapchain.
     * @param frame Active FrameContext acquired via BeginFrame.
     */
    void SubmitFrame(FrameContext& frame);

    /**
     * @brief Blocks host CPU execution until all pending GPU operations have completed.
     */
    void WaitIdle();

    /**
     * @brief Returns the human-readable brand name of the active GPU.
     */
    std::string_view GetDeviceName() const noexcept;

    // --- Resource Creation ---

    /**
     * @brief Creates a GPU vertex buffer with the specified byte size.
     */
    VertexBuffer CreateVertexBuffer(size_t size, const void* initialData = nullptr);

    template<typename T>
    VertexBuffer CreateVertexBuffer(std::span<const T> vertices) {
        return CreateVertexBuffer(vertices.size_bytes(), vertices.data());
    }

    template<typename T>
    VertexBuffer CreateVertexBuffer(const std::vector<T>& vertices) {
        return CreateVertexBuffer(std::span<const T>(vertices));
    }

    /**
     * @brief Creates a GPU index buffer for indexed drawing.
     */
    IndexBuffer CreateIndexBuffer(size_t size, const void* initialData = nullptr, RHI::IndexType indexType = RHI::IndexType::Uint32);

    template<typename T>
    IndexBuffer CreateIndexBuffer(std::span<const T> indices) {
        RHI::IndexType type = (sizeof(T) == 2) ? RHI::IndexType::Uint16 : RHI::IndexType::Uint32;
        return CreateIndexBuffer(indices.size_bytes(), indices.data(), type);
    }

    template<typename T>
    IndexBuffer CreateIndexBuffer(const std::vector<T>& indices) {
        return CreateIndexBuffer(std::span<const T>(indices));
    }

    /**
     * @brief Creates a typed uniform buffer for passing matrices and constants to shaders.
     */
    template<typename T>
    UniformBuffer<T> CreateUniformBuffer(const T* initialData = nullptr) {
        RHI::BufferDesc desc{};
        desc.size = sizeof(T);
        desc.usage = RHI::BufferUsage::Uniform;
        desc.memoryType = RHI::BufferMemoryType::CPUVisible;
        desc.debugName = "UniformBuffer";
        auto rhiBuf = GetRHIDevice().CreateBuffer(desc, initialData);
        return UniformBuffer<T>(rhiBuf);
    }

    /**
     * @brief Creates a 2D GPU texture from raw pixel data.
     */
    Texture CreateTexture2D(uint32_t width, uint32_t height, const void* pixels, RHI::TextureFormat format = RHI::TextureFormat::RGBA8_UNORM);

    /**
     * @brief Loads an image from disk (PNG, JPG, BMP) and uploads it directly to GPU memory.
     */
    Texture LoadTexture2D(const std::string& filepath);

    /**
     * @brief Creates a texture sampler for filtering and addressing modes.
     */
    Sampler CreateSampler(const RHI::SamplerDesc& desc = {});

    /**
     * @brief Creates a shader module from SPIR-V bytecode.
     */
    std::shared_ptr<RHI::IRHIShader> CreateShader(const RHI::ShaderDesc& desc);

    /**
     * @brief Creates and caches a graphics pipeline matching the provided state descriptor.
     */
    Pipeline CreatePipeline(const RHI::PipelineDesc& desc);

    /**
     * @brief Creates and caches a graphics pipeline compatible with the specified ShadowMap's depth pass.
     */
    Pipeline CreatePipeline(const RHI::PipelineDesc& desc, const ShadowMap& shadowMap);

    /**
     * @brief Returns the underlying low-level RHI device interface.
     */
    RHI::IRHIDevice& GetRHIDevice() noexcept;

private:
    friend class SwapchainContext;
    friend class ShadowMap;
    Internal::DeviceImpl* GetImpl() const noexcept { return m_impl.get(); }

    std::unique_ptr<Internal::DeviceImpl> m_impl;
};

} // namespace Sunshine
