#include "sunshine/sunshine.hpp"
#include "shaders/ibl_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include <cmath>

using namespace Sunshine;

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

struct alignas(16) SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    glm::vec4 lightDir;
    glm::vec4 lightColor; // xyz = color, w = intensity
};

struct alignas(16) ModelUBO {
    glm::mat4 model;
    glm::vec4 baseColorFactor;
    glm::vec4 pbrFactors; // x: metallic, y: roughness, z: occlusion, w: flags (useTextures > 0.5)
};

// Global Camera Interaction State
struct CameraController {
    float yaw = 25.0f;
    float pitch = 15.0f;
    float distance = 5.0f;
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
} g_camera;

void MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_camera.isDragging = true;
            glfwGetCursorPos(window, &g_camera.lastMouseX, &g_camera.lastMouseY);
        } else if (action == GLFW_RELEASE) {
            g_camera.isDragging = false;
        }
    }
}

void CursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (g_camera.isDragging) {
        float dx = static_cast<float>(xpos - g_camera.lastMouseX);
        float dy = static_cast<float>(ypos - g_camera.lastMouseY);

        g_camera.yaw += dx * 0.4f;
        g_camera.pitch = std::clamp(g_camera.pitch - dy * 0.4f, -85.0f, 85.0f);

        g_camera.lastMouseX = xpos;
        g_camera.lastMouseY = ypos;
    }
}

void ScrollCallback(GLFWwindow*, double, double yoffset) {
    g_camera.distance = std::clamp(g_camera.distance - static_cast<float>(yoffset) * 0.4f, 1.5f, 15.0f);
}

