#pragma once

#include <chrono>
#include <string>
#include <cstdint>
#include <cstdio>

namespace Sunshine {

/**
 * @brief Telemetry metrics collected during frame execution.
 */
struct ProfilerStats {
    float fps{0.0f};
    float frameTimeMs{0.0f};
    float avgFrameTimeMs{0.0f};
    float minFrameTimeMs{0.0f};
    float maxFrameTimeMs{0.0f};
    uint64_t totalFrames{0};
    uint32_t drawCalls{0};
    uint32_t instanceCount{0};
    uint32_t triangleCount{0};
};

/**
 * @brief High-precision profiler for measuring engine overhead, framerates, and stress metrics.
 */
class Profiler {
public:
    Profiler() 
        : m_startTime(std::chrono::high_resolution_clock::now()),
          m_lastFrameTime(m_startTime),
          m_frameStartTime(m_startTime) 
    {}

    /**
     * @brief Call at the very beginning of the render loop iteration.
     */
    void BeginFrame() {
        m_frameStartTime = std::chrono::high_resolution_clock::now();
        m_currentDrawCalls = 0;
        m_currentInstanceCount = 0;
        m_currentTriangleCount = 0;
    }

    /**
     * @brief Record a draw call with triangle and instance counts.
     */
    void RecordDraw(uint32_t triangleCount, uint32_t instanceCount = 1) {
        m_currentDrawCalls++;
        m_currentInstanceCount += instanceCount;
        m_currentTriangleCount += triangleCount * instanceCount;
    }

    /**
     * @brief Call after frame presentation has been submitted.
     */
    void EndFrame() {
        auto now = std::chrono::high_resolution_clock::now();
        float deltaSec = std::chrono::duration<float, std::chrono::seconds::period>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;

        m_stats.frameTimeMs = std::chrono::duration<float, std::chrono::milliseconds::period>(now - m_frameStartTime).count();
        m_stats.totalFrames++;
        m_stats.drawCalls = m_currentDrawCalls;
        m_stats.instanceCount = m_currentInstanceCount;
        m_stats.triangleCount = m_currentTriangleCount;

        m_accumulatedTime += deltaSec;
        m_frameCountInInterval++;

        if (m_stats.minFrameTimeMs == 0.0f || m_stats.frameTimeMs < m_stats.minFrameTimeMs) {
            m_stats.minFrameTimeMs = m_stats.frameTimeMs;
        }
        if (m_stats.frameTimeMs > m_stats.maxFrameTimeMs) {
            m_stats.maxFrameTimeMs = m_stats.frameTimeMs;
        }

        // Update averaged stats every 200ms for stable display
        if (m_accumulatedTime >= 0.20f) {
            m_stats.fps = static_cast<float>(m_frameCountInInterval) / m_accumulatedTime;
            m_stats.avgFrameTimeMs = (m_accumulatedTime * 1000.0f) / static_cast<float>(m_frameCountInInterval);
            m_accumulatedTime = 0.0f;
            m_frameCountInInterval = 0;
        }
    }

    [[nodiscard]] const ProfilerStats& GetStats() const noexcept {
        return m_stats;
    }

    [[nodiscard]] std::string GetFormattedTitle(const std::string& baseTitle) const {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s | FPS: %.0f (%.2f ms) | Draws: %u | Instances: %u | Tris: %u",
                 baseTitle.c_str(), 
                 m_stats.fps, 
                 m_stats.avgFrameTimeMs, 
                 m_stats.drawCalls, 
                 m_stats.instanceCount, 
                 m_stats.triangleCount);
        return std::string(buf);
    }

private:
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;

    float m_accumulatedTime{0.0f};
    uint32_t m_frameCountInInterval{0};

    uint32_t m_currentDrawCalls{0};
    uint32_t m_currentInstanceCount{0};
    uint32_t m_currentTriangleCount{0};

    ProfilerStats m_stats{};
};

} // namespace Sunshine
