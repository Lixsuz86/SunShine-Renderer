cmake_minimum_required(VERSION 3.24)

project(
    SunshineRenderer
    VERSION 0.2.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE indexing
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler-specific flags
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
include(cmake/Dependencies.cmake)

# Ensure volk finds the fetched Vulkan headers
if(TARGET volk AND TARGET Vulkan::Headers)
    target_link_libraries(volk PUBLIC Vulkan::Headers)
endif()

# ------------------------------------------------------------------------------
# Backend: Vulkan Low-Level Library (Private to the Engine)
# ------------------------------------------------------------------------------
add_library(sunshine_backend_vulkan STATIC
    backend/vulkan/vulkan_common.hpp
    backend/vulkan/vulkan_debug.hpp
    backend/vulkan/vulkan_debug.cpp
    backend/vulkan/vulkan_context.hpp
    backend/vulkan/vulkan_context.cpp
    backend/vulkan/vulkan_swapchain.hpp
    backend/vulkan/vulkan_swapchain.cpp
    backend/vulkan/rhi/vulkan_rhi_buffer.hpp
    backend/vulkan/rhi/vulkan_rhi_buffer.cpp
    backend/vulkan/rhi/vulkan_rhi_shader.hpp
    backend/vulkan/rhi/vulkan_rhi_shader.cpp
    backend/vulkan/rhi/vulkan_rhi_pipeline.hpp
    backend/vulkan/rhi/vulkan_rhi_pipeline.cpp
    backend/vulkan/rhi/vulkan_rhi_command_list.hpp
    backend/vulkan/rhi/vulkan_rhi_command_list.cpp
    backend/vulkan/rhi/vulkan_rhi_texture.hpp
    backend/vulkan/rhi/vulkan_rhi_texture.cpp
    backend/vulkan/rhi/vulkan_rhi_sampler.hpp
    backend/vulkan/rhi/vulkan_rhi_sampler.cpp
    backend/vulkan/rhi/vulkan_rhi_device.hpp
    backend/vulkan/rhi/vulkan_rhi_device.cpp
)

target_include_directories(sunshine_backend_vulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backend
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

target_link_libraries(sunshine_backend_vulkan
    PUBLIC
        volk
        Vulkan::Headers
        glfw
)

# ------------------------------------------------------------------------------
# Frontend: Sunshine Engine API
# ------------------------------------------------------------------------------
add_library(sunshine_engine STATIC
    engine/src/device.cpp
    engine/src/swapchain_context.cpp
    engine/src/command_context.cpp
    engine/src/shader_watcher.cpp
    engine/src/model.cpp
    engine/src/shadow_map.cpp
    engine/src/internal_types.hpp
)

target_include_directories(sunshine_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sunshine_engine
    PRIVATE
        sunshine_backend_vulkan
        stb
        tinygltf
    PUBLIC
        glfw
        glm::glm
)

# ------------------------------------------------------------------------------
# Examples
# ------------------------------------------------------------------------------

# Example 00: Clear Screen
add_executable(00_clear_screen
    examples/00_clear_screen/main.cpp
)

target_link_libraries(00_clear_screen
    PRIVATE
        sunshine_engine
)

set_target_properties(00_clear_screen PROPERTIES
    OUTPUT_NAME "00_clear_screen"
    FOLDER "Examples"
)

# Example 01: Triangle (Phase 2)
add_executable(01_triangle
    examples/01_triangle/main.cpp
)

target_include_directories(01_triangle
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/01_triangle
)

target_link_libraries(01_triangle
    PRIVATE
        sunshine_engine
)

set_target_properties(01_triangle PROPERTIES
    OUTPUT_NAME "01_triangle"
    FOLDER "Examples"
)

# Example 02: Textured Model (Phase 3)
add_executable(02_textured_model
    examples/02_textured_model/main.cpp
)

target_include_directories(02_textured_model
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/02_textured_model
)

target_link_libraries(02_textured_model
    PRIVATE
        sunshine_engine
)

set_target_properties(02_textured_model PROPERTIES
    OUTPUT_NAME "02_textured_model"
    FOLDER "Examples"
)

# Example 03: Hot Reload (Phase 4)
add_executable(03_hot_reload
    examples/03_hot_reload/main.cpp
)

target_include_directories(03_hot_reload
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/03_hot_reload
)

target_link_libraries(03_hot_reload
    PRIVATE
        sunshine_engine
)

set_target_properties(03_hot_reload PROPERTIES
    OUTPUT_NAME "03_hot_reload"
    FOLDER "Examples"
)

# Example 04: Benchmark & PBR Stress Test (Phase 5)
add_executable(04_benchmark
    examples/04_benchmark/main.cpp
)

target_include_directories(04_benchmark
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/04_benchmark
)

target_link_libraries(04_benchmark
    PRIVATE
        sunshine_engine
)

set_target_properties(04_benchmark PROPERTIES
    OUTPUT_NAME "04_benchmark"
    FOLDER "Examples"
)

# Example 05: glTF 2.0 PBR Model (Phase 6)
add_executable(05_gltf_model
    examples/05_gltf_model/main.cpp
)

target_include_directories(05_gltf_model
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/05_gltf_model
)

target_link_libraries(05_gltf_model
    PRIVATE
        sunshine_engine
)

set_target_properties(05_gltf_model PROPERTIES
    OUTPUT_NAME "05_gltf_model"
    FOLDER "Examples"
)

# Example 06: Real-Time Soft Shadows (Phase 7)
add_executable(06_shadows
    examples/06_shadows/main.cpp
)

target_include_directories(06_shadows
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/06_shadows
)

target_link_libraries(06_shadows
    PRIVATE
        sunshine_engine
)

set_target_properties(06_shadows PROPERTIES
    OUTPUT_NAME "06_shadows"
    FOLDER "Examples"
)


