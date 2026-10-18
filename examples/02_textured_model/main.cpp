#include "sunshine/sunshine.hpp"
#include "shaders/model_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <array>
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

// 24 vertices for a cube (each face has unique normals and UVs)
const std::vector<Vertex> c_cubeVertices = {
    // Front face (Z = +1.0, normal = 0, 0, 1)
    { {-0.8f, -0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f,  0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },

    // Back face (Z = -1.0, normal = 0, 0, -1)
    { { 0.8f, -0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f} },
    { {-0.8f, -0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f} },
    { {-0.8f,  0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f} },
    { { 0.8f,  0.8f, -0.8f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },

    // Top face (Y = +1.0, normal = 0, 1, 0)
    { {-0.8f,  0.8f,  0.8f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f,  0.8f,  0.8f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f, -0.8f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f, -0.8f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },

    // Bottom face (Y = -1.0, normal = 0, -1, 0)
    { {-0.8f, -0.8f, -0.8f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f, -0.8f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f, -0.8f,  0.8f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f, -0.8f,  0.8f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f} },

    // Right face (X = +1.0, normal = 1, 0, 0)
    { { 0.8f, -0.8f,  0.8f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
    { { 0.8f, -0.8f, -0.8f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { { 0.8f,  0.8f, -0.8f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} },
    { { 0.8f,  0.8f,  0.8f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },

    // Left face (X = -1.0, normal = -1, 0, 0)
    { {-0.8f, -0.8f, -0.8f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
    { {-0.8f, -0.8f,  0.8f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { {-0.8f,  0.8f,  0.8f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} },
    { {-0.8f,  0.8f, -0.8f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} }
};

// 36 indices for a cube (6 faces * 2 triangles)
const std::vector<uint16_t> c_cubeIndices = {
    0, 1, 2,  2, 3, 0,        // Front
    4, 5, 6,  6, 7, 4,        // Back
    8, 9, 10, 10, 11, 8,      // Top
    12, 13, 14, 14, 15, 12,   // Bottom
    16, 17, 18, 18, 19, 16,   // Right
    20, 21, 22, 22, 23, 20    // Left
};

// Generates a 256x256 procedural texture (Sunshine theme checkerboard + accent borders)
std::vector<uint8_t> GenerateCheckerboardTexture(uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels(width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t index = (y * width + x) * 4;
            bool check = ((x / 32) + (y / 32)) % 2 == 0;
            bool border = (x % 32 == 0 || y % 32 == 0);

            if (border) {
                // Bright Gold Sunshine Accent border
                pixels[index + 0] = 255; // R
                pixels[index + 1] = 215; // G
                pixels[index + 2] = 0;   // B
                pixels[index + 3] = 255; // A
            } else if (check) {
                // Vibrant Amber Orange
                pixels[index + 0] = 255;
                pixels[index + 1] = 140;
                pixels[index + 2] = 0;
                pixels[index + 3] = 255;
            } else {
                // Deep Navy Slate
                pixels[index + 0] = 30;
                pixels[index + 1] = 41;
                pixels[index + 2] = 59;
                pixels[index + 3] = 255;
            }
        }
    }
    return pixels;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << "  Sunshine Renderer - Phase 3: Textured 3D Model Demo" << std::endl;
    std::cout << "======================================================" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    constexpr uint32_t WIDTH = 1280;
    constexpr uint32_t HEIGHT = 720;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer - 02 Textured 3D Model", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    try {
        // 1. Initialize Engine Device & Swapchain Context
        Sunshine::DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "02_textured_model";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // 2. Compile/Create Shaders from portable SPIR-V bytecode
        auto vs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::ModelVertexSPIRV,
            .entryPoint = "main"
        });

        auto fs = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::ModelFragmentSPIRV,
            .entryPoint = "main"
        });

        // 3. Create Pipeline with Depth Testing & Descriptor Bindings
        Sunshine::RHI::PipelineDesc pipeDesc{};
        pipeDesc.vertexShader = vs;
        pipeDesc.fragmentShader = fs;
        pipeDesc.debugName = "ModelPipeline";

        // Vertex layout: Position(vec3) + Normal(vec3) + TexCoord(vec2)
        pipeDesc.vertexLayout.stride = sizeof(Vertex);
        pipeDesc.vertexLayout.attributes = {
            { 0, Sunshine::RHI::VertexFormat::Float3, offsetof(Vertex, position) },
            { 1, Sunshine::RHI::VertexFormat::Float3, offsetof(Vertex, normal) },
            { 2, Sunshine::RHI::VertexFormat::Float2, offsetof(Vertex, texCoord) }
        };

        // Enable Depth Testing
        pipeDesc.depthStencil.depthTestEnable = true;
        pipeDesc.depthStencil.depthWriteEnable = true;
        pipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::Less;

        // Descriptors: Binding 0 = UniformBuffer, Binding 1 = CombinedImageSampler
        pipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex },
            { 1, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment }
        };

        auto pipeline = device.CreatePipeline(pipeDesc);

        // 4. Create Buffers (Vertex, Index, Uniform)
        auto vertexBuffer = device.CreateVertexBuffer(c_cubeVertices);
        auto indexBuffer = device.CreateIndexBuffer(c_cubeIndices);
        auto ubo = device.CreateUniformBuffer<UniformBufferObject>();

        // 5. Create Procedural Texture & Sampler
        constexpr uint32_t TEX_SIZE = 256;
        auto texPixels = GenerateCheckerboardTexture(TEX_SIZE, TEX_SIZE);
        auto texture = device.CreateTexture2D(TEX_SIZE, TEX_SIZE, texPixels.data());

        Sunshine::RHI::SamplerDesc samplerDesc{};
        samplerDesc.minFilter = Sunshine::RHI::FilterMode::Linear;
        samplerDesc.magFilter = Sunshine::RHI::FilterMode::Linear;
        samplerDesc.addressModeU = Sunshine::RHI::AddressMode::Repeat;
        samplerDesc.addressModeV = Sunshine::RHI::AddressMode::Repeat;
        auto sampler = device.CreateSampler(samplerDesc);

        // 6. Camera System
        Sunshine::Camera camera(45.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
        camera.LookAt(glm::vec3(0.0f, 1.8f, 3.2f), glm::vec3(0.0f, 0.0f, 0.0f));

        std::cout << "[Demo] Entering main render loop..." << std::endl;
        std::cout << "[Demo] Rendering 3D textured rotating cube with depth testing!" << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue; // Window minimized
            }

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            camera.SetAspectRatio(static_cast<float>(currentWidth) / static_cast<float>(currentHeight));

            // Update rotation and Uniform Buffer Object
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            UniformBufferObject uboData{};
            uboData.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.3f, 1.0f, 0.2f));
            uboData.view = camera.GetViewMatrix();
            uboData.proj = camera.GetProjectionMatrix();
            ubo.Update(uboData);

            // Render Frame
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                // Clear with modern dark twilight background
                frame->Clear(Sunshine::Color{ 0.08f, 0.10f, 0.14f, 1.0f });

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