// Procedural UV Sphere Generator
void GenerateSphere(float radius, uint32_t xSegments, uint32_t ySegments,
                    std::vector<ModelVertex>& outVertices, std::vector<uint32_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float PI = 3.14159265358979323846f;

    for (uint32_t y = 0; y <= ySegments; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(ySegments);
        float theta = v * PI;

        for (uint32_t x = 0; x <= xSegments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(xSegments);
            float phi = u * 2.0f * PI;

            float xPos = std::cos(phi) * std::sin(theta);
            float yPos = std::cos(theta);
            float zPos = std::sin(phi) * std::sin(theta);

            ModelVertex vert{};
            vert.position = glm::vec3(xPos, yPos, zPos) * radius;
            vert.normal = glm::normalize(glm::vec3(xPos, yPos, zPos));
            vert.texCoord = glm::vec2(u, v);

            // Compute tangent vector
            glm::vec3 tangent(-std::sin(phi), 0.0f, std::cos(phi));
            vert.tangent = glm::vec4(glm::normalize(tangent), 1.0f);

            outVertices.push_back(vert);
        }
    }

    for (uint32_t y = 0; y < ySegments; ++y) {
        for (uint32_t x = 0; x < xSegments; ++x) {
            uint32_t i0 = y * (xSegments + 1) + x;
            uint32_t i1 = (y + 1) * (xSegments + 1) + x;
            uint32_t i2 = (y + 1) * (xSegments + 1) + (x + 1);
            uint32_t i3 = y * (xSegments + 1) + (x + 1);

            outIndices.push_back(i0);
            outIndices.push_back(i1);
            outIndices.push_back(i2);

            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
    }
}

// Cube vertices for skybox background rendering (CW/CCW agnostic with CullMode::None)
const std::vector<glm::vec3> SkyboxCubeVertices = {
    // -Z
    { -1.0f,  1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f, -1.0f }, {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
    // -X
    { -1.0f, -1.0f,  1.0f }, { -1.0f, -1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
    { -1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f,  1.0f }, { -1.0f, -1.0f,  1.0f },
    // +X
    {  1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f,  1.0f }, {  1.0f,  1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, {  1.0f,  1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
    // +Z
    { -1.0f, -1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f }, {  1.0f,  1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f }, { -1.0f, -1.0f,  1.0f },
    // +Y
    { -1.0f,  1.0f, -1.0f }, {  1.0f,  1.0f, -1.0f }, {  1.0f,  1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f, -1.0f },
    // -Y
    { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
    {  1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f }
};

struct SphereMaterial {
    std::string name;
    glm::vec3 color;
    float metallic;
    float roughness;
};

} // anonymous namespace

int main() {
    std::cout << "\n======================================================================\n"
              << "    Sunshine Renderer — Phase 10: Image-Based Lighting (IBL) & Skybox\n"
              << "======================================================================\n"
              << "  [Key Features Tested]:\n"
              << "    * 360 HDRI Equirectangular to Cubemap Projection\n"
              << "    * Diffuse Irradiance Cubemap (Hemispherical Convolution)\n"
              << "    * Specular Pre-filtered Environment Map (Roughness Mipmaps)\n"
              << "    * BRDF 2D Integration Look-Up Table (Split-Sum Approximation)\n"
              << "    * Full Cook-Torrance Microfacet PBR + IBL Reflections\n"
              << "    * Dynamic Skybox with Depth Lequal Far-Plane Rendering\n"
              << "  [Interactive Controls]:\n"
              << "    [Left Mouse Drag] : Orbit Camera Around Scene\n"
              << "    [Mouse Scroll]    : Zoom In / Out\n"
              << "    [H]               : Toggle Sphere Gallery vs glTF DamagedHelmet\n"
              << "    [1]               : Toggle Image-Based Lighting (IBL) On/Off\n"
              << "    [2]               : Toggle Direct Sun Light On/Off\n"
              << "    [R]               : Reset Camera Position\n"
              << "    [ESC]             : Exit Demo\n"
              << "======================================================================\n" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer — Phase 10: IBL & HDRI Skybox", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    try {
        DeviceDesc deviceDesc{};
        deviceDesc.enableValidation = true;
        deviceDesc.applicationName = "09_ibl_skybox";

        Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        SwapchainContext swapchain(device, window);

        // ---------------------------------------------------------------------
        // 1. Initialize IBL Environment Map (.hdr file or procedural fallback)
        // ---------------------------------------------------------------------
        std::shared_ptr<EnvironmentMap> envMap;
        try {
            envMap = EnvironmentMap::LoadHDR(device, "assets/textures/hdr/sky.hdr");
        } catch (...) {
            std::cout << "[Demo] Loading procedural atmospheric HDR sky..." << std::endl;
            envMap = EnvironmentMap::CreateProcedural(device);
        }

        // ---------------------------------------------------------------------
        // 2. Build Skybox Pipeline
        // ---------------------------------------------------------------------
        RHI::PipelineDesc skyboxPipeDesc{};
        skyboxPipeDesc.debugName = "SkyboxPipeline";

        skyboxPipeDesc.vertexShader = device.CreateShader({
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = Shaders::SkyboxVertexSPIRV,
            .entryPoint = "main"
        });

        skyboxPipeDesc.fragmentShader = device.CreateShader({
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = Shaders::SkyboxFragmentSPIRV,
            .entryPoint = "main"
        });

        skyboxPipeDesc.vertexLayout.stride = sizeof(glm::vec3);
        skyboxPipeDesc.vertexLayout.attributes = {
            { 0, RHI::VertexFormat::Float3, 0, 0 }
        };

        // Skybox renders at far plane: depth compare LessOrEqual, depth write disabled
        skyboxPipeDesc.depthStencil.depthTestEnable = true;
        skyboxPipeDesc.depthStencil.depthWriteEnable = false;
        skyboxPipeDesc.depthStencil.depthCompareOp = RHI::CompareOp::LessOrEqual;
        skyboxPipeDesc.rasterizer.cullMode = RHI::CullMode::None;

        skyboxPipeDesc.descriptorBindings = {
            { 0, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Vertex },
            { 1, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }
        };

        auto skyboxPipeline = device.CreatePipeline(skyboxPipeDesc);
        auto skyboxVertexBuffer = device.CreateVertexBuffer(SkyboxCubeVertices);

        // ---------------------------------------------------------------------
        // 3. Build PBR + IBL Pipeline
        // ---------------------------------------------------------------------
        RHI::PipelineDesc pbrPipeDesc{};
        pbrPipeDesc.debugName = "PBRIBLPipeline";

        pbrPipeDesc.vertexShader = device.CreateShader({
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = Shaders::PBRIBLVertexSPIRV,
            .entryPoint = "main"
        });

        pbrPipeDesc.fragmentShader = device.CreateShader({
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = Shaders::PBRIBLFragmentSPIRV,
            .entryPoint = "main"
        });

        pbrPipeDesc.vertexLayout.stride = sizeof(ModelVertex);
        pbrPipeDesc.vertexLayout.attributes = {
            { 0, RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(ModelVertex, position)), 0 },
            { 1, RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(ModelVertex, normal)), 0 },
            { 2, RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(ModelVertex, texCoord)), 0 },
            { 3, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(ModelVertex, tangent)), 0 }
        };

        pbrPipeDesc.depthStencil.depthTestEnable = true;
        pbrPipeDesc.depthStencil.depthWriteEnable = true;
        pbrPipeDesc.depthStencil.depthCompareOp = RHI::CompareOp::Less;
        pbrPipeDesc.rasterizer.cullMode = RHI::CullMode::Back;

        pbrPipeDesc.descriptorBindings = {
            { 0, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment },
            { 1, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment },
            { 2, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // albedo
            { 3, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // normal
            { 4, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // metallic-roughness
            { 5, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // ao
            { 6, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // irradiance cubemap
            { 7, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }, // prefiltered cubemap
            { 8, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }  // brdf lut
        };

        auto pbrPipeline = device.CreatePipeline(pbrPipeDesc);

        // ---------------------------------------------------------------------
        // 4. Geometry & Materials
        // ---------------------------------------------------------------------
        // Generate high-tessellation UV sphere (64x64 segments)
        std::vector<ModelVertex> sphereVertices;
        std::vector<uint32_t> sphereIndices;
        GenerateSphere(0.45f, 64, 64, sphereVertices, sphereIndices);

        auto sphereVertexBuffer = device.CreateVertexBuffer(sphereVertices);
        auto sphereIndexBuffer = device.CreateIndexBuffer(sphereIndices);
        uint32_t sphereIndexCount = static_cast<uint32_t>(sphereIndices.size());

        // Default fallback 1x1 textures for sphere rendering
        uint32_t whitePixel = 0xFFFFFFFF;
        uint32_t flatNormalPixel = 0xFFFF8080; // (0.5, 0.5, 1.0)
        auto defaultWhiteTex = device.CreateTexture2D(1, 1, &whitePixel);
        auto defaultNormalTex = device.CreateTexture2D(1, 1, &flatNormalPixel);
        auto defaultSampler = device.CreateSampler({});

        // PBR Material Gallery (Varying Metallic & Roughness)
        std::vector<SphereMaterial> materials = {
            // Row 1: Metals (Metallic = 1.0)
            { "Gold",           glm::vec3(1.000f, 0.765f, 0.336f), 1.0f, 0.05f },
            { "Silver",         glm::vec3(0.972f, 0.960f, 0.915f), 1.0f, 0.20f },
            { "Copper",         glm::vec3(0.955f, 0.637f, 0.538f), 1.0f, 0.40f },
            { "Titanium",       glm::vec3(0.542f, 0.497f, 0.449f), 1.0f, 0.65f },
            { "Rough Iron",     glm::vec3(0.560f, 0.570f, 0.580f), 1.0f, 0.90f },

            // Row 2: Dielectrics (Metallic = 0.0)
            { "Clear Plastic",  glm::vec3(0.950f, 0.100f, 0.100f), 0.0f, 0.05f },
            { "Smooth Emerald", glm::vec3(0.100f, 0.850f, 0.350f), 0.0f, 0.20f },
            { "Satin Ceramic",  glm::vec3(0.200f, 0.550f, 0.950f), 0.0f, 0.40f },
            { "Matte Clay",     glm::vec3(0.850f, 0.650f, 0.450f), 0.0f, 0.65f },
            { "Chalk / Stone",  glm::vec3(0.900f, 0.900f, 0.900f), 0.0f, 0.90f }
        };

        // Try loading glTF 2.0 DamagedHelmet model
        std::shared_ptr<Model> helmetModel;
        try {
            helmetModel = Model::LoadGLTF(device, "assets/models/DamagedHelmet.glb");
            std::cout << "[Demo] glTF model 'DamagedHelmet.glb' loaded successfully!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[Demo] glTF model not found or failed to load: " << e.what() << std::endl;
        }

        // ---------------------------------------------------------------------
        // 5. Uniform Buffers & State
        // ---------------------------------------------------------------------
        auto sceneUbo = device.CreateUniformBuffer<SceneUBO>();
        auto modelUbo = device.CreateUniformBuffer<ModelUBO>();

        Camera camera(45.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
        Profiler profiler;

        bool showModel = false;
        bool iblEnabled = true;
        bool directLightEnabled = true;
        bool hWasPressed = false;
        bool oneWasPressed = false;
        bool twoWasPressed = false;

        std::cout << "\n[Demo] Entering render loop...\n" << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                g_camera.yaw = 25.0f;
                g_camera.pitch = 15.0f;
                g_camera.distance = 5.0f;
            }

            // [H] Toggle Model vs Sphere Gallery
            bool hPressed = (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS);
            if (hPressed && !hWasPressed && helmetModel) {
                showModel = !showModel;
                std::cout << "[Demo] Display mode switched to: " << (showModel ? "glTF DamagedHelmet" : "PBR Sphere Gallery") << std::endl;
            }
            hWasPressed = hPressed;

            // [1] Toggle IBL
            bool onePressed = (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS);
            if (onePressed && !oneWasPressed) {
                iblEnabled = !iblEnabled;
                std::cout << "[Demo] Image-Based Lighting (IBL): " << (iblEnabled ? "ENABLED" : "DISABLED") << std::endl;
            }
            oneWasPressed = onePressed;

            // [2] Toggle Direct Sun Light
            bool twoPressed = (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS);
            if (twoPressed && !twoWasPressed) {
                directLightEnabled = !directLightEnabled;
                std::cout << "[Demo] Direct Sun Light: " << (directLightEnabled ? "ENABLED" : "DISABLED") << std::endl;
            }
            twoWasPressed = twoPressed;

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue;
            }

            profiler.BeginFrame();

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

            // Update Scene UBO
            SceneUBO sceneData{};
            sceneData.view = camera.GetViewMatrix();
            sceneData.proj = camera.GetProjectionMatrix();
            sceneData.camPos = glm::vec4(camPos, 1.0f);
            sceneData.lightDir = glm::vec4(glm::normalize(glm::vec3(0.4f, 0.45f, 0.8f)), 0.0f);
            sceneData.lightColor = directLightEnabled ? glm::vec4(1.0f, 0.95f, 0.85f, 3.5f) : glm::vec4(0.0f);
            sceneUbo.Update(sceneData);

            // Render Frame
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                frame->Clear(Color{ 0.02f, 0.02f, 0.03f, 1.0f });

                auto& cmd = frame->GetCommandContext();

                // -------------------------------------------------------------
                // 1. Draw PBR Objects (Spheres or glTF Helmet)
                // -------------------------------------------------------------
                cmd.SetPipeline(pbrPipeline);
                cmd.SetUniformBuffer(0, sceneUbo);

                // Bind IBL Textures
                auto skyboxTex = envMap->GetSkyboxTexture();
                auto irradTex = iblEnabled ? envMap->GetIrradianceTexture() : defaultWhiteTex;
                auto prefTex = iblEnabled ? envMap->GetPrefilteredTexture() : defaultWhiteTex;
                auto brdfLut = envMap->GetBRDFLUTTexture();
                auto cubeSampler = envMap->GetCubemapSampler();
                auto lutSampler = envMap->GetLUTSampler();

                cmd.SetTexture(6, irradTex, cubeSampler);
                cmd.SetTexture(7, prefTex, cubeSampler);
                cmd.SetTexture(8, brdfLut, lutSampler);

                if (!showModel || !helmetModel) {
                    // Render 2x5 PBR Sphere Grid
                    cmd.SetVertexBuffer(sphereVertexBuffer);
                    cmd.SetIndexBuffer(sphereIndexBuffer);

                    // 2 rows, 5 columns
                    const int cols = 5;
                    const float spacingX = 1.15f;
                    const float spacingY = 1.25f;

                    for (size_t i = 0; i < materials.size(); ++i) {
                        int row = static_cast<int>(i / cols);
                        int col = static_cast<int>(i % cols);

                        float posX = (col - 2.0f) * spacingX;
                        float posY = (row == 0 ? 0.5f : -0.5f) * spacingY;

                        glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, 0.0f));

                        ModelUBO modelData{};
                        modelData.model = modelMat;
                        modelData.baseColorFactor = glm::vec4(materials[i].color, 1.0f);
                        modelData.pbrFactors = glm::vec4(materials[i].metallic, materials[i].roughness, 1.0f, 0.0f); // useTextures = 0.0
                        modelUbo.Update(modelData);

                        cmd.SetUniformBuffer(1, modelUbo);
                        cmd.SetTexture(2, defaultWhiteTex, defaultSampler);
                        cmd.SetTexture(3, defaultNormalTex, defaultSampler);
                        cmd.SetTexture(4, defaultWhiteTex, defaultSampler);
                        cmd.SetTexture(5, defaultWhiteTex, defaultSampler);

                        cmd.DrawIndexed(sphereIndexCount);
                        profiler.RecordDraw(sphereIndexCount / 3, 1);
                    }
                } else {
                    // Render glTF DamagedHelmet
                    glm::mat4 baseTransform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    baseTransform = glm::scale(baseTransform, glm::vec3(1.2f));

                    auto renderNode = [&](auto& self, const std::shared_ptr<SceneNode>& node, const glm::mat4& parentMat) -> void {
                        if (!node) return;
                        glm::mat4 worldMat = parentMat * node->transform.GetMatrix();

                        for (uint32_t submeshIdx : node->submeshIndices) {
                            if (submeshIdx >= helmetModel->GetSubmeshes().size()) continue;
                            const auto& submesh = helmetModel->GetSubmeshes()[submeshIdx];
                            const auto* material = helmetModel->GetMaterial(submesh.materialIndex);

                            ModelUBO modelData{};
                            modelData.model = worldMat;
                            if (material) {
                                modelData.baseColorFactor = material->baseColorFactor;
                                modelData.pbrFactors = glm::vec4(
                                    material->metallicFactor,
                                    material->roughnessFactor,
                                    material->occlusionStrength,
                                    1.0f // useTextures = 1.0
                                );
                            } else {
                                modelData.baseColorFactor = glm::vec4(1.0f);
                                modelData.pbrFactors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
                            }
                            modelUbo.Update(modelData);

                            cmd.SetUniformBuffer(1, modelUbo);

                            auto albedoTex = (material && material->baseColorTexture) ? material->baseColorTexture : helmetModel->defaultWhiteTexture;
                            auto normalTex = (material && material->normalTexture) ? material->normalTexture : helmetModel->defaultNormalTexture;
                            auto mrTex = (material && material->metallicRoughnessTexture) ? material->metallicRoughnessTexture : helmetModel->defaultMetallicRoughnessTexture;
                            auto aoTex = (material && material->occlusionTexture) ? material->occlusionTexture : helmetModel->defaultWhiteTexture;
                            auto sampler = (material && material->sampler) ? material->sampler : helmetModel->defaultSampler;

                            cmd.SetTexture(2, *albedoTex, *sampler);
                            cmd.SetTexture(3, *normalTex, *sampler);
                            cmd.SetTexture(4, *mrTex, *sampler);
                            cmd.SetTexture(5, *aoTex, *sampler);

                            cmd.SetVertexBuffer(submesh.vertexBuffer);
                            cmd.SetIndexBuffer(submesh.indexBuffer);
                            cmd.DrawIndexed(submesh.indexCount);

                            profiler.RecordDraw(submesh.indexCount / 3, 1);
                        }

                        for (const auto& child : node->children) {
                            self(self, child, worldMat);
                        }
                    };

                    for (const auto& rootNode : helmetModel->GetRootNodes()) {
                        renderNode(renderNode, rootNode, baseTransform);
                    }
                }

                // -------------------------------------------------------------
                // 2. Draw Skybox (Leq Depth Test at Far Clip Plane)
                // -------------------------------------------------------------
                cmd.SetPipeline(skyboxPipeline);
                cmd.SetUniformBuffer(0, sceneUbo);
                cmd.SetTexture(1, skyboxTex, cubeSampler);
                cmd.SetVertexBuffer(skyboxVertexBuffer);
                cmd.Draw(36);
                profiler.RecordDraw(12, 1);

                device.SubmitFrame(*frame);
            }

            profiler.EndFrame();

            // Telemetry in Window Title
            static int frameCounter = 0;
            if (++frameCounter % 15 == 0) {
                const auto& stats = profiler.GetStats();
                std::string title = "Sunshine Renderer — Phase 10: IBL & Skybox | FPS: " +
                    std::to_string(static_cast<int>(std::round(stats.fps))) +
                    " | Frame: " + std::to_string(stats.avgFrameTimeMs).substr(0, 4) + " ms" +
                    " | DrawCalls: " + std::to_string(stats.drawCalls) +
                    " | Triangles: " + std::to_string(stats.triangleCount) +
                    " | Mode: " + (showModel ? "glTF Helmet" : "Sphere Grid") +
                    " | IBL: " + (iblEnabled ? "ON" : "OFF") +
                    " | Sun: " + (directLightEnabled ? "ON" : "OFF");
                glfwSetWindowTitle(window, title.c_str());
            }
        }

        device.WaitIdle();

    } catch (const GraphicsException& e) {
        std::cerr << "\n[GraphicsException Caught]: " << e.what() << "\n";
        if (!e.GetSuggestion().empty()) {
            std::cerr << "  -> Actionable Suggestion: " << e.GetSuggestion() << "\n";
        }
        glfwTerminate();
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "\n[Standard Exception Caught]: " << e.what() << "\n";
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "\n[Demo] Clean shutdown completed successfully." << std::endl;
    return 0;
}
