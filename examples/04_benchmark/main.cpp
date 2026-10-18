#include "sunshine/sunshine.hpp"
#include "shaders/pbr_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr float PI = 3.14159265358979323846f;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct alignas(16) InstanceData {
    glm::vec4 modelCol0;
    glm::vec4 modelCol1;
    glm::vec4 modelCol2;
    glm::vec4 modelCol3;
    glm::vec4 material; // x: metallic, y: roughness, z: ao, w: phase
    glm::vec4 color;    // rgb: albedo, a: opacity
};

struct Light {
    glm::vec4 position; // xyz: world pos, w: intensity
    glm::vec4 color;    // rgb: color, w: radius
};

struct alignas(16) SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    Light lights[4];
    glm::vec4 postProcessParams; // x: enableTonemap (1/0), y: exposure, z: vignette, w: time
};

// Procedural UV Sphere generator for realistic PBR reflections
void GenerateUVSphere(float radius, uint32_t rings, uint32_t sectors, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float R = 1.0f / static_cast<float>(rings - 1);
    const float S = 1.0f / static_cast<float>(sectors - 1);

    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < sectors; ++s) {
            float y = std::sin(-PI / 2.0f + PI * r * R);
            float x = std::cos(2.0f * PI * s * S) * std::sin(PI * r * R);
            float z = std::sin(2.0f * PI * s * S) * std::sin(PI * r * R);

            glm::vec3 pos = glm::vec3(x, y, z) * radius;
            glm::vec3 norm = glm::normalize(pos);
            glm::vec2 uv = glm::vec2(s * S, r * R);

            outVertices.push_back({ pos, norm, uv });
        }
    }

    for (uint32_t r = 0; r < rings - 1; ++r) {
        for (uint32_t s = 0; s < sectors - 1; ++s) {
            uint32_t i0 = r * sectors + s;
            uint32_t i1 = (r + 1) * sectors + s;
            uint32_t i2 = (r + 1) * sectors + (s + 1);
            uint32_t i3 = r * sectors + (s + 1);

            outIndices.push_back(i0);
            outIndices.push_back(i1);
            outIndices.push_back(i2);

            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
    }
}

// Instance properties for simulation
struct ObjectSim {
    glm::vec3 basePos;
    glm::vec3 rotationAxis;
    float rotationSpeed;
    float orbitRadius;
    float orbitSpeed;
    float phase;
    float metallic;
    float roughness;
    glm::vec3 albedo;
};

std::vector<ObjectSim> CreateSimulationEntities(uint32_t count) {
    std::vector<ObjectSim> sims;
    sims.reserve(count);

    // Arrange in a 3D spherical galaxy / grid cluster
    int side = static_cast<int>(std::cbrt(static_cast<float>(count))) + 1;
    float spacing = 3.6f;
    float halfExt = (side * spacing) * 0.5f;

    uint32_t index = 0;
    for (int x = 0; x < side && index < count; ++x) {
        for (int y = 0; y < side && index < count; ++y) {
            for (int z = 0; z < side && index < count; ++z) {
                ObjectSim sim{};
                sim.basePos = glm::vec3(
                    x * spacing - halfExt,
                    y * spacing - halfExt,
                    z * spacing - halfExt
                );

                float fx = static_cast<float>(x) / static_cast<float>(side);
                float fy = static_cast<float>(y) / static_cast<float>(side);
                float fz = static_cast<float>(z) / static_cast<float>(side);

                // Varied PBR material properties across grid
                sim.metallic = fx;
                sim.roughness = std::clamp(fy, 0.05f, 0.95f);
                sim.phase = fz * 6.28f;
                sim.rotationAxis = glm::normalize(glm::vec3(fx + 0.1f, fy + 0.5f, fz + 0.2f));
                sim.rotationSpeed = 0.5f + (fx + fy) * 1.5f;
                sim.orbitRadius = 0.4f + fx * 0.5f;
                sim.orbitSpeed = 0.8f + fz * 1.2f;

                // Palette: Golden amber, cyber turquoise, vibrant ruby, neon emerald, royal purple
                float hue = fmod(fx * 2.0f + fy * 1.5f + fz, 1.0f);
                if (hue < 0.2f) {
                    sim.albedo = glm::vec3(1.0f, 0.76f, 0.33f); // Gold
                } else if (hue < 0.4f) {
                    sim.albedo = glm::vec3(0.15f, 0.85f, 0.95f); // Cyan
                } else if (hue < 0.6f) {
                    sim.albedo = glm::vec3(0.95f, 0.20f, 0.35f); // Ruby Red
                } else if (hue < 0.8f) {
                    sim.albedo = glm::vec3(0.20f, 0.95f, 0.45f); // Emerald
                } else {
                    sim.albedo = glm::vec3(0.75f, 0.35f, 0.95f); // Amethyst
                }

                sims.push_back(sim);
                index++;
            }
        }
    }

    return sims;
}

} // anonymous namespace

