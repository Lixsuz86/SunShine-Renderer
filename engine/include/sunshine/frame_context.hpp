#pragma once

#include "types.hpp"
#include "command_context.hpp"
#include <utility>

namespace Sunshine {

class Device;

class FrameContext {
public:
    ~FrameContext() = default;
    FrameContext(const FrameContext&) = delete;
    FrameContext& operator=(const FrameContext&) = delete;
    FrameContext(FrameContext&&) noexcept = default;
    FrameContext& operator=(FrameContext&&) noexcept = default;

    void Clear(const Color& color) {
        m_commandContext.Clear(color);
    }

    void Draw(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, uint32_t vertexCount) {
        m_commandContext.Draw(pipeline, vertexBuffer, vertexCount);
    }

    void DrawIndexed(const Pipeline& pipeline, const VertexBuffer& vertexBuffer, const IndexBuffer& indexBuffer, uint32_t indexCount) {
        m_commandContext.DrawIndexed(pipeline, vertexBuffer, indexBuffer, indexCount);
    }

    CommandContext& GetCommandContext() noexcept {
        return m_commandContext;
    }

    uint32_t GetFrameIndex() const noexcept {
        return m_frameIndex;
    }

    uint32_t GetImageIndex() const noexcept {
        return m_imageIndex;
    }

private:
    friend class Device;
    FrameContext(CommandContext&& cmdContext, uint32_t frameIndex, uint32_t imageIndex)
        : m_commandContext(std::move(cmdContext)),
          m_frameIndex(frameIndex),
          m_imageIndex(imageIndex) {}

    CommandContext m_commandContext;
    uint32_t m_frameIndex{0};
    uint32_t m_imageIndex{0};
};

} // namespace Sunshine
