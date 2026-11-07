#include "sunshine/sunshine.hpp"
#include "shaders/particle_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <chrono>

using namespace Sunshine;

// 48-byte particle structure matching compute & vertex shader memory layout
struct Particle {
    glm::vec4 pos; // xyz = position, w = life
    glm::vec4 vel; // xyz = velocity, w = point size
    glm::vec4 col; // rgba color
};

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
};

struct SimulationUBO {
    glm::vec4 attractorA{0.0f};
    glm::vec4 attractorB{0.0f};
    glm::vec4 attractorC{0.0f};
    glm::vec4 attractorD{0.0f};
    float deltaTime{0.016f};
    float time{0.0f};
    int32_t mode{0}; // 0 = 4 Attractors, 1 = Vortex Tornado, 2 = Galactic Supernova
    int32_t particleCount{0};
};

// Global Input & Camera State
static bool g_mousePressed = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;
static float g_yaw = -90.0f;
static float g_pitch = 20.0f;
static float g_distance = 32.0f;

static int g_simMode = 0; // 0, 1, 2
static bool g_multiThreadedRecording = true;
static bool g_simulationPaused = false;

static void MouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_mousePressed = (action == GLFW_PRESS);
    }
}

static void CursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (g_mousePressed) {
        float xoffset = static_cast<float>(xpos - g_lastMouseX);
        float yoffset = static_cast<float>(g_lastMouseY - ypos);

        constexpr float sensitivity = 0.25f;
        g_yaw += xoffset * sensitivity;
        g_pitch += yoffset * sensitivity;
        g_pitch = std::clamp(g_pitch, -85.0f, 85.0f);
    }
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;
}

static void ScrollCallback(GLFWwindow*, double, double yoffset) {
    g_distance -= static_cast<float>(yoffset) * 2.0f;
    g_distance = std::clamp(g_distance, 5.0f, 90.0f);
}

static void KeyCallback(GLFWwindow*, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) g_simMode = 0;
        if (key == GLFW_KEY_2) g_simMode = 1;
        if (key == GLFW_KEY_3) g_simMode = 2;
        if (key == GLFW_KEY_T) g_multiThreadedRecording = !g_multiThreadedRecording;
        if (key == GLFW_KEY_SPACE) g_simulationPaused = !g_simulationPaused;
        if (key == GLFW_KEY_R) {
            g_yaw = -90.0f;
            g_pitch = 20.0f;
            g_distance = 32.0f;
        }
    }
}

