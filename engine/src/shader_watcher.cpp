#include "sunshine/shader_watcher.hpp"
#include "sunshine/error.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <array>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <array>
#endif

namespace Sunshine {

static std::filesystem::path FindGlslangCompiler() {
    const std::vector<std::filesystem::path> candidates = {
        "tools/glslang/bin/glslang.exe",
        "../tools/glslang/bin/glslang.exe",
        "../../tools/glslang/bin/glslang.exe",
        "tools/glslang/bin/glslangValidator.exe",
        "glslang.exe",
        "glslangValidator.exe"
    };

    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec)) {
            return std::filesystem::canonical(c, ec);
        }
    }

    return "glslang.exe";
}

bool ShaderWatcher::CompileGLSLToSPIRV(const std::filesystem::path& sourcePath, 
                                       const std::string& stageFlag, 
                                       std::vector<uint32_t>& outSpirv, 
                                       std::string& outErrorMessage) 
{
    auto compiler = FindGlslangCompiler();

    auto tempSpv = std::filesystem::temp_directory_path() / 
        ("sunshine_hotreload_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + ".spv");

    std::string cmd = "\"" + compiler.string() + "\" -V -S " + stageFlag + " \"" + sourcePath.string() + "\" -o \"" + tempSpv.string() + "\"";

    std::string output;
    int exitCode = -1;

#if defined(_WIN32)
    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        outErrorMessage = "Failed to create pipe for glslang compiler";
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmdBuffer = cmd;

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        outErrorMessage = "Failed to launch glslang compiler process: " + compiler.string();
        return false;
    }

    CloseHandle(hWritePipe);

    char buffer[512];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD processExit = 0;
    GetExitCodeProcess(pi.hProcess, &processExit);
    exitCode = static_cast<int>(processExit);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
#else
    std::string posixCmd = cmd + " 2>&1";
    FILE* pipe = popen(posixCmd.c_str(), "r");
    if (!pipe) {
        outErrorMessage = "Failed to launch glslang compiler process: " + compiler.string();
        return false;
    }

    std::array<char, 256> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    exitCode = pclose(pipe);
#endif

    if (exitCode != 0) {
        outErrorMessage = output;
        std::error_code ec;
        std::filesystem::remove(tempSpv, ec);
        return false;
    }

    // Read the compiled SPIR-V bytecode
    std::ifstream file(tempSpv, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        outErrorMessage = "Failed to open compiled SPIR-V output file: " + tempSpv.string();
        std::error_code ec;
        std::filesystem::remove(tempSpv, ec);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    outSpirv.resize(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(outSpirv.data()), fileSize);
    file.close();

    std::error_code ec;
    std::filesystem::remove(tempSpv, ec);
    return true;
}

static std::filesystem::path ResolveShaderPath(const std::filesystem::path& p) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec)) {
        return p;
    }
    if (std::filesystem::exists("../" / p, ec)) {
        return "../" / p;
    }
    if (std::filesystem::exists("../../" / p, ec)) {
        return "../../" / p;
    }
    return p;
}

ShaderWatcher::ShaderWatcher(Device& device, Pipeline& pipeline, std::string vertPath, std::string fragPath)
    : m_device(device), 
      m_pipeline(pipeline), 
      m_vertPath(ResolveShaderPath(vertPath).string()), 
      m_fragPath(ResolveShaderPath(fragPath).string()) 
{
    std::error_code ec;
    if (std::filesystem::exists(m_vertPath, ec)) {
        m_lastVertWriteTime = std::filesystem::last_write_time(m_vertPath, ec);
    }
    if (std::filesystem::exists(m_fragPath, ec)) {
        m_lastFragWriteTime = std::filesystem::last_write_time(m_fragPath, ec);
    }
    m_lastCheckTime = std::chrono::steady_clock::now();
}

bool ShaderWatcher::CheckAndReload() {
    auto now = std::chrono::steady_clock::now();
    if (now - m_lastCheckTime < m_pollInterval) {
        return false;
    }
    m_lastCheckTime = now;

    std::error_code ec;
    if (!std::filesystem::exists(m_vertPath, ec) || !std::filesystem::exists(m_fragPath, ec)) {
        return false;
    }

    auto vertTime = std::filesystem::last_write_time(m_vertPath, ec);
    auto fragTime = std::filesystem::last_write_time(m_fragPath, ec);

    bool vertChanged = (vertTime != m_lastVertWriteTime);
    bool fragChanged = (fragTime != m_lastFragWriteTime);

    if (!vertChanged && !fragChanged) {
        return false;
    }

    std::cout << "[Sunshine HotReload] Detected shader file modification. Recompiling..." << std::endl;

    std::vector<uint32_t> vertSpirv;
    std::vector<uint32_t> fragSpirv;
    std::string vertErr, fragErr;

    bool vertOk = CompileGLSLToSPIRV(m_vertPath, "vert", vertSpirv, vertErr);
    if (!vertOk) {
        std::cerr << "\n================ [Sunshine HotReload ERROR] ================\n"
                  << "Failed to compile vertex shader: " << m_vertPath << "\n\n"
                  << vertErr
                  << "============================================================\n"
                  << "[Sunshine HotReload] Keeping active pipeline. Fix the shader error to reload.\n" << std::endl;
        m_lastVertWriteTime = vertTime;
        m_lastFragWriteTime = fragTime;
        return false;
    }

    bool fragOk = CompileGLSLToSPIRV(m_fragPath, "frag", fragSpirv, fragErr);
    if (!fragOk) {
        std::cerr << "\n================ [Sunshine HotReload ERROR] ================\n"
                  << "Failed to compile fragment shader: " << m_fragPath << "\n\n"
                  << fragErr
                  << "============================================================\n"
                  << "[Sunshine HotReload] Keeping active pipeline. Fix the shader error to reload.\n" << std::endl;
        m_lastVertWriteTime = vertTime;
        m_lastFragWriteTime = fragTime;
        return false;
    }

    // Both compiled successfully!
    m_lastVertWriteTime = vertTime;
    m_lastFragWriteTime = fragTime;

    const auto* currentDesc = m_pipeline.GetDesc();
    if (!currentDesc) {
        std::cerr << "[Sunshine HotReload Error] Current pipeline descriptor is null!" << std::endl;
        return false;
    }

    try {
        RHI::PipelineDesc newDesc = *currentDesc;

        newDesc.vertexShader = m_device.CreateShader({
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = vertSpirv,
            .entryPoint = "main"
        });

        newDesc.fragmentShader = m_device.CreateShader({
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = fragSpirv,
            .entryPoint = "main"
        });

        m_device.WaitIdle();
        auto newPipeline = m_device.CreatePipeline(newDesc);
        m_pipeline.ReplaceRHIPipeline(newPipeline.GetRHIPipeline());

        std::cout << "[Sunshine HotReload] SUCCESS: Pipeline hot-reloaded and active on GPU!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Sunshine HotReload Error] Failed to update pipeline: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Sunshine
