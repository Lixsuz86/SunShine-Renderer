#include <sunshine/sunshine.hpp>
#include "shaders/triangle_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

struct Vertex {
    float position[2];
    float color[3];
};

static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* swapchain = reinterpret_cast<Sunshine::SwapchainContext*>(glfwGetWindowUserPointer(window));
    if (swapchain) {
        swapchain->RequestResize();
    }
}

int main() {
    std::cout << "=========================================\n"
              << " Sunshine Renderer — Faz 2 Triangle Demo\n"
              << "=========================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Sunshine Renderer — 01 Triangle", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    try {
        // 1. Initialize Sunshine Device & Swapchain
        Sunshine::Device device({ .enableValidation = true });
        Sunshine::SwapchainContext swapchain(device, window);

        glfwSetWindowUserPointer(window, &swapchain);
        glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);

        std::cout << "[Sunshine] GPU Device: " << device.GetDeviceName() << std::endl;

        // 2. Vertex Data for RGB Triangle
        const std::vector<Vertex> triangleVertices = {
            { {  0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } }, // Top (Red)
            { {  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } }, // Bottom Right (Green)
            { { -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }  // Bottom Left (Blue)
        };

        auto vertexBuffer = device.CreateVertexBuffer(triangleVertices);
        std::cout << "[Sunshine] VertexBuffer created with 3 vertices (" 
                  << vertexBuffer.GetSize() << " bytes)." << std::endl;

        // 3. Shaders
        auto vs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::TriangleVertexSPIRV,
            .entryPoint = "main"
        });

        auto fs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::TriangleFragmentSPIRV,
            .entryPoint = "main"
        });

        // 4. Graphics Pipeline
        Sunshine::RHI::PipelineDesc pipelineDesc{};
        pipelineDesc.vertexShader = vs;
        pipelineDesc.fragmentShader = fs;
        pipelineDesc.vertexLayout = {
            .stride = sizeof(Vertex),
            .attributes = {
                { .location = 0, .format = Sunshine::RHI::VertexFormat::Float2, .offset = offsetof(Vertex, position) },
                { .location = 1, .format = Sunshine::RHI::VertexFormat::Float3, .offset = offsetof(Vertex, color) }
            }
        };
        pipelineDesc.topology = Sunshine::RHI::PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::None;
        pipelineDesc.rasterizer.frontFace = Sunshine::RHI::FrontFace::CounterClockwise;
        pipelineDesc.debugName = "TrianglePipeline";

        auto pipeline = device.CreatePipeline(pipelineDesc);

        // Test pipeline cache hit
        auto cachedPipeline = device.CreatePipeline(pipelineDesc);
        if (pipeline.GetRHIPipeline() == cachedPipeline.GetRHIPipeline()) {
            std::cout << "[Sunshine] Pipeline State Cache verified: identical pipeline reused successfully!" << std::endl;
        }

        std::cout << "[Sunshine] Entering main render loop..." << std::endl;

        double lastTime = glfwGetTime();
        int frameCount = 0;

        // 5. Main Render Loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            if (auto frame = device.BeginFrame(swapchain)) {
                // Clear background to dark slate
                frame->Clear(Sunshine::Color::DarkSlate());

                // Draw RGB Triangle
                frame->Draw(pipeline, vertexBuffer, 3);

                device.SubmitFrame(*frame);
            }

            frameCount++;
            double currentTime = glfwGetTime();
            if (currentTime - lastTime >= 1.0) {
                double fps = frameCount / (currentTime - lastTime);
                double msPerFrame = 1000.0 / fps;

                char title[128];
                snprintf(title, sizeof(title), "Sunshine Renderer — 01 Triangle [%.1f FPS | %.2f ms]", fps, msPerFrame);
                glfwSetWindowTitle(window, title);
                std::cout << "[Sunshine] Running @ " << static_cast<int>(fps) << " FPS (" << msPerFrame << " ms/frame)" << std::endl;

                frameCount = 0;
                lastTime = currentTime;
            }
        }

        device.WaitIdle();
        std::cout << "[Sunshine] Main loop exited cleanly." << std::endl;

    } catch (const Sunshine::GraphicsException& ex) {
        std::cerr << "\n" << ex.what() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "\n[Standard Exception] " << ex.what() << "\n";
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "[Sunshine] Exited with success." << std::endl;
    return EXIT_SUCCESS;
}
