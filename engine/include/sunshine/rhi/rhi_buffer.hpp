#pragma once

#include "rhi_types.hpp"

namespace Sunshine::RHI {

class IRHIBuffer {
public:
    virtual ~IRHIBuffer() = default;

    virtual size_t GetSize() const noexcept = 0;
    virtual BufferUsage GetUsage() const noexcept = 0;
    virtual void UpdateData(const void* data, size_t size, size_t offset = 0) = 0;
};

} // namespace Sunshine::RHI
