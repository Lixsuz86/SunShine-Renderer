#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include <string>
#include <filesystem>
#include <chrono>

namespace Sunshine {

class ShaderWatcher {
public:
    ShaderWatcher(Device& device, 
                  Pipeline& pipeline, 
                  std::string vertPath, 
                  std::string fragPath);

    ~ShaderWatcher() = default;

    // Checks filesystem for shader file changes. If changed, recompiles and swaps the pipeline.
    // Returns true if a reload successfully occurred.
    bool CheckAndReload();

    void SetPollIntervalSeconds(float seconds) noexcept {
        m_pollInterval = std::chrono::duration<float>(seconds);
    }

    const std::string& GetVertexPath() const noexcept { return m_vertPath; }
    const std::string& GetFragmentPath() const noexcept { return m_fragPath; }

private:
    static bool CompileGLSLToSPIRV(const std::filesystem::path& sourcePath, 
                                   const std::string& stageFlag, 
                                   std::vector<uint32_t>& outSpirv, 
                                   std::string& outErrorMessage);

    Device& m_device;
    Pipeline& m_pipeline;
    std::string m_vertPath;
    std::string m_fragPath;

    std::filesystem::file_time_type m_lastVertWriteTime{};
    std::filesystem::file_time_type m_lastFragWriteTime{};

    std::chrono::duration<float> m_pollInterval{0.25f}; // Check every 250ms
    std::chrono::steady_clock::time_point m_lastCheckTime{};
};

} // namespace Sunshine
