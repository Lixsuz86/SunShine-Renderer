include(FetchContent)

# If dependencies are already downloaded in build/_deps, reuse them directly without running git subbuilds
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/glfw-src")
    set(FETCHCONTENT_SOURCE_DIR_GLFW "${CMAKE_BINARY_DIR}/_deps/glfw-src")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/vulkan_headers-src")
    set(FETCHCONTENT_SOURCE_DIR_VULKAN_HEADERS "${CMAKE_BINARY_DIR}/_deps/vulkan_headers-src")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/volk-src")
    set(FETCHCONTENT_SOURCE_DIR_VOLK "${CMAKE_BINARY_DIR}/_deps/volk-src")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/glm-src")
    set(FETCHCONTENT_SOURCE_DIR_GLM "${CMAKE_BINARY_DIR}/_deps/glm-src")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/stb-src")
    set(FETCHCONTENT_SOURCE_DIR_STB "${CMAKE_BINARY_DIR}/_deps/stb-src")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/tinygltf-src")
    set(FETCHCONTENT_SOURCE_DIR_TINYGLTF "${CMAKE_BINARY_DIR}/_deps/tinygltf-src")
endif()

# 1. GLFW
message(STATUS "Fetching GLFW...")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# 2. Vulkan-Headers (guarantees official, up-to-date Vulkan headers across all platforms)
message(STATUS "Fetching Vulkan-Headers...")
FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG        v1.4.304
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vulkan_headers)

# 3. volk (meta-loader for Vulkan)
message(STATUS "Fetching volk...")
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        vulkan-sdk-1.3.296.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(volk)

# 4. GLM (OpenGL Mathematics for vectors and matrices)
message(STATUS "Fetching GLM...")
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# 5. stb (stb_image for texture loading)
message(STATUS "Fetching stb...")
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

if(NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# 6. tinygltf (glTF 2.0 loader)
message(STATUS "Fetching tinygltf...")
set(TINYGLTF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinygltf)

if(NOT TARGET tinygltf)
    add_library(tinygltf INTERFACE)
    target_include_directories(tinygltf INTERFACE ${tinygltf_SOURCE_DIR})
endif()
