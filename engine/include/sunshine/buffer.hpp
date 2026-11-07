#pragma once

#include "rhi/rhi_buffer.hpp"
#include <memory>

namespace Sunshine {

class Device;

class VertexBuffer {
public:
    VertexBuffer() = default;
    explicit VertexBuffer(std::shared_ptr<RHI::IRHIBuffer> rhiBuffer)
        : m_buffer(std::move(rhiBuffer)) {}

    void Update(const void* data, size_t size, size_t offset = 0) {
        if (m_buffer) {
            m_buffer->UpdateData(data, size, offset);
        }
    }

    template<typename T>
    void Update(std::span<const T> data, size_t offset = 0) {
        Update(data.data(), data.size_bytes(), offset);
    }

    template<typename T>
    void Update(const std::vector<T>& data, size_t offset = 0) {
        Update(data.data(), data.size() * sizeof(T), offset);
    }

    size_t GetSize() const noexcept {
        return m_buffer ? m_buffer->GetSize() : 0;
    }

    std::shared_ptr<RHI::IRHIBuffer> GetRHIBuffer() const noexcept {
        return m_buffer;
    }

private:
    std::shared_ptr<RHI::IRHIBuffer> m_buffer;
};

class IndexBuffer {
public:
    IndexBuffer() = default;
    explicit IndexBuffer(std::shared_ptr<RHI::IRHIBuffer> rhiBuffer, RHI::IndexType indexType = RHI::IndexType::Uint32)
        : m_buffer(std::move(rhiBuffer)), m_indexType(indexType) {}

    void Update(const void* data, size_t size, size_t offset = 0) {
        if (m_buffer) {
            m_buffer->UpdateData(data, size, offset);
        }
    }

    size_t GetSize() const noexcept {
        return m_buffer ? m_buffer->GetSize() : 0;
    }

    RHI::IndexType GetIndexType() const noexcept {
        return m_indexType;
    }

    std::shared_ptr<RHI::IRHIBuffer> GetRHIBuffer() const noexcept {
        return m_buffer;
    }

private:
    std::shared_ptr<RHI::IRHIBuffer> m_buffer;
    RHI::IndexType m_indexType{RHI::IndexType::Uint32};
};

template <typename T>
class UniformBuffer {
public:
    UniformBuffer() = default;
    explicit UniformBuffer(std::shared_ptr<RHI::IRHIBuffer> rhiBuffer)
        : m_buffer(std::move(rhiBuffer)) {}

    void Update(const T& data) {
        if (m_buffer) {
            m_buffer->UpdateData(&data, sizeof(T), 0);
        }
    }

    size_t GetSize() const noexcept {
        return sizeof(T);
    }

    std::shared_ptr<RHI::IRHIBuffer> GetRHIBuffer() const noexcept {
        return m_buffer;
    }

private:
    std::shared_ptr<RHI::IRHIBuffer> m_buffer;
};

template <typename T = void>
class StorageBuffer {
public:
    StorageBuffer() = default;
    explicit StorageBuffer(std::shared_ptr<RHI::IRHIBuffer> rhiBuffer, size_t size = 0)
        : m_buffer(std::move(rhiBuffer)), m_size(size ? size : (m_buffer ? m_buffer->GetSize() : 0)) {}

    void Update(const void* data, size_t size, size_t offset = 0) {
        if (m_buffer) {
            m_buffer->UpdateData(data, size, offset);
        }
    }

    template<typename U>
    void Update(const std::vector<U>& data, size_t offset = 0) {
        Update(data.data(), data.size() * sizeof(U), offset);
    }

    template<typename U>
    void Update(std::span<const U> data, size_t offset = 0) {
        Update(data.data(), data.size_bytes(), offset);
    }

    size_t GetSize() const noexcept {
        return m_buffer ? m_buffer->GetSize() : m_size;
    }

    std::shared_ptr<RHI::IRHIBuffer> GetRHIBuffer() const noexcept {
        return m_buffer;
    }

    // Helper conversion to VertexBuffer so compute storage buffer can be drawn directly
    VertexBuffer AsVertexBuffer() const {
        return VertexBuffer(m_buffer);
    }

private:
    std::shared_ptr<RHI::IRHIBuffer> m_buffer;
    size_t m_size{0};
};

} // namespace Sunshine
