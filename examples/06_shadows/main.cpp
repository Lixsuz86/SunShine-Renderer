#include "sunshine/sunshine.hpp"
#include "shaders/shadow_shaders.spv.hpp"

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
constexpr uint32_t SHADOW_MAP_RES = 2048;

struct alignas(16) ShadowUBO {
    glm::mat4 lightSpaceMatrix;
    glm::mat4 modelMatrix;
};

struct alignas(16) SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 camPos;
    float padding0{0.0f};
    glm::vec3 lightDir;
    float lightIntensity;
    glm::vec3 lightColor;
    float ambientStrength;
};

// Orbit camera interaction state
struct CameraController {
    float yaw = 35.0f;
    float pitch = 20.0f;
    float distance = 4.0f;
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
} g_camera;

bool g_sunPaused = false;

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    (void)window;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_camera.isDragging = true;
            glfwGetCursorPos(window, &g_camera.lastMouseX, &g_camera.lastMouseY);
        } else if (action == GLFW_RELEASE) {
            g_camera.isDragging = false;
        }
    }
}

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (g_camera.isDragging) {
        float dx = static_cast<float>(xpos - g_camera.lastMouseX);
        float dy = static_cast<float>(ypos - g_camera.lastMouseY);

        g_camera.yaw += dx * 0.4f;
        g_camera.pitch = std::clamp(g_camera.pitch - dy * 0.4f, -85.0f, 85.0f);

        g_camera.lastMouseX = xpos;
        g_camera.lastMouseY = ypos;
    }
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    g_camera.distance = std::clamp(g_camera.distance - static_cast<float>(yoffset) * 0.4f, 1.2f, 12.0f);
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_SPACE) {
            g_sunPaused = !g_sunPaused;
            std::cout << "[Demo] Sun rotation " << (g_sunPaused ? "PAUSED" : "RESUMED") << std::endl;
        } else if (key == GLFW_KEY_R) {
            g_camera.yaw = 35.0f;
            g_camera.pitch = 20.0f;
            g_camera.distance = 4.0f;
            std::cout << "[Demo] Camera reset." << std::endl;
        }
    }
}

} // namespace

