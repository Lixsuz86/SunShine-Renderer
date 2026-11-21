#include "sunshine/sunshine.hpp"
#include "shaders/csm_shaders.spv.hpp"

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
#include <array>

using namespace Sunshine;

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

struct alignas(16) SceneUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
};

struct alignas(16) CascadeDepthUBO {
    glm::mat4 lightSpaceMatrix;
};

struct alignas(16) CSMUBO {
    glm::mat4 lightSpaceMatrices[4];
    glm::vec4 cascadeSplits; // Split depths in view-space
    glm::vec4 lightDir;
    glm::vec4 lightColor; // rgb = color, w = intensity
    glm::vec4 csmParams;  // x: biasMultiplier, y: blendBand, z: debugColors, w: cascadeCount
};

struct alignas(16) InstanceData {
    glm::vec4 modelCol0;
    glm::vec4 modelCol1;
    glm::vec4 modelCol2;
    glm::vec4 modelCol3;
    glm::vec4 pbrFactors; // x: roughness, y: metallic, z: specular, w: tileScale
    glm::vec4 color;      // baseColorFactor
};

inline InstanceData MakeInstance(const glm::mat4& model, const glm::vec4& color, const glm::vec4& pbr) {
    InstanceData inst{};
    inst.modelCol0 = model[0];
    inst.modelCol1 = model[1];
    inst.modelCol2 = model[2];
    inst.modelCol3 = model[3];
    inst.pbrFactors = pbr;
    inst.color = color;
    return inst;
}

// Global Camera Interaction State
struct CameraController {
    float yaw = 35.0f;
    float pitch = 22.0f;
    float distance = 18.0f;
    glm::vec3 lookCenter = glm::vec3(0.0f, 2.0f, 0.0f);
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

        g_camera.yaw += dx * 0.35f;
        g_camera.pitch = std::clamp(g_camera.pitch - dy * 0.35f, 2.0f, 85.0f);

        g_camera.lastMouseX = xpos;
        g_camera.lastMouseY = ypos;
    }
}

void ScrollCallback(GLFWwindow*, double, double yoffset) {
    g_camera.distance = std::clamp(g_camera.distance - static_cast<float>(yoffset) * 1.5f, 3.0f, 95.0f);
}

// Procedural Terrain Grid Mesh
void GeneratePlane(float size, uint32_t subdivisions, std::vector<ModelVertex>& outVertices, std::vector<uint32_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    float halfSize = size * 0.5f;
    float step = size / static_cast<float>(subdivisions);

    for (uint32_t z = 0; z <= subdivisions; ++z) {
        float zPos = -halfSize + static_cast<float>(z) * step;
        float v = static_cast<float>(z) / static_cast<float>(subdivisions);

        for (uint32_t x = 0; x <= subdivisions; ++x) {
            float xPos = -halfSize + static_cast<float>(x) * step;
            float u = static_cast<float>(x) / static_cast<float>(subdivisions);

            ModelVertex vert{};
            vert.position = glm::vec3(xPos, 0.0f, zPos);
            vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vert.texCoord = glm::vec2(u * 32.0f, v * 32.0f);
            vert.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

            outVertices.push_back(vert);
        }
    }

    for (uint32_t z = 0; z < subdivisions; ++z) {
        for (uint32_t x = 0; x < subdivisions; ++x) {
            uint32_t i0 = z * (subdivisions + 1) + x;
            uint32_t i1 = (z + 1) * (subdivisions + 1) + x;
            uint32_t i2 = (z + 1) * (subdivisions + 1) + (x + 1);
            uint32_t i3 = z * (subdivisions + 1) + (x + 1);

            outIndices.push_back(i0);
            outIndices.push_back(i1);
            outIndices.push_back(i2);

            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
    }
}

// Procedural Box / Pillar Mesh Generator
void GenerateBox(const glm::vec3& halfExtents, std::vector<ModelVertex>& outVertices, std::vector<uint32_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    struct Face {
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec3 corners[4];
    };

    Face faces[6] = {
        // Front (+Z)
        { { 0, 0, 1 }, { 1, 0, 0, 1 }, { {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1} } },
        // Back (-Z)
        { { 0, 0,-1 }, {-1, 0, 0, 1 }, { { 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1} } },
        // Left (-X)
        { {-1, 0, 0 }, { 0, 0, 1, 1 }, { {-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1} } },
        // Right (+X)
        { { 1, 0, 0 }, { 0, 0,-1, 1 }, { { 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1} } },
        // Top (+Y)
        { { 0, 1, 0 }, { 1, 0, 0, 1 }, { {-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1} } },
        // Bottom (-Y)
        { { 0,-1, 0 }, {-1, 0, 0, 1 }, { {-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1} } }
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());
        glm::vec2 uvs[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

        for (int i = 0; i < 4; ++i) {
            ModelVertex v{};
            v.position = faces[f].corners[i] * halfExtents;
            v.normal = faces[f].normal;
            v.tangent = faces[f].tangent;
            v.texCoord = uvs[i];
            outVertices.push_back(v);
        }

        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 2);

        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 2);
        outIndices.push_back(baseIdx + 3);
    }
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
            vert.normal = glm::normalize(vert.position);
            vert.texCoord = glm::vec2(u, v);
            vert.tangent = glm::vec4(-std::sin(phi), 0.0f, std::cos(phi), 1.0f);

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

} // anonymous namespace

