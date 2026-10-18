#include "sunshine/sunshine.hpp"
#include "shaders/pbr_model_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

struct Light {
    glm::vec4 position; // xyz = world pos, w = intensity
    glm::vec4 color;    // rgb = color, w = radius
};

struct alignas(16) SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    Light lights[4];
};

struct alignas(16) ModelUBO {
    glm::mat4 model;
    glm::vec4 baseColorFactor;
    glm::vec4 pbrFactors; // x: metallic, y: roughness, z: normalScale, w: occlusionStrength
};

// Global camera interaction state
struct CameraController {
    float yaw = 25.0f;
    float pitch = 15.0f;
    float distance = 3.2f;
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
} g_camera;

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
    g_camera.distance = std::clamp(g_camera.distance - static_cast<float>(yoffset) * 0.3f, 1.0f, 12.0f);
}

} // anonymous namespace

int main() {
    std::cout << "\n======================================================================\n"
              << "    Sunshine Renderer — Phase 6: glTF 2.0 PBR Model Demo\n"
              << "======================================================================\n"
              << "  [Key Features Tested]:\n"
              << "    * glTF 2.0 Asset Loading (tinygltf integration)\n"
              << "    * Hierarchical Scene Graph (SceneNode & Transform)\n"
              << "    * Tangent-Space Normal Mapping (TBN Matrix in GLSL 450)\n"
              << "    * PBR Texture Maps (BaseColor, Normal, Metallic-Roughness, AO)\n"
              << "    * Multi-Submesh Rendering with Dynamic Materials\n"
              << "  [Interactive Controls]:\n"
              << "    [Left Mouse Drag] : Orbit Camera Around Model\n"
              << "    [Mouse Scroll]    : Zoom In / Out\n"
              << "    [SPACE]           : Pause / Resume Dynamic Light Animation\n"
              << "    [R]               : Reset Camera Position\n"
              << "    [ESC]             : Exit Demo\n"
              << "======================================================================\n" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer — Phase 6: glTF 2.0 PBR Model", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    try {
        Sunshine::DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "05_gltf_model";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // Load glTF 2.0 Model
        std::cout << "[Demo] Loading 3D model asset..." << std::endl;
        auto model = Sunshine::Model::LoadGLTF(device, "assets/models/DamagedHelmet.glb");

        // Pipeline for glTF PBR Model
        Sunshine::RHI::PipelineDesc pipeDesc{};
        pipeDesc.debugName = "GLTFPBRPipeline";

        pipeDesc.vertexShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::PBRModelVertexSPIRV,
            .entryPoint = "main"
        });

        pipeDesc.fragmentShader = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::PBRModelFragmentSPIRV,
            .entryPoint = "main"
        });

        pipeDesc.vertexLayout.stride = sizeof(Sunshine::ModelVertex);
        pipeDesc.vertexLayout.attributes = {
            { 0, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, position)), 0 },
            { 1, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, normal)), 0 },
            { 2, Sunshine::RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, texCoord)), 0 },
            { 3, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, tangent)), 0 }
        };

        pipeDesc.depthStencil.depthTestEnable = true;
        pipeDesc.depthStencil.depthWriteEnable = true;
        pipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::Less;

        pipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::Back;

        pipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex | Sunshine::RHI::ShaderStage::Fragment },
            { 1, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex | Sunshine::RHI::ShaderStage::Fragment },
            { 2, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 3, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 4, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment }
        };

        auto pipeline = device.CreatePipeline(pipeDesc);

        auto sceneUbo = device.CreateUniformBuffer<SceneUBO>();
        auto modelUbo = device.CreateUniformBuffer<ModelUBO>();

        Sunshine::Camera camera(45.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
        Sunshine::Profiler profiler;

        auto startTime = std::chrono::high_resolution_clock::now();
        bool lightAnimationPaused = false;
        bool spaceWasPressed = false;
        float lightTimeAccum = 0.0f;
        auto lastTime = startTime;

        std::cout << "[Demo] Entering render loop..." << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                g_camera.yaw = 25.0f;
                g_camera.pitch = 15.0f;
                g_camera.distance = 3.2f;
            }

            bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
            if (spacePressed && !spaceWasPressed) {
                lightAnimationPaused = !lightAnimationPaused;
            }
            spaceWasPressed = spacePressed;

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue;
            }

            profiler.BeginFrame();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float delta = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (!lightAnimationPaused) {
                lightTimeAccum += delta;
            }

            camera.SetAspectRatio(static_cast<float>(currentWidth) / static_cast<float>(currentHeight));

            // Orbit camera computation
            float radYaw = glm::radians(g_camera.yaw);
            float radPitch = glm::radians(g_camera.pitch);
            glm::vec3 camPos = glm::vec3(
                g_camera.distance * std::cos(radPitch) * std::sin(radYaw),
                g_camera.distance * std::sin(radPitch),
                g_camera.distance * std::cos(radPitch) * std::cos(radYaw)
            );
            camera.LookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f));

            // Update Scene UBO (Lights & Camera)
            SceneUBO sceneData{};
            sceneData.view = camera.GetViewMatrix();
            sceneData.proj = camera.GetProjectionMatrix();
            sceneData.camPos = glm::vec4(camPos, 1.0f);

            // 4 Dynamic Colored Lights
            // Light 0: Warm Key Sun Light
            sceneData.lights[0].position = glm::vec4(std::cos(lightTimeAccum * 0.8f) * 4.0f, 2.8f, std::sin(lightTimeAccum * 0.8f) * 4.0f, 25.0f);
            sceneData.lights[0].color = glm::vec4(1.0f, 0.90f, 0.75f, 10.0f);

            // Light 1: Neon Cyan Rim Light
            sceneData.lights[1].position = glm::vec4(std::sin(lightTimeAccum * 1.2f) * 3.5f, -1.0f, std::cos(lightTimeAccum * 1.2f) * 3.5f, 18.0f);
            sceneData.lights[1].color = glm::vec4(0.15f, 0.90f, 1.0f, 8.0f);

            // Light 2: Warm Amber Accent Light
            sceneData.lights[2].position = glm::vec4(std::cos(-lightTimeAccum * 1.1f) * 3.0f, 1.5f, std::sin(-lightTimeAccum * 1.1f) * 3.0f, 15.0f);
            sceneData.lights[2].color = glm::vec4(1.0f, 0.35f, 0.15f, 8.0f);

            // Light 3: Cool Violet Top Light
            sceneData.lights[3].position = glm::vec4(0.0f, 4.0f, 0.0f, 10.0f);
            sceneData.lights[3].color = glm::vec4(0.6f, 0.3f, 0.9f, 6.0f);

            sceneUbo.Update(sceneData);

            // Render
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                frame->Clear(Sunshine::Color{ 0.05f, 0.06f, 0.09f, 1.0f });

                auto& cmd = frame->GetCommandContext();
                cmd.SetPipeline(pipeline);
                cmd.SetUniformBuffer(0, sceneUbo);

                // Base model transform (rotate slightly over time)
                glm::mat4 baseTransform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

                // Recursive node rendering helper
                auto renderNode = [&](auto& self, const std::shared_ptr<Sunshine::SceneNode>& node, const glm::mat4& parentMat) -> void {
                    if (!node) return;
                    glm::mat4 worldMat = parentMat * node->transform.GetMatrix();

                    for (uint32_t submeshIdx : node->submeshIndices) {
                        if (submeshIdx >= model->GetSubmeshes().size()) continue;
                        const auto& submesh = model->GetSubmeshes()[submeshIdx];
                        const auto* material = model->GetMaterial(submesh.materialIndex);

                        ModelUBO modelData{};
                        modelData.model = worldMat;
                        if (material) {
                            modelData.baseColorFactor = material->baseColorFactor;
                            modelData.pbrFactors = glm::vec4(
                                material->metallicFactor,
                                material->roughnessFactor,
                                material->normalScale,
                                material->occlusionStrength
                            );
                        } else {
                            modelData.baseColorFactor = glm::vec4(1.0f);
                            modelData.pbrFactors = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                        }
                        modelUbo.Update(modelData);

                        cmd.SetUniformBuffer(1, modelUbo);

                        // Bind PBR Textures
                        auto albedoTex = (material && material->baseColorTexture) ? material->baseColorTexture : model->defaultWhiteTexture;
                        auto normalTex = (material && material->normalTexture) ? material->normalTexture : model->defaultNormalTexture;
                        auto mrTex = (material && material->metallicRoughnessTexture) ? material->metallicRoughnessTexture : model->defaultMetallicRoughnessTexture;
                        auto sampler = (material && material->sampler) ? material->sampler : model->defaultSampler;

                        cmd.SetTexture(2, *albedoTex, *sampler);
                        cmd.SetTexture(3, *normalTex, *sampler);
                        cmd.SetTexture(4, *mrTex, *sampler);

                        cmd.SetVertexBuffer(submesh.vertexBuffer);
                        cmd.SetIndexBuffer(submesh.indexBuffer);
                        cmd.DrawIndexed(submesh.indexCount);

                        profiler.RecordDraw(submesh.indexCount / 3, 1);
                    }

                    for (const auto& child : node->children) {
                        self(self, child, worldMat);
                    }
                };

                for (const auto& rootNode : model->GetRootNodes()) {
                    renderNode(renderNode, rootNode, baseTransform);
                }

                device.SubmitFrame(*frame);
            }

            profiler.EndFrame();

            std::string title = profiler.GetFormattedTitle("Sunshine glTF PBR Demo");
            glfwSetWindowTitle(window, title.c_str());
        }

        std::cout << "[Demo] Exiting cleanly." << std::endl;

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
