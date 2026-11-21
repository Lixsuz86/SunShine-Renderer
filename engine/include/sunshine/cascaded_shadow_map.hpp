#pragma once

#include "types.hpp"
#include "texture.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Sunshine {

class Device;
class Camera;

namespace Internal {
    struct CascadedShadowMapImpl;
}

/**
 * @brief Manages open-world Cascaded Shadow Maps (CSM) across multiple frustum depth splits.
 * 
 * Features:
 * 1. Logarithmic + Practical camera frustum splitting across 3-4 cascades.
 * 2. Tight-fit orthographic projections with sub-pixel texel snapping (zero shadow shimmering).
 * 3. 2D Texture Array (VK_IMAGE_VIEW_TYPE_2D_ARRAY) depth buffer storing all slices in one resource.
 * 4. Separate per-layer framebuffers for multi-pass cascade recording without geometry shader overhead.
 * 5. Smooth cascade blend band transitions eliminating visual seams across distance.
 */
class CascadedShadowMap {
public:
    static constexpr uint32_t DEFAULT_CASCADE_COUNT = 4;
    static constexpr uint32_t DEFAULT_RESOLUTION = 2048;

    /**
     * @brief Allocates a cascaded shadow map with the given resolution and cascade slices.
     * @param device Parent graphics Device.
     * @param resolution Resolution of each cascade slice (e.g. 2048x2048).
     * @param cascadeCount Number of cascade depth slices (typically 3 or 4).
     */
    CascadedShadowMap(Device& device, uint32_t resolution = DEFAULT_RESOLUTION, uint32_t cascadeCount = DEFAULT_CASCADE_COUNT);
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;
    CascadedShadowMap(CascadedShadowMap&&) noexcept;
    CascadedShadowMap& operator=(CascadedShadowMap&&) noexcept;

    uint32_t GetResolution() const noexcept;
    uint32_t GetCascadeCount() const noexcept;

    /**
     * @brief Calculates split distances and light-space view-projection matrices with texel snapping.
     * @param camera Main scene camera.
     * @param lightDir Normalized vector pointing towards the directional light.
     * @param maxShadowDistance Maximum shadow rendering distance in world units (default 150m).
     * @param lambda Blend between logarithmic (1.0) and uniform (0.0) split scheme (default 0.85).
     */
    void UpdateCascades(const Camera& camera, const glm::vec3& lightDir, float maxShadowDistance = 150.0f, float lambda = 0.85f);

    /**
     * @brief Returns the light-space view-projection matrix for a specific cascade.
     */
    const glm::mat4& GetLightSpaceMatrix(uint32_t cascadeIndex) const;

    /**
     * @brief Returns all light-space matrices for uniform buffer upload.
     */
    const std::vector<glm::mat4>& GetLightSpaceMatrices() const;

    /**
     * @brief Returns the view-space split depths for all cascades.
     */
    const std::vector<float>& GetCascadeSplitDepths() const;

    /**
     * @brief Returns the split depths packed into a glm::vec4 (up to 4 cascades) for GLSL uniforms.
     */
    glm::vec4 GetCascadeSplitDepthsVec4() const;

    /**
     * @brief Returns the 2D Texture Array depth texture for binding into scene shaders.
     */
    Texture GetTexture() const;

    /**
     * @brief Returns the clamp-to-border shadow sampler with linear filtering.
     */
    Sampler GetSampler() const;

private:
    friend class Device;
    friend class CommandContext;
    Internal::CascadedShadowMapImpl* GetImpl() const noexcept { return m_impl.get(); }

    std::unique_ptr<Internal::CascadedShadowMapImpl> m_impl;
};

} // namespace Sunshine
