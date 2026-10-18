#include "sunshine/sunshine.hpp"
#include "shaders/hot_reload_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <chrono>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// 24 vertices for a 3D cube with independent faces, normals and UVs
const std::vector<Vertex> c_cubeVertices = {
    // Front face (Z = +0.8)
    { {-0.8f, -0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },

    // Back face (Z = -0.8)
    { { 0.8f, -0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f} },
    { {-0.8f, -0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f} },
    { {-0.8f,  0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f} },
    { { 0.8f,  0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },

    // Top face (Y = +0.8)
    { {-0.8f,  0.8f,  0.8f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f,  0.8f,  0.8f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f, -0.8f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f, -0.8f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

    // Bottom face (Y = -0.8)
    { {-0.8f, -0.8f, -0.8f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f, -0.8f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f, -0.8f,  0.8f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f, -0.8f,  0.8f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f} },

    // Right face (X = +0.8)
    { { 0.8f, -0.8f,  0.8f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f, -0.8f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f, -0.8f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} },
    { { 0.8f,  0.8f,  0.8f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

    // Left face (X = -0.8)
    { {-0.8f, -0.8f, -0.8f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
    { {-0.8f, -0.8f,  0.8f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { {-0.8f,  0.8f,  0.8f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f, -0.8f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} }
};

const std::vector<uint16_t> c_cubeIndices = {
    0, 1, 2,  2, 3, 0,
    4, 5, 6,  6, 7, 4,
    8, 9, 10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20
};

std::vector<uint8_t> GenerateGridTexture(uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels(width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t idx = (y * width + x) * 4;
            bool check = ((x / 32) + (y / 32)) % 2 == 0;
            bool border = (x % 32 == 0 || y % 32 == 0);

            if (border) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else if (check) {
                pixels[idx + 0] = 220;
                pixels[idx + 1] = 220;
                pixels[idx + 2] = 230;
                pixels[idx + 3] = 255;
            } else {
                pixels[idx + 0] = 160;
                pixels[idx + 1] = 170;
                pixels[idx + 2] = 190;
                pixels[idx + 3] = 255;
            }
        }
    }
    return pixels;
}

int main() {
    std::cout << "\n======================================================================" << std::endl;
    std::cout << "    Sunshine Renderer — Phase 4: Live Shader Hot-Reloading Demo" << std::endl;
    std::cout << "======================================================================" << std::endl;
    std::cout << "  [HotReload Guide]:" << std::endl;
    std::cout << "    1. While this window is running, open:" << std::endl;
    std::cout << "       examples/03_hot_reload/shaders/hot_reload.frag" << std::endl;
    std::cout << "    2. Edit the 'vec3 tint = ...' line and save the file." << std::endl;
    std::cout << "    3. Watch the shader recompile and update LIVE on screen!" << std::endl;
    std::cout << "    4. Try writing invalid syntax: the engine won't crash!" << std::endl;
    std::cout << "  [Controls]: ESC or close window to exit." << std::endl;
    std::cout << "======================================================================\n" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    constexpr uint32_t WIDTH = 1280;
    constexpr uint32_t HEIGHT = 720;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer - 03 Live Shader Hot Reload", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    try {
        Sunshine::DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "03_hot_reload";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // Initial Shaders
        auto vs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::HotReloadVertexSPIRV,
            .entryPoint = "main"
        });

        auto fs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::HotReloadFragmentSPIRV,
            .entryPoint = "main"
        });

        // Pipeline
        Sunshine::RHI::PipelineDesc pipeDesc{};
        pipeDesc.vertexShader = vs;
        pipeDesc.fragmentShader = fs;
        pipeDesc.debugName = "HotReloadPipeline";

        pipeDesc.vertexLayout.stride = sizeof(Vertex);
        pipeDesc.vertexLayout.attributes = {
            { 0, Sunshine::RHI::VertexFormat::Float3, offsetof(Vertex, position) },
            { 1, Sunshine::RHI::VertexFormat::Float3, offsetof(Vertex, normal) },
            { 2, Sunshine::RHI::VertexFormat::Float2, offsetof(Vertex, texCoord) }
        };

        pipeDesc.depthStencil.depthTestEnable = true;
        pipeDesc.depthStencil.depthWriteEnable = true;
        pipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::Less;

        pipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex },
            { 1, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment }
        };

        auto pipeline = device.CreatePipeline(pipeDesc);

        // Buffers
        auto vertexBuffer = device.CreateVertexBuffer(c_cubeVertices);
        auto indexBuffer = device.CreateIndexBuffer(c_cubeIndices);
        auto ubo = device.CreateUniformBuffer<UniformBufferObject>();

        // Texture & Sampler
        constexpr uint32_t TEX_SIZE = 256;
        auto texPixels = GenerateGridTexture(TEX_SIZE, TEX_SIZE);
        auto texture = device.CreateTexture2D(TEX_SIZE, TEX_SIZE, texPixels.data());

        Sunshine::RHI::SamplerDesc samplerDesc{};
        samplerDesc.minFilter = Sunshine::RHI::FilterMode::Linear;
        samplerDesc.magFilter = Sunshine::RHI::FilterMode::Linear;
        auto sampler = device.CreateSampler(samplerDesc);

        // Camera
        Sunshine::Camera camera(45.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
        camera.LookAt(glm::vec3(0.0f, 1.8f, 3.2f), glm::vec3(0.0f, 0.0f, 0.0f));

        // Shader Watcher for Live Hot Reloading
        Sunshine::ShaderWatcher watcher(
            device, 
            pipeline, 
            "examples/03_hot_reload/shaders/hot_reload.vert", 
            "examples/03_hot_reload/shaders/hot_reload.frag"
        );

        auto startTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue;
            }

            // Check if shaders were modified on disk; if so, recompile and swap pipeline on GPU!
            watcher.CheckAndReload();

            camera.SetAspectRatio(static_cast<float>(currentWidth) / static_cast<float>(currentHeight));

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            UniformBufferObject uboData{};
            uboData.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(35.0f), glm::vec3(0.2f, 1.0f, 0.3f));
            uboData.view = camera.GetViewMatrix();
            uboData.proj = camera.GetProjectionMatrix();
            ubo.Update(uboData);

            // Render
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                frame->Clear(Sunshine::Color{ 0.07f, 0.08f, 0.11f, 1.0f });

                auto& cmd = frame->GetCommandContext();
                cmd.SetPipeline(pipeline);
                cmd.SetVertexBuffer(vertexBuffer);
                cmd.SetIndexBuffer(indexBuffer);
                cmd.SetUniformBuffer(0, ubo);
                cmd.SetTexture(1, texture, sampler);
                cmd.DrawIndexed(static_cast<uint32_t>(c_cubeIndices.size()));

                device.SubmitFrame(*frame);
            }
        }

        device.WaitIdle();
        std::cout << "[Demo] Exiting cleanly." << std::endl;

    } catch (const Sunshine::GraphicsException& e) {
        std::cerr << "\n[Sunshine Caught Exception] " << e.what() << std::endl;
        if (!e.GetSuggestion().empty()) {
            std::cerr << "-> Troubleshooting Tip: " << e.GetSuggestion() << std::endl;
        }
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "[Demo Error] " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