int main() {
    std::cout << "\n======================================================================\n"
              << "    Sunshine Renderer — Phase 11: Cascaded Shadow Maps (CSM)\n"
              << "======================================================================\n"
              << "  [Key Features Tested]:\n"
              << "    * Practical Logarithmic 4-Cascade Frustum Splitting\n"
              << "    * Sub-pixel Texel Snapping (Zero Edge Shimmering & Artifacts)\n"
              << "    * Dedicated 2D Texture Array Depth Buffer (2048x2048x4, D32_SFLOAT)\n"
              << "    * Hardware Instanced Rendering (All Colonnades & Monoliths in 3 Calls)\n"
              << "    * Smooth 20% Cascade Blend Band Transition\n"
              << "    * Real-Time Interactive Open-World Scene (Near-Detail to 150m Horizon)\n"
              << "  [Interactive Controls]:\n"
              << "    [Left Mouse Drag] : Orbit Camera View Around Open World\n"
              << "    [Mouse Scroll]    : Zoom In (Micro-Details) / Zoom Out (Full Landscape)\n"
              << "    [C]               : Toggle Cascade Color Debug Tint (Red/Green/Blue/Yellow)\n"
              << "    [B]               : Toggle Cascade Blend Band (Smooth Transition)\n"
              << "    [SPACE]           : Pause / Resume Dynamic Sun Direction Orbit\n"
              << "    [R]               : Reset Camera Position\n"
              << "    [ESC]             : Exit Demo\n"
              << "======================================================================\n" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Sunshine Renderer — Phase 11: Cascaded Shadows", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    try {
        Device device;
        std::cout << "[Demo] Active GPU: " << device.GetDeviceName() << std::endl;

        SwapchainContext swapchain(device, window);

        // ---------------------------------------------------------------------
        // 1. Initialize Cascaded Shadow Map (4 Cascades, 2048x2048)
        // ---------------------------------------------------------------------
        const uint32_t NUM_CASCADES = 4;
        const uint32_t SHADOW_RES = 2048;
        CascadedShadowMap csm(device, SHADOW_RES, NUM_CASCADES);
        std::cout << "[Demo] CascadedShadowMap initialized: " << SHADOW_RES << "x" << SHADOW_RES << " x " << NUM_CASCADES << " layers (D32_SFLOAT)." << std::endl;

        // ---------------------------------------------------------------------
        // 2. Vertex Layout (Binding 0: ModelVertex, Binding 1: InstanceData)
        // ---------------------------------------------------------------------
        RHI::VertexLayout sharedVertexLayout{};
        sharedVertexLayout.bindings = {
            { 0, sizeof(ModelVertex), RHI::VertexInputRate::PerVertex },
            { 1, sizeof(InstanceData), RHI::VertexInputRate::PerInstance }
        };
        sharedVertexLayout.attributes = {
            // Binding 0: Per-vertex geometry
            { 0, RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(ModelVertex, position)), 0 },
            { 1, RHI::VertexFormat::Float3, static_cast<uint32_t>(offsetof(ModelVertex, normal)), 0 },
            { 2, RHI::VertexFormat::Float2, static_cast<uint32_t>(offsetof(ModelVertex, texCoord)), 0 },
            { 3, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(ModelVertex, tangent)), 0 },

            // Binding 1: Per-instance data
            { 4, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol0)), 1 },
            { 5, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol1)), 1 },
            { 6, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol2)), 1 },
            { 7, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, modelCol3)), 1 },
            { 8, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, pbrFactors)), 1 },
            { 9, RHI::VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceData, color)), 1 }
        };

        // ---------------------------------------------------------------------
        // 3. Build Shadow Depth Pipeline
        // ---------------------------------------------------------------------
        RHI::PipelineDesc depthPipeDesc{};
        depthPipeDesc.debugName = "CSMDepthPipeline";
        depthPipeDesc.colorAttachmentCount = 0; // Pure depth pass

        depthPipeDesc.vertexShader = device.CreateShader({
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = Shaders::CSMDepthVertexSPIRV,
            .entryPoint = "main"
        });

        depthPipeDesc.fragmentShader = device.CreateShader({
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = Shaders::CSMDepthFragmentSPIRV,
            .entryPoint = "main"
        });

        depthPipeDesc.vertexLayout = sharedVertexLayout;
        depthPipeDesc.depthStencil.depthTestEnable = true;
        depthPipeDesc.depthStencil.depthWriteEnable = true;
        depthPipeDesc.depthStencil.depthCompareOp = RHI::CompareOp::LessOrEqual;

        // Depth bias to eliminate acne
        depthPipeDesc.rasterizer.cullMode = RHI::CullMode::None;
        depthPipeDesc.rasterizer.depthBiasEnable = true;
        depthPipeDesc.rasterizer.depthBiasConstantFactor = 1.6f;
        depthPipeDesc.rasterizer.depthBiasSlopeFactor = 2.0f;

        depthPipeDesc.descriptorBindings = {
            { 0, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Vertex } // CascadeUBO
        };

        auto depthPipeline = device.CreatePipeline(depthPipeDesc, csm);

        // ---------------------------------------------------------------------
        // 4. Build Main Scene CSM Pipeline
        // ---------------------------------------------------------------------
        RHI::PipelineDesc scenePipeDesc{};
        scenePipeDesc.debugName = "CSMScenePipeline";
        scenePipeDesc.colorAttachmentCount = 1;

        scenePipeDesc.vertexShader = device.CreateShader({
            .stage = RHI::ShaderStage::Vertex,
            .spirvCode = Shaders::CSMSceneVertexSPIRV,
            .entryPoint = "main"
        });

        scenePipeDesc.fragmentShader = device.CreateShader({
            .stage = RHI::ShaderStage::Fragment,
            .spirvCode = Shaders::CSMSceneFragmentSPIRV,
            .entryPoint = "main"
        });

        scenePipeDesc.vertexLayout = sharedVertexLayout;
        scenePipeDesc.depthStencil.depthTestEnable = true;
        scenePipeDesc.depthStencil.depthWriteEnable = true;
        scenePipeDesc.depthStencil.depthCompareOp = RHI::CompareOp::LessOrEqual;
        scenePipeDesc.rasterizer.cullMode = RHI::CullMode::None;

        scenePipeDesc.descriptorBindings = {
            { 0, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment }, // SceneUBO
            { 1, RHI::DescriptorType::UniformBuffer, RHI::ShaderStage::Fragment },                             // CSMUBO
            { 2, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment },                     // sampler2DArray
            { 3, RHI::DescriptorType::CombinedImageSampler, RHI::ShaderStage::Fragment }                      // albedoTexture
        };

        auto scenePipeline = device.CreatePipeline(scenePipeDesc);

        // ---------------------------------------------------------------------
        // 5. Generate Procedural Meshes
        // ---------------------------------------------------------------------
        // Terrain Mesh (160x160m grid)
        std::vector<ModelVertex> terrainVertices;
        std::vector<uint32_t> terrainIndices;
        GeneratePlane(160.0f, 64, terrainVertices, terrainIndices);
        auto terrainVB = device.CreateVertexBuffer(terrainVertices);
        auto terrainIB = device.CreateIndexBuffer(terrainIndices);
        uint32_t terrainIndexCount = static_cast<uint32_t>(terrainIndices.size());

        // Pillar / Box Mesh
        std::vector<ModelVertex> boxVertices;
        std::vector<uint32_t> boxIndices;
        GenerateBox(glm::vec3(0.5f, 0.5f, 0.5f), boxVertices, boxIndices);
        auto boxVB = device.CreateVertexBuffer(boxVertices);
        auto boxIB = device.CreateIndexBuffer(boxIndices);
        uint32_t boxIndexCount = static_cast<uint32_t>(boxIndices.size());

        // Sphere Mesh
        std::vector<ModelVertex> sphereVertices;
        std::vector<uint32_t> sphereIndices;
        GenerateSphere(1.0f, 32, 32, sphereVertices, sphereIndices);
        auto sphereVB = device.CreateVertexBuffer(sphereVertices);
        auto sphereIB = device.CreateIndexBuffer(sphereIndices);
        uint32_t sphereIndexCount = static_cast<uint32_t>(sphereIndices.size());

        // Textures
        // 64x64 Procedural checkerboard texture
        const uint32_t TEX_SIZE = 64;
        std::vector<uint32_t> checkerPixels(TEX_SIZE * TEX_SIZE);
        for (uint32_t y = 0; y < TEX_SIZE; ++y) {
            for (uint32_t x = 0; x < TEX_SIZE; ++x) {
                bool isWhite = ((x / 8) + (y / 8)) % 2 == 0;
                checkerPixels[y * TEX_SIZE + x] = isWhite ? 0xFFEEEEEE : 0xFFCCCCCC;
            }
        }
        auto checkerTexture = device.CreateTexture2D(TEX_SIZE, TEX_SIZE, checkerPixels.data());

        RHI::SamplerDesc repeatSamplerDesc{};
        repeatSamplerDesc.addressModeU = RHI::AddressMode::Repeat;
        repeatSamplerDesc.addressModeV = RHI::AddressMode::Repeat;
        auto checkerSampler = device.CreateSampler(repeatSamplerDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        auto defaultWhiteTex = device.CreateTexture2D(1, 1, &whitePixel);
        auto defaultSampler = device.CreateSampler({});

        // ---------------------------------------------------------------------
        // 6. Populate Instance Buffers (Hardware Instancing)
        // ---------------------------------------------------------------------
        std::vector<InstanceData> terrainInstances;
        std::vector<InstanceData> boxInstances;
        std::vector<InstanceData> sphereInstances;

        // Ground Plane Instance
        terrainInstances.push_back(MakeInstance(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
            glm::vec4(0.75f, 0.78f, 0.80f, 1.0f),
            glm::vec4(0.85f, 0.05f, 0.2f, 1.0f)
        ));

        // Near-Range Colonnade (Cascade 0: 0 to 12m)
        for (int i = -3; i <= 3; ++i) {
            // Front Pillars
            glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(i * 2.8f, 2.2f, 2.5f));
            mat = glm::scale(mat, glm::vec3(0.5f, 4.4f, 0.5f));
            boxInstances.push_back(MakeInstance(mat, glm::vec4(0.95f, 0.90f, 0.82f, 1.0f), glm::vec4(0.4f, 0.1f, 0.5f, 1.0f)));

            // Back Pillars
            glm::mat4 matBack = glm::translate(glm::mat4(1.0f), glm::vec3(i * 2.8f, 2.2f, -2.5f));
            matBack = glm::scale(matBack, glm::vec3(0.5f, 4.4f, 0.5f));
            boxInstances.push_back(MakeInstance(matBack, glm::vec4(0.95f, 0.90f, 0.82f, 1.0f), glm::vec4(0.4f, 0.1f, 0.5f, 1.0f)));

            // Spheres
            glm::mat4 sphereMat = glm::translate(glm::mat4(1.0f), glm::vec3(i * 2.8f, 0.6f, 0.0f));
            sphereMat = glm::scale(sphereMat, glm::vec3(0.6f));
            sphereInstances.push_back(MakeInstance(sphereMat, glm::vec4(0.95f, 0.35f, 0.25f, 1.0f), glm::vec4(0.2f, 0.7f, 0.8f, 1.0f)));
        }

        // Colonnade Architraves (Horizontal beams)
        glm::mat4 beamLeft = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.6f, 2.5f));
        beamLeft = glm::scale(beamLeft, glm::vec3(18.5f, 0.4f, 0.8f));
        boxInstances.push_back(MakeInstance(beamLeft, glm::vec4(0.92f, 0.88f, 0.80f, 1.0f), glm::vec4(0.4f, 0.1f, 0.5f, 1.0f)));

        glm::mat4 beamRight = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.6f, -2.5f));
        beamRight = glm::scale(beamRight, glm::vec3(18.5f, 0.4f, 0.8f));
        boxInstances.push_back(MakeInstance(beamRight, glm::vec4(0.92f, 0.88f, 0.80f, 1.0f), glm::vec4(0.4f, 0.1f, 0.5f, 1.0f)));

        // Mid-Range Monoliths (Cascade 1 & 2: 15 to 55m)
        for (int r = 0; r < 12; ++r) {
            float angle = static_cast<float>(r) * (6.283185f / 12.0f);
            float radius = 22.0f;
            float px = std::cos(angle) * radius;
            float pz = std::sin(angle) * radius;

            glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(px, 4.0f, pz));
            mat = glm::scale(mat, glm::vec3(1.4f, 8.0f, 1.4f));
            boxInstances.push_back(MakeInstance(mat, glm::vec4(0.65f, 0.70f, 0.85f, 1.0f), glm::vec4(0.5f, 0.2f, 0.4f, 1.0f)));

            glm::mat4 sphMat = glm::translate(glm::mat4(1.0f), glm::vec3(px, 9.2f, pz));
            sphMat = glm::scale(sphMat, glm::vec3(1.2f));
            sphereInstances.push_back(MakeInstance(sphMat, glm::vec4(0.95f, 0.80f, 0.20f, 1.0f), glm::vec4(0.15f, 0.9f, 0.9f, 1.0f)));
        }

        // Far-Range Horizon Towers (Cascade 3: 60 to 140m)
        for (int f = 0; f < 8; ++f) {
            float angle = static_cast<float>(f) * (6.283185f / 8.0f) + 0.35f;
            float radius = 65.0f;
            float px = std::cos(angle) * radius;
            float pz = std::sin(angle) * radius;

            glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(px, 15.0f, pz));
            mat = glm::scale(mat, glm::vec3(4.5f, 30.0f, 4.5f));
            boxInstances.push_back(MakeInstance(mat, glm::vec4(0.45f, 0.50f, 0.60f, 1.0f), glm::vec4(0.7f, 0.1f, 0.3f, 1.0f)));

            glm::mat4 sphMat = glm::translate(glm::mat4(1.0f), glm::vec3(px, 33.0f, pz));
            sphMat = glm::scale(sphMat, glm::vec3(3.0f));
            sphereInstances.push_back(MakeInstance(sphMat, glm::vec4(0.85f, 0.45f, 0.20f, 1.0f), glm::vec4(0.2f, 0.8f, 0.8f, 1.0f)));
        }

        auto terrainInstanceVB = device.CreateVertexBuffer(terrainInstances);
        auto boxInstanceVB = device.CreateVertexBuffer(boxInstances);
        auto sphereInstanceVB = device.CreateVertexBuffer(sphereInstances);

        uint32_t totalInstances = static_cast<uint32_t>(terrainInstances.size() + boxInstances.size() + sphereInstances.size());
        std::cout << "[Demo] Scene initialized with " << totalInstances << " instanced objects (1 terrain, " 
                  << boxInstances.size() << " boxes/pillars, " << sphereInstances.size() << " spheres)." << std::endl;

        // ---------------------------------------------------------------------
        // 7. Uniform Buffers & Animation State
        // ---------------------------------------------------------------------
        auto sceneUbo = device.CreateUniformBuffer<SceneUBO>();
        auto csmUbo = device.CreateUniformBuffer<CSMUBO>();

        std::array<UniformBuffer<CascadeDepthUBO>, NUM_CASCADES> cascadeDepthUbos = {
            device.CreateUniformBuffer<CascadeDepthUBO>(),
            device.CreateUniformBuffer<CascadeDepthUBO>(),
            device.CreateUniformBuffer<CascadeDepthUBO>(),
            device.CreateUniformBuffer<CascadeDepthUBO>()
        };

        Camera camera(45.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 250.0f);
        Profiler profiler;

        bool sunAnimationPaused = false;
        bool debugColorsEnabled = false;
        bool blendBandEnabled = true;
        bool spaceWasPressed = false;
        bool cWasPressed = false;
        bool bWasPressed = false;

        float sunTime = 0.8f;
        auto lastTime = std::chrono::high_resolution_clock::now();

        std::cout << "\n[Demo] Entering render loop...\n" << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                g_camera.yaw = 35.0f;
                g_camera.pitch = 22.0f;
                g_camera.distance = 18.0f;
            }

            // [Space] Toggle Sun Orbit
            bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
            if (spacePressed && !spaceWasPressed) {
                sunAnimationPaused = !sunAnimationPaused;
                std::cout << "[Demo] Sun orbit animation: " << (sunAnimationPaused ? "PAUSED" : "RUNNING") << std::endl;
            }
            spaceWasPressed = spacePressed;

            // [C] Toggle Cascade Debug Color Tint
            bool cPressed = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
            if (cPressed && !cWasPressed) {
                debugColorsEnabled = !debugColorsEnabled;
                std::cout << "[Demo] Cascade Color Debug Tint: " << (debugColorsEnabled ? "ENABLED (Red/Green/Blue/Yellow)" : "DISABLED") << std::endl;
            }
            cWasPressed = cPressed;

            // [B] Toggle Cascade Blend Band
            bool bPressed = (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS);
            if (bPressed && !bWasPressed) {
                blendBandEnabled = !blendBandEnabled;
                std::cout << "[Demo] Cascade Blend Band Transition: " << (blendBandEnabled ? "ENABLED (Smooth)" : "DISABLED (Hard Cut)") << std::endl;
            }
            bWasPressed = bPressed;

            int currentWidth = 0, currentHeight = 0;
            glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
            if (currentWidth == 0 || currentHeight == 0) {
                continue;
            }

            profiler.BeginFrame();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (!sunAnimationPaused) {
                sunTime += deltaTime * 0.15f;
            }

            camera.SetAspectRatio(static_cast<float>(currentWidth) / static_cast<float>(currentHeight));

            // Orbit Camera Position
            float radYaw = glm::radians(g_camera.yaw);
            float radPitch = glm::radians(g_camera.pitch);
            glm::vec3 camPos = g_camera.lookCenter + glm::vec3(
                g_camera.distance * std::cos(radPitch) * std::sin(radYaw),
                g_camera.distance * std::sin(radPitch),
                g_camera.distance * std::cos(radPitch) * std::cos(radYaw)
            );
            camera.LookAt(camPos, g_camera.lookCenter);

            // Dynamic Sun Direction (Rotating slowly across the sky)
            glm::vec3 sunDirection = glm::normalize(glm::vec3(
                std::cos(sunTime) * 0.75f,
                0.65f + std::sin(sunTime * 0.5f) * 0.15f,
                std::sin(sunTime) * 0.75f
            ));

            // -----------------------------------------------------------------
            // Update Cascades (Splits, Projections, Texel Snapping)
            // -----------------------------------------------------------------
            csm.UpdateCascades(camera, sunDirection, 150.0f, 0.85f);

            // Update UBOs
            SceneUBO sceneData{};
            sceneData.view = camera.GetViewMatrix();
            sceneData.proj = camera.GetProjectionMatrix();
            sceneData.camPos = glm::vec4(camPos, 1.0f);
            sceneUbo.Update(sceneData);

            CSMUBO csmData{};
            for (uint32_t i = 0; i < NUM_CASCADES; ++i) {
                csmData.lightSpaceMatrices[i] = csm.GetLightSpaceMatrix(i);
            }
            csmData.cascadeSplits = csm.GetCascadeSplitDepthsVec4();
            csmData.lightDir = glm::vec4(sunDirection, 0.0f);
            csmData.lightColor = glm::vec4(1.0f, 0.95f, 0.88f, 2.5f);
            csmData.csmParams = glm::vec4(1.0f, blendBandEnabled ? 1.0f : 0.0f, debugColorsEnabled ? 1.0f : 0.0f, static_cast<float>(NUM_CASCADES));
            csmUbo.Update(csmData);

            // -----------------------------------------------------------------
            // Render Frame
            // -----------------------------------------------------------------
            auto frame = device.BeginFrame(swapchain);
            if (frame) {
                auto& cmd = frame->GetCommandContext();

                // -------------------------------------------------------------
                // 1. Cascaded Shadow Depth Passes (4 Passes, 1 per Cascade)
                // -------------------------------------------------------------
                for (uint32_t c = 0; c < NUM_CASCADES; ++c) {
                    cmd.BeginShadowPass(csm, c);
                    cmd.SetPipeline(depthPipeline);

                    CascadeDepthUBO depthUBOData{ csm.GetLightSpaceMatrix(c) };
                    cascadeDepthUbos[c].Update(depthUBOData);
                    cmd.SetUniformBuffer(0, cascadeDepthUbos[c]);

                    // 1a. Draw Box Casters
                    cmd.SetVertexBuffer(0, boxVB);
                    cmd.SetVertexBuffer(1, boxInstanceVB);
                    cmd.SetIndexBuffer(boxIB);
                    cmd.DrawIndexed(boxIndexCount, static_cast<uint32_t>(boxInstances.size()));
                    profiler.RecordDraw(boxIndexCount / 3, static_cast<uint32_t>(boxInstances.size()));

                    // 1b. Draw Sphere Casters
                    cmd.SetVertexBuffer(0, sphereVB);
                    cmd.SetVertexBuffer(1, sphereInstanceVB);
                    cmd.SetIndexBuffer(sphereIB);
                    cmd.DrawIndexed(sphereIndexCount, static_cast<uint32_t>(sphereInstances.size()));
                    profiler.RecordDraw(sphereIndexCount / 3, static_cast<uint32_t>(sphereInstances.size()));

                    // 1c. Draw Terrain (Receiver)
                    cmd.SetVertexBuffer(0, terrainVB);
                    cmd.SetVertexBuffer(1, terrainInstanceVB);
                    cmd.SetIndexBuffer(terrainIB);
                    cmd.DrawIndexed(terrainIndexCount, 1);
                    profiler.RecordDraw(terrainIndexCount / 3, 1);

                    cmd.EndShadowPass();
                }

                // -------------------------------------------------------------
                // 2. Main Scene Pass (Forward Lighting + CSM Sampling)
                // -------------------------------------------------------------
                cmd.Clear(Color{ 0.08f, 0.10f, 0.14f, 1.0f });
                cmd.SetPipeline(scenePipeline);
                cmd.SetUniformBuffer(0, sceneUbo);
                cmd.SetUniformBuffer(1, csmUbo);
                cmd.SetTexture(2, csm.GetTexture(), csm.GetSampler());

                // 2a. Draw Boxes with White Texture
                cmd.SetTexture(3, defaultWhiteTex, defaultSampler);
                cmd.SetVertexBuffer(0, boxVB);
                cmd.SetVertexBuffer(1, boxInstanceVB);
                cmd.SetIndexBuffer(boxIB);
                cmd.DrawIndexed(boxIndexCount, static_cast<uint32_t>(boxInstances.size()));
                profiler.RecordDraw(boxIndexCount / 3, static_cast<uint32_t>(boxInstances.size()));

                // 2b. Draw Spheres with White Texture
                cmd.SetTexture(3, defaultWhiteTex, defaultSampler);
                cmd.SetVertexBuffer(0, sphereVB);
                cmd.SetVertexBuffer(1, sphereInstanceVB);
                cmd.SetIndexBuffer(sphereIB);
                cmd.DrawIndexed(sphereIndexCount, static_cast<uint32_t>(sphereInstances.size()));
                profiler.RecordDraw(sphereIndexCount / 3, static_cast<uint32_t>(sphereInstances.size()));

                // 2c. Draw Terrain with Checkerboard
                cmd.SetTexture(3, checkerTexture, checkerSampler);
                cmd.SetVertexBuffer(0, terrainVB);
                cmd.SetVertexBuffer(1, terrainInstanceVB);
                cmd.SetIndexBuffer(terrainIB);
                cmd.DrawIndexed(terrainIndexCount, 1);
                profiler.RecordDraw(terrainIndexCount / 3, 1);

                device.SubmitFrame(*frame);
            }

            profiler.EndFrame();

            // Telemetry in Window Title
            static int frameCounter = 0;
            if (++frameCounter % 15 == 0) {
                const auto& stats = profiler.GetStats();
                std::string title = "Sunshine Renderer — Phase 11: Cascaded Shadows | FPS: " +
                    std::to_string(static_cast<int>(std::round(stats.fps))) +
                    " | Frame: " + std::to_string(stats.avgFrameTimeMs).substr(0, 4) + " ms" +
                    " | Draws: " + std::to_string(stats.drawCalls) +
                    " | Tris: " + std::to_string(stats.triangleCount) +
                    " | Sun: " + (sunAnimationPaused ? "PAUSED" : "ACTIVE") +
                    " | Blend: " + (blendBandEnabled ? "ON" : "OFF") +
                    " | DebugTint: " + (debugColorsEnabled ? "ON (R/G/B/Y)" : "OFF");
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