int main() {
    std::cout << "========================================================\n"
              << " Sunshine Renderer - Phase 7: Real-Time Soft Shadows\n"
              << " 2048x2048 Shadow Map + Cook-Torrance PBR + 3x3 PCF Filtering\n"
              << " Controls: Left Mouse Drag = Orbit | Scroll = Zoom\n"
              << "           Space = Pause Sun Orbit | R = Reset Camera\n"
              << "========================================================" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer - Phase 7: Shadow Mapping & PCF", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);

    try {
        Sunshine::DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "Sunshine 06_shadows";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // 1. Allocate 2048x2048 Offscreen Shadow Map
        std::cout << "[Demo] Creating 2048x2048 D32_SFLOAT ShadowMap..." << std::endl;
        Sunshine::ShadowMap shadowMap(device, SHADOW_MAP_RES, SHADOW_MAP_RES);

        // 2. Load 3D glTF Model (DamagedHelmet)
        std::cout << "[Demo] Loading 3D model asset..." << std::endl;
        auto model = Sunshine::Model::LoadGLTF(device, "assets/models/DamagedHelmet.glb");

        // 3. Create Ground Plane Geometry (20x20 units at Y = -1.2)
        std::vector<Sunshine::ModelVertex> groundVertices = {
            { {-10.0f, -1.2f, -10.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { { 10.0f, -1.2f, -10.0f}, {0.0f, 1.0f, 0.0f}, {8.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { { 10.0f, -1.2f,  10.0f}, {0.0f, 1.0f, 0.0f}, {8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { {-10.0f, -1.2f,  10.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }
        };
        std::vector<uint32_t> groundIndices = { 0, 1, 2, 2, 3, 0 };

        auto groundVBO = device.CreateVertexBuffer(groundVertices);
        auto groundIBO = device.CreateIndexBuffer(groundIndices);

        // 4. Create Ground Textures
        std::vector<uint32_t> groundPixels(256 * 256);
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                bool border = (x % 32 == 0) || (y % 32 == 0);
                bool check = ((x / 32) + (y / 32)) % 2 == 0;
                uint8_t c = border ? 45 : (check ? 130 : 105);
                groundPixels[y * 256 + x] = 0xFF000000 | (c << 16) | (c << 8) | c;
            }
        }
        auto groundAlbedo = device.CreateTexture2D(256, 256, groundPixels.data(), Sunshine::RHI::TextureFormat::RGBA8_UNORM);

        uint32_t flatNormalPixel = 0xFFFF8080; // (128, 128, 255) -> (0, 0, 1)
        auto defaultNormalMap = device.CreateTexture2D(1, 1, &flatNormalPixel);

        uint32_t groundMRPixel = 0xFF19CC00; // roughness ~ 0.8, metallic ~ 0.1
        auto groundMRMap = device.CreateTexture2D(1, 1, &groundMRPixel);

        Sunshine::RHI::SamplerDesc repeatSamplerDesc{};
        repeatSamplerDesc.addressModeU = Sunshine::RHI::AddressMode::Repeat;
        repeatSamplerDesc.addressModeV = Sunshine::RHI::AddressMode::Repeat;
        auto groundSampler = device.CreateSampler(repeatSamplerDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        auto defaultWhiteTex = device.CreateTexture2D(1, 1, &whitePixel);
        auto defaultSampler = device.CreateSampler();

        // 5. Create Depth-Only Shadow Pipeline
        Sunshine::RHI::PipelineDesc shadowPipeDesc{};
        shadowPipeDesc.debugName = "ShadowDepthPipeline";
        shadowPipeDesc.colorAttachmentCount = 0; // Pure depth pass

        shadowPipeDesc.vertexShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::ShadowDepthVertexSPIRV,
            .entryPoint = "main"
        });

        shadowPipeDesc.fragmentShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::ShadowDepthFragmentSPIRV,
            .entryPoint = "main"
        });

        shadowPipeDesc.vertexLayout.stride = sizeof(Sunshine::ModelVertex);
        shadowPipeDesc.vertexLayout.attributes = {
            { 0, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, position)), 0 },
            { 1, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, normal)), 0 },
            { 2, Sunshine::RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, texCoord)), 0 },
            { 3, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, tangent)), 0 }
        };

        shadowPipeDesc.depthStencil.depthTestEnable = true;
        shadowPipeDesc.depthStencil.depthWriteEnable = true;
        shadowPipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::LessOrEqual;

        // Hardware depth bias for shadow map rasterization to eliminate acne
        shadowPipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::None;
        shadowPipeDesc.rasterizer.depthBiasEnable = true;
        shadowPipeDesc.rasterizer.depthBiasConstantFactor = 2.0f;
        shadowPipeDesc.rasterizer.depthBiasSlopeFactor = 2.5f;

        shadowPipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex }
        };

        auto shadowPipeline = device.CreatePipeline(shadowPipeDesc, shadowMap);

        // 6. Create Main Scene PBR + Shadow Pipeline
        Sunshine::RHI::PipelineDesc scenePipeDesc{};
        scenePipeDesc.debugName = "PBRShadowPipeline";
        scenePipeDesc.colorAttachmentCount = 1;

        scenePipeDesc.vertexShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::PBRShadowVertexSPIRV,
            .entryPoint = "main"
        });

        scenePipeDesc.fragmentShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::PBRShadowFragmentSPIRV,
            .entryPoint = "main"
        });

        scenePipeDesc.vertexLayout = shadowPipeDesc.vertexLayout;

        scenePipeDesc.depthStencil.depthTestEnable = true;
        scenePipeDesc.depthStencil.depthWriteEnable = true;
        scenePipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::LessOrEqual;
        scenePipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::Back;

        scenePipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex | Sunshine::RHI::ShaderStage::Fragment },
            { 1, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 2, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 3, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 4, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment }
        };

        auto scenePipeline = device.CreatePipeline(scenePipeDesc);

        // 7. Uniform Buffers
        auto shadowUbo = device.CreateUniformBuffer<ShadowUBO>();
        auto sceneUbo = device.CreateUniformBuffer<SceneUBO>();

        auto startTime = std::chrono::high_resolution_clock::now();
        float sunAngle = 0.8f;

        // Telemetry profiler
        auto lastTime = startTime;
        uint32_t frameCount = 0;
        float fpsTimer = 0.0f;

        // Model base transform: centered, upright, scale 1.15
        glm::mat4 modelBaseMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.15f));

        std::cout << "[Demo] Starting rendering loop..." << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            if (!g_sunPaused) {
                sunAngle += dt * 0.45f;
            }

            frameCount++;
            fpsTimer += dt;
            if (fpsTimer >= 0.5f) {
                float fps = frameCount / fpsTimer;
                float frameMs = (fpsTimer / frameCount) * 1000.0f;
                std::string title = "Sunshine Renderer | Phase 7: Real-Time Soft Shadows (PCF) | " +
                                    std::to_string(static_cast<int>(fps)) + " FPS (" +
                                    std::to_string(frameMs).substr(0, 4) + " ms)";
                glfwSetWindowTitle(window, title.c_str());
                frameCount = 0;
                fpsTimer = 0.0f;
            }

            // Directional Light Setup (Sun orbit)
            glm::vec3 lightDir = glm::normalize(glm::vec3(std::cos(sunAngle) * 5.0f, 7.5f, std::sin(sunAngle) * 5.0f));
            glm::vec3 lightPos = lightDir * 12.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            float orthoBox = 5.5f;
            glm::mat4 lightProj = glm::ortho(-orthoBox, orthoBox, -orthoBox, orthoBox, 1.0f, 26.0f);
            lightProj[1][1] *= -1.0f; // Invert Vulkan clip-space Y

            glm::mat4 lightSpaceMatrix = lightProj * lightView;

            // Camera Setup
            float yawRad = glm::radians(g_camera.yaw);
            float pitchRad = glm::radians(g_camera.pitch);
            glm::vec3 camPos{
                g_camera.distance * std::cos(pitchRad) * std::sin(yawRad),
                g_camera.distance * std::sin(pitchRad),
                g_camera.distance * std::cos(pitchRad) * std::cos(yawRad)
            };
            camPos += g_camera.target;

            auto extent = swapchain.GetExtent();
            float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height > 0 ? extent.height : 1);
            glm::mat4 view = glm::lookAt(camPos, g_camera.target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
            proj[1][1] *= -1.0f;

            auto frame = device.BeginFrame(swapchain);
            if (!frame) continue;

            auto& cmd = frame->GetCommandContext();

            // =========================================================================
            // PASS 1: OFFSCREEN SHADOW MAP PASS
            // =========================================================================
            cmd.BeginShadowPass(shadowMap);
            cmd.SetPipeline(shadowPipeline);

            // 1a. Draw Model into Shadow Map
            auto renderShadowNode = [&](auto&& self, const std::shared_ptr<Sunshine::SceneNode>& node, const glm::mat4& parentMat) -> void {
                if (!node) return;
                glm::mat4 worldMat = parentMat * node->transform.GetMatrix();

                for (uint32_t submeshIdx : node->submeshIndices) {
                    if (submeshIdx >= model->GetSubmeshes().size()) continue;
                    const auto& submesh = model->GetSubmeshes()[submeshIdx];

                    ShadowUBO ubo{};
                    ubo.lightSpaceMatrix = lightSpaceMatrix;
                    ubo.modelMatrix = worldMat;
                    shadowUbo.Update(ubo);

                    cmd.SetUniformBuffer(0, shadowUbo);
                    cmd.SetVertexBuffer(0, submesh.vertexBuffer);
                    cmd.SetIndexBuffer(submesh.indexBuffer);
                    cmd.DrawIndexed(submesh.indexCount);
                }

                for (const auto& child : node->children) {
                    self(self, child, worldMat);
                }
            };

            for (const auto& rootNode : model->GetRootNodes()) {
                renderShadowNode(renderShadowNode, rootNode, modelBaseMatrix);
            }

            // 1b. Draw Ground Plane into Shadow Map
            {
                ShadowUBO ubo{};
                ubo.lightSpaceMatrix = lightSpaceMatrix;
                ubo.modelMatrix = glm::mat4(1.0f);
                shadowUbo.Update(ubo);

                cmd.SetUniformBuffer(0, shadowUbo);
                cmd.SetVertexBuffer(0, groundVBO);
                cmd.SetIndexBuffer(groundIBO);
                cmd.DrawIndexed(static_cast<uint32_t>(groundIndices.size()));
            }

            cmd.EndShadowPass();

            // =========================================================================
            // PASS 2: MAIN SCENE RENDER PASS (PBR + 3x3 PCF Shadows)
            // =========================================================================
            cmd.Clear(Sunshine::Color{0.07f, 0.08f, 0.10f, 1.0f});
            cmd.SetPipeline(scenePipeline);

            // Bind global shadow map depth texture and border-clamped sampler at binding 4
            cmd.SetTexture(4, shadowMap.GetTexture(), shadowMap.GetSampler());

            // 2a. Draw Ground Plane with PBR & Received Shadow
            {
                SceneUBO sceneData{};
                sceneData.view = view;
                sceneData.proj = proj;
                sceneData.model = glm::mat4(1.0f);
                sceneData.lightSpaceMatrix = lightSpaceMatrix;
                sceneData.camPos = camPos;
                sceneData.lightDir = lightDir;
                sceneData.lightIntensity = 3.2f;
                sceneData.lightColor = glm::vec3(1.0f, 0.95f, 0.88f); // Warm sunlight
                sceneData.ambientStrength = 0.08f;
                sceneUbo.Update(sceneData);

                cmd.SetUniformBuffer(0, sceneUbo);
                cmd.SetTexture(1, groundAlbedo, groundSampler);
                cmd.SetTexture(2, defaultNormalMap, defaultSampler);
                cmd.SetTexture(3, groundMRMap, defaultSampler);

                cmd.SetVertexBuffer(0, groundVBO);
                cmd.SetIndexBuffer(groundIBO);
                cmd.DrawIndexed(static_cast<uint32_t>(groundIndices.size()));
            }

            // 2b. Draw 3D Model with PBR, Normal Maps & Soft Shadows
            auto renderSceneNode = [&](auto&& self, const std::shared_ptr<Sunshine::SceneNode>& node, const glm::mat4& parentMat) -> void {
                if (!node) return;
                glm::mat4 worldMat = parentMat * node->transform.GetMatrix();

                for (uint32_t submeshIdx : node->submeshIndices) {
                    if (submeshIdx >= model->GetSubmeshes().size()) continue;
                    const auto& submesh = model->GetSubmeshes()[submeshIdx];
                    const auto* mat = model->GetMaterial(submesh.materialIndex);

                    SceneUBO sceneData{};
                    sceneData.view = view;
                    sceneData.proj = proj;
                    sceneData.model = worldMat;
                    sceneData.lightSpaceMatrix = lightSpaceMatrix;
                    sceneData.camPos = camPos;
                    sceneData.lightDir = lightDir;
                    sceneData.lightIntensity = 3.2f;
                    sceneData.lightColor = glm::vec3(1.0f, 0.95f, 0.88f);
                    sceneData.ambientStrength = 0.08f;
                    sceneUbo.Update(sceneData);

                    cmd.SetUniformBuffer(0, sceneUbo);

                    auto albedoTex = (mat && mat->baseColorTexture) ? mat->baseColorTexture : model->defaultWhiteTexture;
                    auto normalTex = (mat && mat->normalTexture) ? mat->normalTexture : model->defaultNormalTexture;
                    auto mrTex = (mat && mat->metallicRoughnessTexture) ? mat->metallicRoughnessTexture : model->defaultMetallicRoughnessTexture;
                    auto sampler = (mat && mat->sampler) ? mat->sampler : model->defaultSampler;

                    cmd.SetTexture(1, *albedoTex, *sampler);
                    cmd.SetTexture(2, *normalTex, *sampler);
                    cmd.SetTexture(3, *mrTex, *sampler);

                    cmd.SetVertexBuffer(0, submesh.vertexBuffer);
                    cmd.SetIndexBuffer(submesh.indexBuffer);
                    cmd.DrawIndexed(submesh.indexCount);
                }

                for (const auto& child : node->children) {
                    self(self, child, worldMat);
                }
            };

            for (const auto& rootNode : model->GetRootNodes()) {
                renderSceneNode(renderSceneNode, rootNode, modelBaseMatrix);
            }

            device.SubmitFrame(*frame);
        }

        device.WaitIdle();
    }
    catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "[Demo] Application exited cleanly." << std::endl;
    return 0;
}