int main() {
    std::cout << "========================================================\n"
              << " Sunshine Renderer - Phase 9: GPU Particles & Multi-threading\n"
              << " Compute Shader Simulation + Secondary Command Recording\n"
              << " Controls: Left Mouse Drag = Orbit | Scroll = Zoom\n"
              << "           1 = Attractor Dance | 2 = Cosmic Vortex | 3 = Supernova\n"
              << "           T = Toggle Multi-threaded Recording\n"
              << "           Space = Pause | R = Reset Camera\n"
              << "========================================================" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    constexpr uint32_t WIDTH = 1280;
    constexpr uint32_t HEIGHT = 720;
    constexpr uint32_t PARTICLE_COUNT = 131072; // 131,072 GPU Particles (512 workgroups of 256)

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer - Phase 9 (GPU Particles)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);

    try {
        Device device({.enableValidation = true, .applicationName = "Sunshine GPU Particles"});
        SwapchainContext swapchain(device, window);
        WorkerThreadPool threadPool; // Uses CPU hardware concurrency

        std::cout << "[Sunshine Engine] WorkerThreadPool initialized with " 
                  << threadPool.GetThreadCount() << " worker threads." << std::endl;

        // 1. Initialize 131,072 Particles on Host CPU
        std::vector<Particle> initialParticles(PARTICLE_COUNT);
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> distAngle(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> distRadius(1.0f, 14.0f);
        std::uniform_real_distribution<float> distHeight(-2.5f, 2.5f);
        std::uniform_real_distribution<float> distSpeed(-1.0f, 1.0f);
        std::uniform_real_distribution<float> distLife(0.2f, 1.0f);
        std::uniform_real_distribution<float> distSize(2.5f, 5.5f);

        for (uint32_t i = 0; i < PARTICLE_COUNT; ++i) {
            float angle = distAngle(rng);
            float r = distRadius(rng);
            float y = distHeight(rng);

            initialParticles[i].pos = glm::vec4(r * std::cos(angle), y, r * std::sin(angle), distLife(rng));
            initialParticles[i].vel = glm::vec4(distSpeed(rng), distSpeed(rng), distSpeed(rng), distSize(rng));
            initialParticles[i].col = glm::vec4(0.2f, 0.6f, 1.0f, 0.9f);
        }

        // 2. Create GPU Storage Buffer (SSBO + Vertex Buffer flag)
        auto particleSSBO = device.CreateStorageBuffer<Particle>(
            PARTICLE_COUNT, initialParticles.data(), RHI::BufferUsage::Vertex
        );
        auto particleVB = particleSSBO.AsVertexBuffer();

        // 3. Create Uniform Buffers
        SimulationUBO simData{};
        simData.particleCount = PARTICLE_COUNT;
        auto simUBO = device.CreateUniformBuffer<SimulationUBO>(&simData);

        CameraUBO camData{};
        auto cameraUBO = device.CreateUniformBuffer<CameraUBO>(&camData);

        // 4. Create Compute Pipeline (particle.comp)
        RHI::ShaderDesc compShaderDesc{
            .stage = RHI::ShaderStage::Compute,
            .spirvCode = Shaders::ParticleComputeSPIRV,
            .entryPoint = "main"
        };
        auto computeShader = device.CreateShader(compShaderDesc);

        RHI::ComputePipelineDesc compPipeDesc{
            .computeShader = computeShader,
            .descriptorBindings = {
                { .binding = 0, .type = RHI::DescriptorType::StorageBuffer, .stageFlags = RHI::ShaderStage::Compute },
                { .binding = 1, .type = RHI::DescriptorType::UniformBuffer, .stageFlags = RHI::ShaderStage::Compute }
            },
            .debugName = "ParticleSimulationComputePipeline"
        };
        auto computePipeline = device.CreateComputePipeline(compPipeDesc);

        // 5. Create Graphics Pipeline for Particle Rendering
        RHI::ShaderDesc vertShaderDesc{
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = Shaders::ParticleVertexSPIRV,
            .entryPoint = "main"
        };
        auto vertShader = device.CreateShader(vertShaderDesc);

        RHI::ShaderDesc fragShaderDesc{
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = Shaders::ParticleFragmentSPIRV,
            .entryPoint = "main"
        };
        auto fragShader = device.CreateShader(fragShaderDesc);

        RHI::VertexLayout particleLayout{};
        particleLayout.stride = sizeof(Particle);
        particleLayout.bindings = {
            { .binding = 0, .stride = sizeof(Particle), .inputRate = RHI::VertexInputRate::PerVertex }
        };
        particleLayout.attributes = {
            { .location = 0, .format = RHI::VertexFormat::Float4, .offset = offsetof(Particle, pos), .binding = 0 },
            { .location = 1, .format = RHI::VertexFormat::Float4, .offset = offsetof(Particle, vel), .binding = 0 },
            { .location = 2, .format = RHI::VertexFormat::Float4, .offset = offsetof(Particle, col), .binding = 0 }
        };

        RHI::PipelineDesc graphicsPipeDesc{
            .vertexShader = vertShader,
            .fragmentShader = fragShader,
            .vertexLayout = particleLayout,
            .topology = RHI::PrimitiveTopology::PointList,
            .rasterizer = { .cullMode = RHI::CullMode::None, .polygonMode = RHI::PolygonMode::Point },
            .blend = { .blendEnable = true }, // Additive blending
            .depthStencil = { .depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = RHI::CompareOp::LessOrEqual },
            .descriptorBindings = {
                { .binding = 0, .type = RHI::DescriptorType::UniformBuffer, .stageFlags = RHI::ShaderStage::Vertex }
            },
            .colorAttachmentCount = 1,
            .debugName = "ParticleRenderGraphicsPipeline"
        };
        auto graphicsPipeline = device.CreatePipeline(graphicsPipeDesc);

        // 6. Frame Loop Timing & Telemetry
        auto lastTime = std::chrono::high_resolution_clock::now();
        auto lastTitleTime = lastTime;
        float totalTime = 0.0f;
        uint32_t frameCounter = 0;

        const size_t threadCount = threadPool.GetThreadCount();
        const size_t batchCount = std::max<size_t>(1, threadCount * 2);
        const uint32_t particlesPerBatch = PARTICLE_COUNT / static_cast<uint32_t>(batchCount);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;
            dt = std::clamp(dt, 0.001f, 0.05f);

            if (!g_simulationPaused) {
                totalTime += dt;
            }

            frameCounter++;
            float titleElapsed = std::chrono::duration<float>(currentTime - lastTitleTime).count();
            if (titleElapsed >= 0.15f) {
                float fps = frameCounter / titleElapsed;
                float ms = (1000.0f / (fps > 0.0001f ? fps : 1.0f));

                const char* modeNames[] = { "Attractor Dance", "Cosmic Vortex", "Galactic Supernova" };
                std::string title = "Sunshine Renderer - Phase 9 | " +
                                    std::to_string(static_cast<int>(fps + 0.5f)) + " FPS (" +
                                    std::to_string(ms).substr(0, 4) + " ms) | " +
                                    "Mode: [" + modeNames[g_simMode] + "] | " +
                                    "Recording: [" + (g_multiThreadedRecording ? ("Multi-Threaded (" + std::to_string(threadCount) + " Threads)") : "Single-Threaded") + "] | " +
                                    "Particles: 131,072";

                glfwSetWindowTitle(window, title.c_str());
                frameCounter = 0;
                lastTitleTime = currentTime;
            }

            // Update Dynamic Attractor Positions
            float t = totalTime * 0.8f;
            simData.attractorA = glm::vec4(std::cos(t * 1.2f) * 8.0f, std::sin(t * 0.9f) * 4.0f, std::sin(t * 1.2f) * 8.0f, 65.0f);
            simData.attractorB = glm::vec4(std::cos(t * 1.5f + 2.0f) * 10.0f, std::cos(t * 1.1f) * 5.0f, std::sin(t * 1.5f + 2.0f) * 10.0f, 80.0f);
            simData.attractorC = glm::vec4(std::sin(t * 0.7f) * 7.0f, std::sin(t * 1.4f) * 6.0f, std::cos(t * 0.7f) * 7.0f, -50.0f); // Repulsor!
            simData.attractorD = glm::vec4(std::sin(t * 1.8f + 1.0f) * 6.0f, std::cos(t * 0.6f) * 3.0f, std::cos(t * 1.8f + 1.0f) * 6.0f, 70.0f);
            simData.deltaTime = g_simulationPaused ? 0.0f : dt;
            simData.time = totalTime;
            simData.mode = g_simMode;
            simData.particleCount = PARTICLE_COUNT;
            simUBO.Update(simData);

            // Update Orbit Camera
            int fbWidth = 0, fbHeight = 0;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            if (fbWidth == 0 || fbHeight == 0) continue;

            float radYaw = glm::radians(g_yaw);
            float radPitch = glm::radians(g_pitch);
            glm::vec3 camPos{
                g_distance * std::cos(radPitch) * std::cos(radYaw),
                g_distance * std::sin(radPitch),
                g_distance * std::cos(radPitch) * std::sin(radYaw)
            };

            camData.view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            camData.proj = glm::perspective(glm::radians(50.0f), static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 300.0f);
            camData.proj[1][1] *= -1.0f; // Invert Y for Vulkan
            camData.cameraPos = glm::vec4(camPos, 1.0f);
            cameraUBO.Update(camData);

            // Begin Frame
            auto frameOpt = device.BeginFrame(swapchain);
            if (!frameOpt) continue;
            auto& frame = *frameOpt;
            auto& cmd = frame.GetCommandContext();

            // ================================================================
            // PASS 1: GPU Compute Shader Particle Simulation
            // ================================================================
            cmd.SetComputePipeline(computePipeline);
            cmd.SetStorageBuffer(0, particleSSBO);
            cmd.SetUniformBuffer(1, simUBO);
            cmd.Dispatch(PARTICLE_COUNT / 256, 1, 1);

            // Memory Barrier: Synchronize Compute Writes -> Vertex Input Reads
            cmd.ComputeToGraphicsBarrier();

            // ================================================================
            // PASS 2: Graphics Pass (Single-thread vs Multi-thread Demonstration)
            // ================================================================
            Color clearColor{ 0.015f, 0.018f, 0.025f, 1.0f }; // Deep void cosmic background

            if (g_multiThreadedRecording) {
                // Multi-threaded Secondary Command Recording via WorkerThreadPool
                cmd.BeginSecondaryPass(clearColor);

                cmd.RecordParallel(threadPool, batchCount, [&](size_t batchIndex, size_t, SecondaryCommandContext& secCmd) {
                    secCmd.SetPipeline(graphicsPipeline);
                    secCmd.SetVertexBuffer(0, particleVB, 0);
                    secCmd.SetUniformBuffer(0, cameraUBO);
                    secCmd.Draw(particlesPerBatch, 1, static_cast<uint32_t>(batchIndex * particlesPerBatch), 0);
                });

                cmd.EndPass();
            } else {
                // Single-threaded Primary Command Recording
                cmd.Clear(clearColor);
                cmd.SetPipeline(graphicsPipeline);
                cmd.SetVertexBuffer(0, particleVB, 0);
                cmd.SetUniformBuffer(0, cameraUBO);
                cmd.Draw(PARTICLE_COUNT, 1, 0, 0);
                cmd.EndPass();
            }

            device.SubmitFrame(frame);
        }

        device.WaitIdle();
    }
    catch (const std::exception& e) {
        std::cerr << "\n[Sunshine Fatal Exception]: " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "[Sunshine Engine] Graceful shutdown completed." << std::endl;
    return 0;
}
