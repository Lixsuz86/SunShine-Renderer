#include "sunshine/sunshine.hpp"
#include "shaders/bloom_shaders.spv.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numbers>

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

struct alignas(16) SceneUniforms {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::vec3 camPos;
    float padding0{0.0f};
    glm::vec3 lightDir;
    float lightIntensity;
    glm::vec3 lightColor;
    float ambientStrength;
    glm::vec4 emissiveFactor{0.0f}; // rgb = color, w = intensity
};

struct alignas(16) BrightParams {
    float threshold{1.0f};
    float softKnee{0.5f};
    glm::vec2 padding{0.0f};
};

struct alignas(16) BlurParams {
    glm::vec2 texelSize{1.0f / 640.0f, 1.0f / 360.0f};
    glm::vec2 padding{0.0f};
};

struct alignas(16) CompositeParams {
    float bloomIntensity{1.5f};
    float exposure{1.0f};
    int32_t bloomEnabled{1};
    float padding{0.0f};
};

// Global camera and interaction state
struct CameraController {
    float yaw = 25.0f;
    float pitch = 15.0f;
    float distance = 4.2f;
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
} g_camera;

bool g_animationPaused = false;
bool g_bloomEnabled = true;
float g_bloomIntensity = 1.6f;

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
    g_camera.distance = std::clamp(g_camera.distance - static_cast<float>(yoffset) * 0.4f, 1.5f, 15.0f);
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_B) {
            g_bloomEnabled = !g_bloomEnabled;
            std::cout << "[Demo] Bloom " << (g_bloomEnabled ? "ENABLED" : "DISABLED") << std::endl;
        } else if (key == GLFW_KEY_SPACE) {
            g_animationPaused = !g_animationPaused;
            std::cout << "[Demo] Animation " << (g_animationPaused ? "PAUSED" : "RESUMED") << std::endl;
        } else if (key == GLFW_KEY_UP || key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
            g_bloomIntensity = std::min(g_bloomIntensity + 0.25f, 5.0f);
            std::cout << "[Demo] Bloom Intensity: " << g_bloomIntensity << std::endl;
        } else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) {
            g_bloomIntensity = std::max(g_bloomIntensity - 0.25f, 0.0f);
            std::cout << "[Demo] Bloom Intensity: " << g_bloomIntensity << std::endl;
        } else if (key == GLFW_KEY_R) {
            g_camera.yaw = 25.0f;
            g_camera.pitch = 15.0f;
            g_camera.distance = 4.2f;
            std::cout << "[Demo] Camera reset." << std::endl;
        }
    }
}

