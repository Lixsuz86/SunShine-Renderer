# ☀️ Sunshine Renderer

> **"OpenGL Simplicity, Vulkan Performance."**  
> A high-performance, frameworkless modern C++20 3D graphics engine and Render Hardware Interface (RHI) built on pure Vulkan.

[![Release](https://img.shields.io/badge/Release-v1.0.0-green.svg)](https://github.com/Lixsuz86/SunShine-Renderer/releases/tag/v1.0.0)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![API](https://img.shields.io/badge/Vulkan-1.2%2B-red.svg)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()

---

## 📖 Overview

**Sunshine Renderer** is an independent, custom 3D graphics engine written from the ground up in modern C++20. It bridges the historical chasm between **OpenGL's developer ergonomics** and **Vulkan's zero-driver overhead and multi-threaded horsepower**.

By strictly following the **Frameworkless Principle**, Sunshine does not depend on third-party rendering engines (such as bgfx, Diligent, or Filament). Every layer—from memory allocation and descriptor management to DAG pass graphs, compute pipelines, and multi-threaded command recording—is custom-crafted directly on top of raw Vulkan APIs.

### 🛡️ The Zero-Vulkan-Leak Guarantee
Sunshine's public API is completely insulated from driver internals:
- **Zero raw Vulkan handles** (`VkInstance`, `VkDevice`, `VkCommandBuffer`, `VkPipeline`, etc.) leak into public headers.
- **Deterministic RAII Lifecycle:** Every GPU resource is automatically cleaned up in its destructor; no manual `vkDestroy*` or dangling memory leaks.
- **Actionable Diagnostics:** Kriptik numerical `VkResult` codes are translated into descriptive `GraphicsException` diagnoses with exact file, line, function, and actionable developer recommendations.

---

## 🌟 Key Features

### ⚡ 1. Render Hardware Interface (RHI)
- Hardware-agnostic RHI architecture (`IRHIDevice`, `IRHICommandList`, `IRHIPipeline`, `IRHIComputePipeline`, `IRHIBuffer`, `IRHITexture`, `IRHISampler`).
- Pure Vulkan backend implementation utilizing dynamic rendering, synchronization primitives, and subpass dependency automation.
- Transparent double-buffering (`MAX_FRAMES_IN_FLIGHT = 2`) for CPU-to-GPU uniform buffer streaming without pipeline stalls.

### 🎨 2. Physically-Based Rendering (PBR) & glTF 2.0
- **Cook-Torrance Microfacet BRDF:** GGX Normal Distribution Function (NDF), Smith's Schlick-GGX geometric shadowing, and Fresnel-Schlick reflection.
- **Metallic-Roughness Standard:** Full support for Base Color (Albedo), Normal, Metallic-Roughness composite, and Ambient Occlusion (AO) maps.
- **Tangent-Space Normal Mapping:** Vertex-calculated TBN matrices with automated MikkTSpace procedural fallback for models lacking tangents.
- **glTF 2.0 Scene Graph:** Native `.gltf` and `.glb` asset parser (`tinygltf`), parent-child transform hierarchy (`SceneNode`, `Transform`), and multi-material submesh batching.

### 🌑 3. Real-Time Dynamic Soft Shadows
- Dedicated 2048x2048 `VK_FORMAT_D32_SFLOAT` offscreen depth render pass (`Sunshine::ShadowMap`).
- Clean two-pass rendering API: `cmd.BeginShadowPass(shadowMap)` and `cmd.EndShadowPass()`.
- Hardware Depth Bias and slope scaling eliminating surface acne and self-shadow artifacts.
- 3x3 Percentage Closer Filtering (PCF) producing cinematic soft penumbras.
- Hardware border clamping (`VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE`) outside the directional light frustum.

### 📊 4. DAG-Based Render Graph & HDR Bloom
- **Topological DAG Pass Compiler:** Pass dependencies resolved automatically via Kahn's Algorithm.
- **Automated Memory & Layout Transitions:** Subpass dependencies (`EXTERNAL -> 0` and `0 -> EXTERNAL`) automatically transition render targets to `SHADER_READ_ONLY_OPTIMAL` for sampling without manual memory barriers.
- **Physical Render Target Pooling:** Automatic reuse and aliasing of transient offscreen targets across render passes, minimizing GPU memory footprints.
- **Multi-Pass Dual-Kawase / Pyramid HDR Bloom:**
  1. `SceneHDR`: 16-bit float (`RGBA16_FLOAT`) offscreen scene target.
  2. `BrightFilter`: Luma thresholding ($L > 1.0$) isolating high-energy neon highlights.
  3. `BloomDown1` & `BloomDown2`: Hierarchical tent downsampling pyramid (1/4 and 1/8 resolution).
  4. `BloomUp1` & `BloomUp2`: Tent upsampling with additive blending.
  5. `Composite`: HDR scene blending, ACES Filmic tonemapping, and sRGB gamma 2.2 correction.
- **Zero-Overhead Fullscreen Quad:** Procedural `gl_VertexIndex` 3-vertex clip-space triangle (`cmd.Draw(3)`) requiring zero vertex or index buffers.

### 🚀 5. Compute Shaders & Storage Buffers (SSBO)
- Dedicated Compute Pipeline interface: `device.CreateComputePipeline(...)`, `cmd.SetComputePipeline(...)`, and `cmd.Dispatch(x, y, z)`.
- **Zero-Copy SSBO-to-Vertex Aliasing:** `StorageBuffer<T>::AsVertexBuffer()` allows GPU compute simulation buffers to be bound directly as vertex buffers in subsequent graphics passes without round-tripping through CPU memory.
- **Compute-to-Graphics Synchronization:** `cmd.ComputeToGraphicsBarrier()` issues hardware memory barriers linking `COMPUTE_SHADER_BIT` writes to `VERTEX_INPUT_BIT` / `VERTEX_SHADER_BIT` reads.

### 🧵 6. Multi-threaded Command Recording
- **Modern C++20 `WorkerThreadPool`:** Work-stealing thread pool dynamically configured to host CPU hardware concurrency (`std::thread::hardware_concurrency`).
- **Thread-Safe Per-Thread Command Pools:** Each worker thread accesses its own dedicated `VkCommandPool` across active frame slots, eliminating thread synchronization locks during recording.
- **Secondary Command Buffers:** Worker threads concurrently record draw calls into `VK_COMMAND_BUFFER_LEVEL_SECONDARY` buffers (`cmd.RecordParallel(...)`).
- **Single-Call Hardware Injection:** The primary command buffer executes all secondary command buffers in a single Vulkan command (`vkCmdExecuteCommands`).

### 🔥 7. Live Shader Hot-Reloading & Telemetry
- **Background File Watcher:** `Sunshine::ShaderWatcher` monitors shader sources on disk and recompiles them via `glslang` in the background.
- **Zero-Crash Tolerance:** In case of a GLSL compilation or syntax error, the engine continues rendering with the previous working pipeline and prints diagnosis to the console. Once corrected, the pipeline swaps seamlessly on the fly.
- **Real-Time Telemetry:** `Sunshine::Profiler` measuring CPU frame time, GPU rendering time, FPS, draw call counts, and triangle counts.

---

## 💻 Quick Code Example

Here is how clean, modern, and readable a complete rendering loop looks in Sunshine:

```cpp
#include <sunshine/sunshine.hpp>

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Sunshine Hello World", nullptr, nullptr);

    // 1. Initialize Device & Swapchain
    Sunshine::Device device({ .enableValidation = true });
    Sunshine::SwapchainContext swapchain(device, window);

    // 2. Create Geometry & Pipeline
    auto vertexBuffer = device.CreateVertexBuffer(vertices);
    auto pipeline = device.CreatePipeline(pipelineDesc);

    // 3. Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (auto frame = device.BeginFrame(swapchain)) {
            auto& cmd = frame->GetCommandContext();

            cmd.Clear(Sunshine::Color::DarkSlate());
            cmd.SetPipeline(pipeline);
            cmd.SetVertexBuffer(vertexBuffer);
            cmd.Draw(3);
            cmd.EndPass();

            device.SubmitFrame(*frame);
        }
    }

    device.WaitIdle();
    glfwTerminate();
}
```

---

## 🎮 Interactive Demos Showcase

Sunshine Renderer ships with 9 progressive, fully interactive examples:

| Executable | Highlight / Milestone | Controls |
|:---|:---|:---|
| **`00_clear_screen.exe`** | Window lifecycle, swapchain acquisition, clean Vulkan clear | `Esc` to exit |
| **`01_triangle.exe`** | RHI abstraction, VertexBuffer, custom shader stages | Interactive window |
| **`02_textured_model.exe`** | 3D Camera, depth testing, texture mapping, samplers | Continuous 3D rotation |
| **`03_hot_reload.exe`** | Live shader hot-reloading with zero-crash tolerance | Edit GLSL on the fly |
| **`04_benchmark.exe`** | Cook-Torrance PBR, 5,000 instanced spheres in 1 draw call | `Space` toggles post-processing |
| **`05_gltf_model.exe`** | Khronos glTF 2.0 DamagedHelmet, normal mapping, PBR textures | Mouse drag = Orbit, Scroll = Zoom |
| **`06_shadows.exe`** | 2048x2048 Directional Shadow Map, 3x3 PCF soft penumbra | Mouse drag = Orbit, `Space` pauses sun |
| **`07_render_graph_bloom.exe`** | DAG RenderGraph, HDR RGBA16F, Dual-Kawase Bloom pyramid | `B` toggles bloom, `+/-` adjusts intensity |
| **`08_gpu_particles.exe`** | 131,072 GPU Particles, Compute SSBO, Multi-threaded recording | `1/2/3` modes, `T` toggles multi-threading |

---

## 📁 Repository Structure

```
Sunshine Renderer/
├── engine/                        # Public Sunshine Engine Core
│   ├── include/sunshine/          # Clean, frameworkless C++20 public headers
│   │   ├── rhi/                   # Hardware-agnostic RHI interfaces
│   │   ├── sunshine.hpp           # Master umbrella engine header
│   │   ├── device.hpp             # Device & resource creation API
│   │   ├── command_context.hpp    # Primary & compute command recording
│   │   ├── secondary_command_context.hpp # Multi-threaded secondary recording
│   │   ├── render_graph.hpp       # DAG framegraph compiler & pooling
│   │   ├── render_target.hpp      # Offscreen HDR color/depth render targets
│   │   ├── compute_pipeline.hpp   # Compute pipeline abstraction
│   │   ├── thread_pool.hpp        # Modern C++20 worker thread pool
│   │   ├── shadow_map.hpp         # Real-time shadow map target
│   │   └── model.hpp              # glTF 2.0 asset & scene graph loader
│   └── src/                       # Engine implementation (PIMPL)
├── backend/                       # Low-Level Driver Implementations
│   └── vulkan/                    # Pure Vulkan 1.2+ backend & RHI
│       ├── rhi/                   # Vulkan implementations of RHI interfaces
│       ├── vulkan_context.hpp     # Instance, physical & logical device management
│       └── vulkan_swapchain.hpp   # Double/triple buffering swapchain
├── assets/                        # 3D Models, textures, and demo assets
│   └── models/                    # Khronos DamagedHelmet.glb
├── examples/                      # 9 Complete progressive standalone examples
│   ├── 00_clear_screen/
│   ├── 01_triangle/
│   ├── 02_textured_model/
│   ├── 03_hot_reload/
│   ├── 04_benchmark/
│   ├── 05_gltf_model/
│   ├── 06_shadows/
│   ├── 07_render_graph_bloom/
│   └── 08_gpu_particles/
├── tools/                         # Offline and runtime glslang compilation tools
├── PROGRESS.md                    # Comprehensive development log across all phases
├── ROADMAP2.md                    # Roadmap 2.0 architectural records
├── LICENSE-MIT                    # MIT License
└── LICENSE-APACHE                 # Apache 2.0 License
```

---

## 🛠️ Building the Project

### Prerequisites
- **C++20 Compiler:** MSVC (Visual Studio 2022 v17.0+), GCC 11+, or Clang 13+
- **CMake:** Version 3.24 or newer
- **Vulkan Driver:** Any GPU supporting Vulkan 1.2+ (Intel UHD/Iris, NVIDIA GeForce, AMD Radeon, or Apple Silicon via MoltenVK)

### Build Instructions

```bash
# 1. Clone the repository
git clone https://github.com/Lixsuz86/SunShine-Renderer.git
cd SunShine-Renderer

# 2. Configure build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build all targets
cmake --build build --config Release
```

### Running Examples

```powershell
# Run GPU Particles & Multi-threaded Command Recording:
.\build\Release\08_gpu_particles.exe

# Run RenderGraph & HDR Bloom Demo:
.\build\Release\07_render_graph_bloom.exe

# Run Real-Time Dynamic Soft Shadows Demo:
.\build\Release\06_shadows.exe

# Run glTF 2.0 PBR 3D Model Loader Demo:
.\build\Release\05_gltf_model.exe
```

---

## 🗺️ Roadmap & Sürüm Bilgisi

- [x] **Faz 0 — Vulkan Başlatma & GLFW Entegrasyonu**
- [x] **Faz 1 — Sunshine API v0 (Frame Yaşam Döngüsü & Double Buffering)**
- [x] **Faz 2 — RHI Soyutlama Katmanı & İlk Üçgen**
- [x] **Faz 3 — 3D Kamera, Depth Buffer, Doku & Sampler**
- [x] **Faz 4 — Kullanılabilirlik & Canlı Shader Hot-Reloading**
- [x] **Faz 5 — Performans Doğrulama, Cook-Torrance PBR & Instancing**
- [x] **Faz 6 — Sahne Yöneticisi & 3D glTF 2.0 Model Yükleme**
- [x] **Faz 7 — İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping)**
- [x] **Faz 8 — DAG Render Graph & İleri Post-Processing (HDR Bloom)**
- [x] **Faz 9 — Compute Shader & Çoklu İş Parçacıklı Komut Kaydı**
- [x] **v1.0.0 Resmi Sürümü Yayınlandı! 🎉**

*(Not: Eski Faz 10 - Dear ImGui Entegrasyonu & Görsel Editör, v1.0.0 çekirdek stabilitesini dondurmak adına gelecek **v2.0 Yol Haritasına** devredilmiştir.)*

---

## 📜 License

Sunshine Renderer is dual-licensed under either:

- **[MIT License](LICENSE-MIT)** ([http://opensource.org/licenses/MIT](http://opensource.org/licenses/MIT))
- **[Apache License, Version 2.0](LICENSE-APACHE)** ([http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0))

at your option.
