# ☀️ Sunshine Renderer

> **"OpenGL Simplicity, Vulkan Performance."**  
> A high-performance, modern C++20 graphics engine and Render Hardware Interface (RHI) built on pure Vulkan.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![API](https://img.shields.io/badge/Vulkan-1.2%2B-red.svg)](https://www.vulkan.org/)

---

## 🌟 Highlights

- **Clean Architectural Abstraction:**
  - Zero raw `Vk*` types leaked into public headers.
  - Type-safe, intuitive C++20 API (`Device`, `CommandContext`, `Pipeline`, `Buffer`, `Texture`, `ShadowMap`).
- **Modern Vulkan RHI Backend:**
  - Modern dynamic rendering & subpass dependencies.
  - Automatic pipeline caching with deterministic descriptor hashing.
  - Double-buffered uniform updates with zero synchronization stalls.
- **Physical Based Rendering (PBR):**
  - Cook-Torrance microfacet specular BRDF (GGX / Smith / Fresnel-Schlick).
  - Metallic-Roughness workflow standard.
  - Tangent-space normal mapping with automatic MikkTSpace fallback.
  - ACES Filmic Tone Mapping and sRGB gamma correction.
- **Real-Time Dynamic Shadows:**
  - Dedicated 2048x2048 `D32_SFLOAT` offscreen depth render pass.
  - Two-pass rendering architecture (`cmd.BeginShadowPass(shadowMap)` / `cmd.EndShadowPass()`).
  - Hardware Depth Bias & slope scaling to completely eliminate shadow acne.
  - 3x3 Percentage Closer Filtering (PCF) with soft penumbra.
  - Border clamping (`VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE`) outside light frustum.
- **glTF 2.0 3D Asset Loading:**
  - Native loading of `.gltf` and `.glb` files (`tinygltf`).
  - Parent-child transform hierarchies (`SceneNode`, `Transform`).
  - Full material texture resolution with automated fallbacks.
- **Hardware Instancing:**
  - Multi-binding per-instance vertex inputs.
  - Rendering thousands of independent 3D meshes in a single draw call.
- **Real-Time Live Shader Hot-Reloading:**
  - Background file system monitoring with runtime pipeline swap (`Pipeline::ReplaceRHIPipeline`).
  - Zero-crash fallback protection if shader compilation errors occur.
- **Built-in Telemetry Profiler:**
  - CPU/GPU frame timing, FPS tracking, draw calls, and triangle counter.

---

## 📁 Repository Structure

```
Sunshine renderer/
├── engine/                # Public Sunshine Engine Core
│   ├── include/sunshine/  # Clean, frameworkless C++20 public headers
│   │   ├── rhi/           # Hardware-agnostic RHI interfaces
│   └── src/               # Engine implementations (PIMPL)
├── backend/               # Low-Level Driver Implementations
│   └── vulkan/            # Pure Vulkan 1.2+ backend & RHI
├── assets/                # Models, textures, and demo assets
│   └── models/            # glTF assets (e.g. DamagedHelmet.glb)
├── examples/              # Incremental progressive demos
│   ├── 00_clear_screen/   # Window lifecycle & clean swapchain clear
│   ├── 01_triangle/       # Basic vertex buffer & pipeline execution
│   ├── 02_textured_model/ # 3D cube with texture mapping & depth buffer
│   ├── 03_hot_reload/     # Live shader editing without restarting app
│   ├── 04_benchmark/      # Hardware instancing & Cook-Torrance PBR
│   ├── 05_gltf_model/     # glTF 2.0 loading, scene graph & normal mapping
│   └── 06_shadows/        # 2048x2048 directional shadow map & PCF soft shadows
├── tools/                 # Shader compilation utilities
├── PROGRESS.md            # Detailed progress log across all phases
├── ROADMAP2.md            # Architecture roadmap for future phases
├── LICENSE-MIT            # MIT License
└── LICENSE-APACHE         # Apache 2.0 License
```

---

## 🛠️ Building the Project

### Prerequisites
- **C++20 Compiler:** MSVC (Visual Studio 2022+), GCC 11+, or Clang 13+
- **CMake:** Version 3.20 or newer
- **Vulkan Driver:** Any modern GPU supporting Vulkan 1.2+ (Intel UHD, AMD Radeon, NVIDIA GeForce, Apple Silicon via MoltenVK)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/Lixsuz86/sunshine-renderer.git
cd sunshine-renderer

# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile all targets
cmake --build build --config Release
```

### Running the Demos

```bash
# Run Phase 7 Real-Time Soft Shadows Demo:
build\Release\06_shadows.exe

# Run Phase 6 glTF 3D PBR Model Demo:
build\Release\05_gltf_model.exe

# Run Phase 5 Instanced Benchmark:
build\Release\04_benchmark.exe
```

---

## 📜 License

Sunshine Renderer is dual-licensed under either:

- **[MIT License](LICENSE-MIT)** ([http://opensource.org/licenses/MIT](http://opensource.org/licenses/MIT))
- **[Apache License, Version 2.0](LICENSE-APACHE)** ([http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0))

at your option.