// Procedural UV Sphere generator for glowing neon orbs
std::pair<std::vector<Sunshine::ModelVertex>, std::vector<uint32_t>> CreateSphere(float radius, uint32_t sectors, uint32_t stacks) {
    std::vector<Sunshine::ModelVertex> vertices;
    std::vector<uint32_t> indices;

    float const R = 1.0f / static_cast<float>(stacks - 1);
    float const S = 1.0f / static_cast<float>(sectors - 1);

    for (uint32_t r = 0; r < stacks; ++r) {
        for (uint32_t s = 0; s < sectors; ++s) {
            float const y = std::sin(-std::numbers::pi_v<float> * 0.5f + std::numbers::pi_v<float> * static_cast<float>(r) * R);
            float const x = std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(s) * S) * std::sin(std::numbers::pi_v<float> * static_cast<float>(r) * R);
            float const z = std::sin(2.0f * std::numbers::pi_v<float> * static_cast<float>(s) * S) * std::sin(std::numbers::pi_v<float> * static_cast<float>(r) * R);

            Sunshine::ModelVertex v{};
            v.position = glm::vec3(x, y, z) * radius;
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.texCoord = glm::vec2(static_cast<float>(s) * S, static_cast<float>(r) * R);
            v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            vertices.push_back(v);
        }
    }

    for (uint32_t r = 0; r < stacks - 1; ++r) {
        for (uint32_t s = 0; s < sectors - 1; ++s) {
            uint32_t cur = r * sectors + s;
            uint32_t next = (r + 1) * sectors + s;

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(cur + 1);

            indices.push_back(cur + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    return {vertices, indices};
}

} // namespace

int main() {
    std::cout << "========================================================\n"
              << " Sunshine Renderer - Phase 8: RenderGraph & HDR Bloom\n"
              << " DAG Pass Architecture + Multi-Target Pooling + Dual-Kawase\n"
              << " Controls: Left Mouse Drag = Orbit | Scroll = Zoom\n"
              << "           B = Toggle Bloom | +/- = Adjust Bloom Intensity\n"
              << "           Space = Pause Orbits | R = Reset Camera\n"
              << "========================================================" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer - Phase 8: RenderGraph & Bloom", nullptr, nullptr);
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
        deviceDesc.applicationName = "Sunshine 07_render_graph_bloom";

        Sunshine::Device device(deviceDesc);
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        Sunshine::SwapchainContext swapchain(device, window);

        // 1. Initialize RenderGraph
        std::cout << "[Demo] Initializing Sunshine::RenderGraph..." << std::endl;
        Sunshine::RenderGraph renderGraph(device);

        // 2. Load 3D glTF Model (DamagedHelmet)
        std::cout << "[Demo] Loading 3D model asset..." << std::endl;
        auto model = Sunshine::Model::LoadGLTF(device, "assets/models/DamagedHelmet.glb");

        // 3. Create Ground Plane Geometry
        std::vector<Sunshine::ModelVertex> groundVertices = {
            { {-12.0f, -1.3f, -12.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { { 12.0f, -1.3f, -12.0f}, {0.0f, 1.0f, 0.0f}, {6.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { { 12.0f, -1.3f,  12.0f}, {0.0f, 1.0f, 0.0f}, {6.0f, 6.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
            { {-12.0f, -1.3f,  12.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 6.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }
        };
        std::vector<uint32_t> groundIndices = { 0, 1, 2, 2, 3, 0 };

        auto groundVBO = device.CreateVertexBuffer(groundVertices);
        auto groundIBO = device.CreateIndexBuffer(groundIndices);

        // 4. Create Glowing Orb Geometry
        auto [sphereVertices, sphereIndices] = CreateSphere(0.24f, 24, 16);
        auto sphereVBO = device.CreateVertexBuffer(sphereVertices);
        auto sphereIBO = device.CreateIndexBuffer(sphereIndices);

        // 5. Textures & Samplers
        std::vector<uint32_t> groundPixels(256 * 256);
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                bool border = (x % 32 == 0) || (y % 32 == 0);
                bool check = ((x / 32) + (y / 32)) % 2 == 0;
                uint8_t c = border ? 35 : (check ? 85 : 65);
                groundPixels[y * 256 + x] = 0xFF000000 | (c << 16) | (c << 8) | c;
            }
        }
        auto groundAlbedo = device.CreateTexture2D(256, 256, groundPixels.data(), Sunshine::RHI::TextureFormat::RGBA8_UNORM);

        uint32_t flatNormalPixel = 0xFFFF8080;
        auto defaultNormalMap = device.CreateTexture2D(1, 1, &flatNormalPixel);

        uint32_t groundMRPixel = 0xFF286600; // Reflective smooth floor (roughness ~ 0.4)
        auto groundMRMap = device.CreateTexture2D(1, 1, &groundMRPixel);

        Sunshine::RHI::SamplerDesc repeatSamplerDesc{};
        repeatSamplerDesc.addressModeU = Sunshine::RHI::AddressMode::Repeat;
        repeatSamplerDesc.addressModeV = Sunshine::RHI::AddressMode::Repeat;
        auto groundSampler = device.CreateSampler(repeatSamplerDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        auto defaultWhiteTex = device.CreateTexture2D(1, 1, &whitePixel);
        auto defaultSampler = device.CreateSampler();

        Sunshine::RHI::SamplerDesc clampSamplerDesc{};
        clampSamplerDesc.addressModeU = Sunshine::RHI::AddressMode::ClampToEdge;
        clampSamplerDesc.addressModeV = Sunshine::RHI::AddressMode::ClampToEdge;
        auto clampSampler = device.CreateSampler(clampSamplerDesc);

        // 6. Create Shaders
        auto vsFullscreen = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::FullscreenVertexSPIRV,
            .entryPoint = "main"
        });

        auto vsScene = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Vertex,
            .spirvCode = Sunshine::Shaders::SceneHDRVertexSPIRV,
            .entryPoint = "main"
        });

        auto fsScene = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::SceneHDRFragmentSPIRV,
            .entryPoint = "main"
        });

        auto fsBright = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::BrightFilterFragmentSPIRV,
            .entryPoint = "main"
        });

        auto fsBlurDown = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::BlurDownFragmentSPIRV,
            .entryPoint = "main"
        });

        auto fsBlurUp = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::BlurUpFragmentSPIRV,
            .entryPoint = "main"
        });

        auto fsComposite = device.CreateShader({
            .stage = Sunshine::RHI::ShaderStage::Fragment,
            .spirvCode = Sunshine::Shaders::CompositeFragmentSPIRV,
            .entryPoint = "main"
        });

        // 7. Temporary Offscreen RenderTarget to deduce pipeline render passes
        Sunshine::RenderTargetDesc hdrTargetDesc{};
        hdrTargetDesc.width = WIDTH;
        hdrTargetDesc.height = HEIGHT;
        hdrTargetDesc.colorFormat = Sunshine::RHI::TextureFormat::RGBA16_FLOAT;
        hdrTargetDesc.hasDepth = true;
        Sunshine::RenderTarget dummyHDRTarget(device, hdrTargetDesc);

        Sunshine::RenderTargetDesc postTargetDesc{};
        postTargetDesc.width = WIDTH / 2;
        postTargetDesc.height = HEIGHT / 2;
        postTargetDesc.colorFormat = Sunshine::RHI::TextureFormat::RGBA16_FLOAT;
        postTargetDesc.hasDepth = false;
        Sunshine::RenderTarget dummyPostTarget(device, postTargetDesc);

        // 8. Create Pipelines
        // 8a. Scene HDR Pipeline
        Sunshine::RHI::PipelineDesc scenePipeDesc{};
        scenePipeDesc.debugName = "SceneHDRPipeline";
        scenePipeDesc.colorAttachmentCount = 1;
        scenePipeDesc.vertexShader = vsScene;
        scenePipeDesc.fragmentShader = fsScene;
        scenePipeDesc.vertexLayout.stride = sizeof(Sunshine::ModelVertex);
        scenePipeDesc.vertexLayout.attributes = {
            { 0, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, position)), 0 },
            { 1, Sunshine::RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, normal)), 0 },
            { 2, Sunshine::RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, texCoord)), 0 },
            { 3, Sunshine::RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(Sunshine::ModelVertex, tangent)), 0 }
        };
        scenePipeDesc.depthStencil.depthTestEnable = true;
        scenePipeDesc.depthStencil.depthWriteEnable = true;
        scenePipeDesc.depthStencil.depthCompareOp = Sunshine::RHI::CompareOp::LessOrEqual;
        scenePipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::Back;
        scenePipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Vertex | Sunshine::RHI::ShaderStage::Fragment },
            { 1, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 2, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 3, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment }
        };
        auto scenePipeline = device.CreatePipeline(scenePipeDesc, dummyHDRTarget);

        // 8b. Bright Filter Pipeline (fullscreen, no vertex layout)
        Sunshine::RHI::PipelineDesc brightPipeDesc{};
        brightPipeDesc.debugName = "BrightFilterPipeline";
        brightPipeDesc.colorAttachmentCount = 1;
        brightPipeDesc.vertexShader = vsFullscreen;
        brightPipeDesc.fragmentShader = fsBright;
        brightPipeDesc.depthStencil.depthTestEnable = false;
        brightPipeDesc.depthStencil.depthWriteEnable = false;
        brightPipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::None;
        brightPipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 1, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Fragment }
        };
        auto brightFilterPipeline = device.CreatePipeline(brightPipeDesc, dummyPostTarget);

        // 8c. Blur Downsample Pipeline
        Sunshine::RHI::PipelineDesc blurDownPipeDesc = brightPipeDesc;
        blurDownPipeDesc.debugName = "BlurDownPipeline";
        blurDownPipeDesc.fragmentShader = fsBlurDown;
        auto blurDownPipeline = device.CreatePipeline(blurDownPipeDesc, dummyPostTarget);

        // 8d. Blur Upsample Pipeline
        Sunshine::RHI::PipelineDesc blurUpPipeDesc = brightPipeDesc;
        blurUpPipeDesc.debugName = "BlurUpPipeline";
        blurUpPipeDesc.fragmentShader = fsBlurUp;
        auto blurUpPipeline = device.CreatePipeline(blurUpPipeDesc, dummyPostTarget);

        // 8e. Composite Pipeline (renders to Swapchain)
        Sunshine::RHI::PipelineDesc compositePipeDesc{};
        compositePipeDesc.debugName = "CompositePipeline";
        compositePipeDesc.colorAttachmentCount = 1;
        compositePipeDesc.vertexShader = vsFullscreen;
        compositePipeDesc.fragmentShader = fsComposite;
        compositePipeDesc.depthStencil.depthTestEnable = false;
        compositePipeDesc.depthStencil.depthWriteEnable = false;
        compositePipeDesc.rasterizer.cullMode = Sunshine::RHI::CullMode::None;
        compositePipeDesc.descriptorBindings = {
            { 0, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 1, Sunshine::RHI::DescriptorType::CombinedImageSampler, Sunshine::RHI::ShaderStage::Fragment },
            { 2, Sunshine::RHI::DescriptorType::UniformBuffer, Sunshine::RHI::ShaderStage::Fragment }
        };
        auto compositePipeline = device.CreatePipeline(compositePipeDesc);

        // 9. Uniform Buffers
        auto sceneUbo = device.CreateUniformBuffer<SceneUniforms>();
        auto brightUbo = device.CreateUniformBuffer<BrightParams>();
        auto blurDown1Ubo = device.CreateUniformBuffer<BlurParams>();
        auto blurDown2Ubo = device.CreateUniformBuffer<BlurParams>();
        auto blurUp1Ubo = device.CreateUniformBuffer<BlurParams>();
        auto blurUp0Ubo = device.CreateUniformBuffer<BlurParams>();
        auto compositeUbo = device.CreateUniformBuffer<CompositeParams>();

        // Set static blur parameters
        BrightParams bParams{1.0f, 0.5f, {0.0f, 0.0f}};
        brightUbo.Update(bParams);

        BlurParams pDown1{glm::vec2(1.0f / (WIDTH / 2.0f), 1.0f / (HEIGHT / 2.0f)), {0.0f, 0.0f}};
        blurDown1Ubo.Update(pDown1);

        BlurParams pDown2{glm::vec2(1.0f / (WIDTH / 4.0f), 1.0f / (HEIGHT / 4.0f)), {0.0f, 0.0f}};
        blurDown2Ubo.Update(pDown2);

        BlurParams pUp1{glm::vec2(1.0f / (WIDTH / 8.0f), 1.0f / (HEIGHT / 8.0f)), {0.0f, 0.0f}};
        blurUp1Ubo.Update(pUp1);

        BlurParams pUp0{glm::vec2(1.0f / (WIDTH / 4.0f), 1.0f / (HEIGHT / 4.0f)), {0.0f, 0.0f}};
        blurUp0Ubo.Update(pUp0);

        auto startTime = std::chrono::high_resolution_clock::now();
        auto lastTime = startTime;
        uint32_t frameCount = 0;
        float fpsTimer = 0.0f;
        float animTime = 0.0f;

        glm::mat4 modelBaseMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.15f));

        std::cout << "[Demo] Starting rendering loop with Sunshine::RenderGraph..." << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            if (!g_animationPaused) {
                animTime += dt;
            }

            frameCount++;
            fpsTimer += dt;
            if (fpsTimer >= 0.5f) {
                float fps = frameCount / fpsTimer;
                float frameMs = (fpsTimer / frameCount) * 1000.0f;
                std::string title = "Sunshine Renderer | Phase 8: RenderGraph & Bloom | " +
                                    std::to_string(static_cast<int>(fps)) + " FPS (" +
                                    std::to_string(frameMs).substr(0, 4) + " ms) | Bloom: " +
                                    (g_bloomEnabled ? ("ON (" + std::to_string(g_bloomIntensity).substr(0, 3) + "x)") : "OFF") +
                                    " | Passes: " + std::to_string(renderGraph.GetPassCount()) +
                                    " | Pooled: " + std::to_string(renderGraph.GetPooledTargetCount());
                glfwSetWindowTitle(window, title.c_str());
                frameCount = 0;
                fpsTimer = 0.0f;
            }

            // Directional Light & Camera Setup
            glm::vec3 lightDir = glm::normalize(glm::vec3(std::cos(animTime * 0.4f) * 4.0f, 6.0f, std::sin(animTime * 0.4f) * 4.0f));

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
            // CONSTRUCT RENDER GRAPH FOR THIS FRAME
            // =========================================================================
            renderGraph.Reset();

            // 1. Declare Virtual Graph Textures
            auto rgSceneHDR = renderGraph.CreateTexture({
                .width = WIDTH,
                .height = HEIGHT,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = true,
                .name = "SceneHDR"
            });

            auto rgBrightBloom = renderGraph.CreateTexture({
                .width = WIDTH / 2,
                .height = HEIGHT / 2,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = false,
                .name = "BrightBloom"
            });

            auto rgBloomDown1 = renderGraph.CreateTexture({
                .width = WIDTH / 4,
                .height = HEIGHT / 4,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = false,
                .name = "BloomDown1"
            });

            auto rgBloomDown2 = renderGraph.CreateTexture({
                .width = WIDTH / 8,
                .height = HEIGHT / 8,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = false,
                .name = "BloomDown2"
            });

            auto rgBloomUp1 = renderGraph.CreateTexture({
                .width = WIDTH / 4,
                .height = HEIGHT / 4,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = false,
                .name = "BloomUp1"
            });

            auto rgBloomFinal = renderGraph.CreateTexture({
                .width = WIDTH / 2,
                .height = HEIGHT / 2,
                .format = Sunshine::RHI::TextureFormat::RGBA16_FLOAT,
                .hasDepth = false,
                .name = "BloomFinal"
            });

            // 2. Define Passes
            // 2a. PASS 1: Scene HDR Pass (3D PBR Helmet + Ground + Orbiting Neon Orbs)
            renderGraph.AddPass("SceneHDRPass",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.SetColorTarget(rgSceneHDR, Sunshine::Color{0.02f, 0.03f, 0.05f, 1.0f});
                    builder.SetDepthTarget(rgSceneHDR);
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(scenePipeline);

                    // Draw Ground Plane
                    {
                        SceneUniforms ubo{};
                        ubo.view = view;
                        ubo.proj = proj;
                        ubo.model = glm::mat4(1.0f);
                        ubo.camPos = camPos;
                        ubo.lightDir = lightDir;
                        ubo.lightIntensity = 2.8f;
                        ubo.lightColor = glm::vec3(1.0f, 0.95f, 0.9f);
                        ubo.ambientStrength = 0.05f;
                        ubo.emissiveFactor = glm::vec4(0.0f);
                        sceneUbo.Update(ubo);

                        passCmd.SetUniformBuffer(0, sceneUbo);
                        passCmd.SetTexture(1, groundAlbedo, groundSampler);
                        passCmd.SetTexture(2, defaultNormalMap, defaultSampler);
                        passCmd.SetTexture(3, groundMRMap, defaultSampler);
                        passCmd.SetVertexBuffer(0, groundVBO);
                        passCmd.SetIndexBuffer(groundIBO);
                        passCmd.DrawIndexed(static_cast<uint32_t>(groundIndices.size()));
                    }

                    // Draw 3D Model
                    auto renderNode = [&](auto&& self, const std::shared_ptr<Sunshine::SceneNode>& node, const glm::mat4& parentMat) -> void {
                        if (!node) return;
                        glm::mat4 worldMat = parentMat * node->transform.GetMatrix();

                        for (uint32_t submeshIdx : node->submeshIndices) {
                            if (submeshIdx >= model->GetSubmeshes().size()) continue;
                            const auto& submesh = model->GetSubmeshes()[submeshIdx];
                            const auto* mat = model->GetMaterial(submesh.materialIndex);

                            SceneUniforms ubo{};
                            ubo.view = view;
                            ubo.proj = proj;
                            ubo.model = worldMat;
                            ubo.camPos = camPos;
                            ubo.lightDir = lightDir;
                            ubo.lightIntensity = 2.8f;
                            ubo.lightColor = glm::vec3(1.0f, 0.95f, 0.9f);
                            ubo.ambientStrength = 0.05f;
                            ubo.emissiveFactor = glm::vec4(0.0f);
                            sceneUbo.Update(ubo);

                            passCmd.SetUniformBuffer(0, sceneUbo);

                            auto albedoTex = (mat && mat->baseColorTexture) ? mat->baseColorTexture : model->defaultWhiteTexture;
                            auto normalTex = (mat && mat->normalTexture) ? mat->normalTexture : model->defaultNormalTexture;
                            auto mrTex = (mat && mat->metallicRoughnessTexture) ? mat->metallicRoughnessTexture : model->defaultMetallicRoughnessTexture;
                            auto sampler = (mat && mat->sampler) ? mat->sampler : model->defaultSampler;

                            passCmd.SetTexture(1, *albedoTex, *sampler);
                            passCmd.SetTexture(2, *normalTex, *sampler);
                            passCmd.SetTexture(3, *mrTex, *sampler);

                            passCmd.SetVertexBuffer(0, submesh.vertexBuffer);
                            passCmd.SetIndexBuffer(submesh.indexBuffer);
                            passCmd.DrawIndexed(submesh.indexCount);
                        }

                        for (const auto& child : node->children) {
                            self(self, child, worldMat);
                        }
                    };

                    for (const auto& rootNode : model->GetRootNodes()) {
                        renderNode(renderNode, rootNode, modelBaseMatrix);
                    }

                    // Draw 4 Orbiting Neon Glowing Orbs with HDR Emissive Radiance!
                    struct Orb {
                        glm::vec3 color;
                        float speed;
                        float radius;
                        float yOffset;
                        float phase;
                    };
                    std::vector<Orb> orbs = {
                        { glm::vec3(0.0f, 6.0f, 15.0f),  1.2f, 1.9f, 0.2f, 0.0f },                    // Neon Cyan
                        { glm::vec3(15.0f, 0.0f, 9.0f), -0.9f, 2.3f, 0.6f, std::numbers::pi_v<float> * 0.5f }, // Hot Pink
                        { glm::vec3(15.0f, 8.0f, 0.0f),  1.4f, 1.6f, -0.2f, std::numbers::pi_v<float> },        // Radiant Gold
                        { glm::vec3(0.0f, 15.0f, 4.0f), -1.1f, 2.1f, 0.9f, std::numbers::pi_v<float> * 1.5f }  // Neon Emerald
                    };

                    for (const auto& orb : orbs) {
                        float angle = animTime * orb.speed + orb.phase;
                        glm::vec3 orbPos{
                            std::cos(angle) * orb.radius,
                            orb.yOffset + std::sin(animTime * 1.5f + orb.phase) * 0.3f,
                            std::sin(angle) * orb.radius
                        };

                        SceneUniforms ubo{};
                        ubo.view = view;
                        ubo.proj = proj;
                        ubo.model = glm::translate(glm::mat4(1.0f), orbPos);
                        ubo.camPos = camPos;
                        ubo.lightDir = lightDir;
                        ubo.lightIntensity = 1.0f;
                        ubo.lightColor = orb.color;
                        ubo.ambientStrength = 1.0f;
                        ubo.emissiveFactor = glm::vec4(orb.color, 4.0f); // High HDR boost!
                        sceneUbo.Update(ubo);

                        passCmd.SetUniformBuffer(0, sceneUbo);
                        passCmd.SetTexture(1, defaultWhiteTex, defaultSampler);
                        passCmd.SetTexture(2, defaultNormalMap, defaultSampler);
                        passCmd.SetTexture(3, defaultWhiteTex, defaultSampler);
                        passCmd.SetVertexBuffer(0, sphereVBO);
                        passCmd.SetIndexBuffer(sphereIBO);
                        passCmd.DrawIndexed(static_cast<uint32_t>(sphereIndices.size()));
                    }
                }
            );

            // 2b. PASS 2: Bright Filter Pass (Extracts HDR pixels with luminance > 1.0)
            renderGraph.AddPass("BrightFilterPass",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgSceneHDR);
                    builder.SetColorTarget(rgBrightBloom, Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(brightFilterPipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgSceneHDR), clampSampler);
                    passCmd.SetUniformBuffer(1, brightUbo);
                    passCmd.Draw(3); // Procedural fullscreen triangle
                }
            );

            // 2c. PASS 3: Blur Downsample Level 1 (640x360 -> 320x180)
            renderGraph.AddPass("BlurDown1",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgBrightBloom);
                    builder.SetColorTarget(rgBloomDown1, Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(blurDownPipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgBrightBloom), clampSampler);
                    passCmd.SetUniformBuffer(1, blurDown1Ubo);
                    passCmd.Draw(3);
                }
            );

            // 2d. PASS 4: Blur Downsample Level 2 (320x180 -> 160x90)
            renderGraph.AddPass("BlurDown2",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgBloomDown1);
                    builder.SetColorTarget(rgBloomDown2, Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(blurDownPipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgBloomDown1), clampSampler);
                    passCmd.SetUniformBuffer(1, blurDown2Ubo);
                    passCmd.Draw(3);
                }
            );

            // 2e. PASS 5: Blur Upsample Level 1 (160x90 -> 320x180)
            renderGraph.AddPass("BlurUp1",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgBloomDown2);
                    builder.SetColorTarget(rgBloomUp1, Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(blurUpPipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgBloomDown2), clampSampler);
                    passCmd.SetUniformBuffer(1, blurUp1Ubo);
                    passCmd.Draw(3);
                }
            );

            // 2f. PASS 6: Blur Upsample Level 0 (320x180 -> 640x360 Final Bloom Target)
            renderGraph.AddPass("BlurUp0",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgBloomUp1);
                    builder.SetColorTarget(rgBloomFinal, Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    passCmd.SetPipeline(blurUpPipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgBloomUp1), clampSampler);
                    passCmd.SetUniformBuffer(1, blurUp0Ubo);
                    passCmd.Draw(3);
                }
            );

            // 2g. PASS 7: Composite & Tone Mapping to Presentation Swapchain
            renderGraph.AddPass("CompositeToSwapchain",
                [&](Sunshine::RenderPassBuilder& builder) {
                    builder.Read(rgSceneHDR);
                    builder.Read(rgBloomFinal);
                    builder.SetSwapchainTarget(Sunshine::Color{0.0f, 0.0f, 0.0f, 1.0f});
                },
                [&](Sunshine::CommandContext& passCmd) {
                    CompositeParams cParams{};
                    cParams.bloomIntensity = g_bloomIntensity;
                    cParams.exposure = 1.0f;
                    cParams.bloomEnabled = g_bloomEnabled ? 1 : 0;
                    compositeUbo.Update(cParams);

                    passCmd.SetPipeline(compositePipeline);
                    passCmd.SetTexture(0, renderGraph.GetPhysicalTexture(rgSceneHDR), clampSampler);
                    passCmd.SetTexture(1, renderGraph.GetPhysicalTexture(rgBloomFinal), clampSampler);
                    passCmd.SetUniformBuffer(2, compositeUbo);
                    passCmd.Draw(3);
                }
            );

            // 3. Compile DAG & Allocate/Reuse Pooled RenderTargets
            renderGraph.Compile();

            // 4. Execute All Graph Passes
            renderGraph.Execute(cmd);

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