int main() {
    std::cout << "\n======================================================================\n"
              << "    Sunshine Renderer — Phase 5: High-Performance PBR Benchmark\n"
              << "======================================================================\n"
              << "  [Key Features Tested]:\n"
              << "    * Hardware Instanced Rendering (thousands of objects in 1 draw call)\n"
              << "    * Cook-Torrance Microfacet PBR Lighting (GGX / Smith / Fresnel)\n"
              << "    * 4 Dynamic Colored Radiance Lights\n"
              << "    * Real-time Telemetry (FPS, Frame Time, Triangles, Draw Calls)\n"
              << "    * ACES Filmic Tone Mapping + Vignette + Gamma 2.2 Post-Processing\n"
              << "  [Interactive Controls]:\n"
              << "    [SPACE] : Toggle Post-Processing (ACES Filmic vs Linear HDR)\n"
              << "    [1]     : 500 Instances\n"
              << "    [2]     : 1,500 Instances\n"
              << "    [3]     : 2,500 Instances (Default Stress Test)\n"
              << "    [4]     : 5,000 Instances (Extreme Stress Test)\n"
              << "    [ESC]   : Exit Demo\n"
              << "======================================================================\n" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer — Phase 5 Benchmark", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    try {
        Sunshine::DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "04_benchmark";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Benchmark] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // Sphere mesh (14 rings, 14 sectors = ~300 vertices, ~600 triangles per instance)
        std::vector<Vertex> sphereVertices;
        std::vector<uint32_t> sphereIndices;
        GenerateUVSphere(1.1f, 14, 14, sphereVertices, sphereIndices);

        auto vertexBuffer = device.CreateVertexBuffer(sphereVertices);
        auto indexBuffer = device.CreateIndexBuffer(sphereIndices);

        // Simulation entities (up to 5,000 instances)
        constexpr uint32_t MAX_INSTANCES = 5000;
        uint32_t activeInstances = 2500;
        auto simulationEntities = CreateSimulationEntities(MAX_INSTANCES);

        std::vector<InstanceData> instanceDataCpu(MAX_INSTANCES);

        // Initial instance data buffer creation
        auto instanceBuffer = device.CreateVertexBuffer(
            sizeof(InstanceData) * MAX_INSTANCES, 
            nullptr
        );

        // Pipeline with multi-binding layout
        Sunshine::RHI::PipelineDesc pipeDesc{};
        pipeDesc.debugName = "PBRInstancedPipeline";

        pipeDesc.vertexShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::PBRVertexSPIRV,
            .entryPoint = "main"
        });

        pipeDesc.fragmentShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::PBRFragmentSPIRV,
            .entryPoint = "main"
        });

        // Binding 0: Per-vertex geometry
        // Binding 1: Per-instance data
        pipeDesc.vertexLayout.bindings = {
            { 0, sizeof(Vertex), Sunshine::RHI::VertexInputRate::PerVertex },
            { 1, sizeof(InstanceData), Sunshine::RHI::VertexInputRate::PerInstance }
        };

        pipeDesc.vertexLayout.attributes = {
            // Binding 0: Vertex
            { 0, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0 },
            { 1, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0 },
            { 2, Sunshine::RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0 },

            // Binding 1: Instance Data
            { 3, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol0)), 1 },
            { 4, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol1)), 1 },
            { 5, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol2)), 1 },
            { 6, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol3)), 1 },
            { 7, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, material)), 1 },
            { 8, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, color)), 1 }
        };

        pipeDesc.depthStencil.depthTestEnable = true;
        pipeDesc.depthStencil.depthWriteEnable = true;
        pipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::Less;

        pipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex | Sunshine::RHI::ShaderStage::Fragment }
        };

        auto pipeline = device.CreatePipeline(pipeDesc);
        auto sceneUbo = device.CreateUniformBuffer<SceneUBO>();

        // Camera
        Sunshine::Camera camera(50.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 300.0f);
        camera.LookAt(glm::vec3(0.0f, 25.0f, 65.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        Sunshine::Profiler profiler;
        auto startTime = std::chrono::high_resolution_clock::now();

        bool enablePostProcessing = true;
        bool spaceWasPressed = false;
        bool key1WasPressed = false, key2WasPressed = false, key3WasPressed = false, key4WasPressed = false;

        std::cout << "[Benchmark] Starting main loop with " << activeInstances << " instances ("
                  << (activeInstances * (sphereIndices.size() / 3)) << " triangles per frame)..." << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // Space: Toggle Post-Processing
            bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
            if (spacePressed && !spaceWasPressed) {
                enablePostProcessing = !enablePostProcessing;
                std::cout << "[Benchmark] Post-Processing (ACES Filmic): " 
                          << (enablePostProcessing ? "ENABLED" : "DISABLED (Linear HDR)") << std::endl;
            }
            spaceWasPressed = spacePressed;

            // Keys 1..4: Instance count selection
            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key1WasPressed) activeInstances = 500;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key2WasPressed) activeInstances = 1500;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !key3WasPressed) activeInstances = 2500;
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !key4WasPressed) activeInstances = 5000;
            key1WasPressed = (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS);
            key2WasPressed = (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS);
            key3WasPressed = (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS);
            key4WasPressed = (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS);

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue;
            }

            profiler.BeginFrame();

            camera.SetAspectRatio(static_cast<float>(currentWidth) / static_cast<float>(currentHeight));

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Dynamic camera orbiting
            float camRadius = 55.0f;
            float camX = std::sin(time * 0.15f) * camRadius;
            float camZ = std::cos(time * 0.15f) * camRadius;
            camera.LookAt(glm::vec3(camX, 22.0f + std::sin(time * 0.2f) * 8.0f, camZ), glm::vec3(0.0f, 0.0f, 0.0f));

            // Update Simulation & Instances on CPU
            for (uint32_t i = 0; i < activeInstances; ++i) {
                const auto& sim = simulationEntities[i];

                float orbitAngle = time * sim.orbitSpeed + sim.phase;
                glm::vec3 pos = sim.basePos + glm::vec3(
                    std::cos(orbitAngle) * sim.orbitRadius,
                    std::sin(orbitAngle * 1.5f) * sim.orbitRadius,
                    std::sin(orbitAngle) * sim.orbitRadius
                );

                glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
                model = glm::rotate(model, time * sim.rotationSpeed + sim.phase, sim.rotationAxis);

                instanceDataCpu[i].modelCol0 = model[0];
                instanceDataCpu[i].modelCol1 = model[1];
                instanceDataCpu[i].modelCol2 = model[2];
                instanceDataCpu[i].modelCol3 = model[3];
                instanceDataCpu[i].material = glm::vec4(sim.metallic, sim.roughness, 1.0f, 0.0f);
                instanceDataCpu[i].color = glm::vec4(sim.albedo, 1.0f);
            }

            // Stream updated instance matrices to GPU vertex buffer
            instanceBuffer.Update(instanceDataCpu.data(), activeInstances * sizeof(InstanceData));

            // Scene UBO (Lights, Matrices, Post-Process params)
            SceneUBO uboData{};
            uboData.view = camera.GetViewMatrix();
            uboData.proj = camera.GetProjectionMatrix();
            uboData.camPos = glm::vec4(camera.GetPosition(), 1.0f);

            // Light 0: Sun Key Light (Warm Amber)
            uboData.lights[0].position = glm::vec4(std::cos(time * 0.5f) * 45.0f, 35.0f, std::sin(time * 0.5f) * 45.0f, 2200.0f);
            uboData.lights[0].color = glm::vec4(1.0f, 0.85f, 0.65f, 100.0f);

            // Light 1: Neon Cyan Orbit Light
            uboData.lights[1].position = glm::vec4(std::sin(time * 1.2f) * 35.0f, std::cos(time * 0.8f) * 25.0f, std::cos(time * 1.2f) * 35.0f, 1600.0f);
            uboData.lights[1].color = glm::vec4(0.1f, 0.95f, 1.0f, 80.0f);

            // Light 2: Neon Ruby Orbit Light
            uboData.lights[2].position = glm::vec4(std::cos(-time * 1.0f) * 35.0f, -15.0f, std::sin(-time * 1.0f) * 35.0f, 1600.0f);
            uboData.lights[2].color = glm::vec4(1.0f, 0.15f, 0.45f, 80.0f);

            // Light 3: Emerald Ambient Fill Light
            uboData.lights[3].position = glm::vec4(0.0f, std::sin(time * 0.7f) * 30.0f, 0.0f, 1200.0f);
            uboData.lights[3].color = glm::vec4(0.3f, 1.0f, 0.6f, 70.0f);

            // Post-Process params
            uboData.postProcessParams = glm::vec4(
                enablePostProcessing ? 1.0f : 0.0f, // Enable ACES
                1.3f,                               // Exposure
                0.45f,                              // Vignette Strength
                time                                // Time
            );

            sceneUbo.Update(uboData);

            // Render
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                frame->Clear(Sunshine::Color{ 0.04f, 0.04f, 0.07f, 1.0f });

                auto& cmd = frame->GetCommandContext();
                cmd.SetPipeline(pipeline);
                cmd.SetVertexBuffer(0, vertexBuffer);
                cmd.SetVertexBuffer(1, instanceBuffer);
                cmd.SetIndexBuffer(indexBuffer);
                cmd.SetUniformBuffer(0, sceneUbo);

                // SINGLE draw call for all instances!
                cmd.DrawIndexed(static_cast<uint32_t>(sphereIndices.size()), activeInstances);

                profiler.RecordDraw(static_cast<uint32_t>(sphereIndices.size() / 3), activeInstances);

                device.SubmitFrame(*frame);
            }

            profiler.EndFrame();

            // Update window title with live telemetry
            std::string title = profiler.GetFormattedTitle("Sunshine Benchmark");
            title += enablePostProcessing ? " | ACES: ON" : " | ACES: OFF";
            glfwSetWindowTitle(window, title.c_str());
        }

        std::cout << "[Benchmark] Exiting cleanly." << std::endl;
        const auto& finalStats = profiler.GetStats();
        std::cout << "\n================ [Benchmark Summary] ================\n"
                  << "  Total Frames Rendered: " << finalStats.totalFrames << "\n"
                  << "  Average FPS:           " << finalStats.fps << "\n"
                  << "  Average Frame Time:    " << finalStats.avgFrameTimeMs << " ms\n"
                  << "  Min Frame Time:        " << finalStats.minFrameTimeMs << " ms\n"
                  << "  Max Frame Time:        " << finalStats.maxFrameTimeMs << " ms\n"
                  << "  Triangles per Frame:   " << finalStats.triangleCount << "\n"
                  << "  Draw Calls per Frame:  " << finalStats.drawCalls << "\n"
                  << "======================================================\n" << std::endl;

    } catch (const Sunshine::GraphicsException& e) {
        std::cerr << "\n[Sunshine Fatal Error] " << e.what() << std::endl;
        if (!e.GetSuggestion().empty()) {
            std::cerr << "[Troubleshooting Tip] " << e.GetSuggestion() << std::endl;
        }
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "[Standard Exception] " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
