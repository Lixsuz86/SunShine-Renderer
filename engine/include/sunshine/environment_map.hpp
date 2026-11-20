#pragma once

#include "types.hpp"
#include "texture.hpp"
#include <memory>
#include <string>

namespace Sunshine {

class Device;

namespace Internal {
    struct EnvironmentMapImpl;
}

/**
 * @brief Configuration settings for generating an IBL Environment Map.
 */
struct EnvironmentMapDesc {
    uint32_t skyboxResolution{512};         ///< Resolution per face of the original environment cubemap.
    uint32_t irradianceResolution{32};      ///< Resolution per face of the diffuse irradiance cubemap.
    uint32_t prefilteredResolution{128};    ///< Base resolution per face of the specular pre-filtered cubemap.
    uint32_t prefilteredMipLevels{5};       ///< Number of mip levels for pre-filtering roughness levels (0.0 to 1.0).
    uint32_t brdfLutResolution{512};        ///< Resolution of the 2D BRDF integration LUT texture.
};

/**
 * @brief Manages Image-Based Lighting (IBL) resources for physically-based rendering.
 * 
 * Encapsulates:
 * 1. HDRI Skybox Environment Cubemap (VK_IMAGE_VIEW_TYPE_CUBE)
 * 2. Diffuse Irradiance Cubemap (cosine-weighted hemispherical convolution)
 * 3. Specular Pre-filtered Environment Cubemap (GGX importance sampling across mip levels)
 * 4. 2D BRDF Integration Look-Up Table (Split-Sum approximation)
 * 5. Hardware Samplers (Trilinear mipmapped cubemap sampler & 2D clamp-to-edge LUT sampler)
 */
class EnvironmentMap {
public:
    /**
     * @brief Loads an HDR equirectangular file (.hdr) and generates full IBL textures.
     * @param device Graphics device for GPU resource allocation.
     * @param hdrFilePath Path to the .hdr image file on disk.
     * @param desc Configuration options for resolution and mipmaps.
     */
    EnvironmentMap(Device& device, const std::string& hdrFilePath, const EnvironmentMapDesc& desc = {});

    /**
     * @brief Generates a procedural physically-based atmospheric HDR sky and full IBL textures.
     * @param device Graphics device for GPU resource allocation.
     * @param desc Configuration options for resolution and mipmaps.
     */
    explicit EnvironmentMap(Device& device, const EnvironmentMapDesc& desc = {});

    ~EnvironmentMap();

    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(EnvironmentMap&&) noexcept;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept;

    /**
     * @brief Factory helper to load an HDR image file.
     */
    static std::shared_ptr<EnvironmentMap> LoadHDR(Device& device, const std::string& hdrFilePath, const EnvironmentMapDesc& desc = {});

    /**
     * @brief Factory helper to create a procedural atmospheric HDR sky.
     */
    static std::shared_ptr<EnvironmentMap> CreateProcedural(Device& device, const EnvironmentMapDesc& desc = {});

    /**
     * @brief Returns the 360-degree environment cubemap texture for skybox background rendering.
     */
    Texture GetSkyboxTexture() const;

    /**
     * @brief Returns the diffuse irradiance cubemap for ambient diffuse PBR lighting.
     */
    Texture GetIrradianceTexture() const;

    /**
     * @brief Returns the pre-filtered specular cubemap with roughness mip levels.
     */
    Texture GetPrefilteredTexture() const;

    /**
     * @brief Returns the 2D BRDF integration LUT texture for Split-Sum specular lighting.
     */
    Texture GetBRDFLUTTexture() const;

    /**
     * @brief Returns the trilinear cubemap sampler (minLod=0.0, maxLod=16.0, clamp-to-edge).
     */
    Sampler GetCubemapSampler() const;

    /**
     * @brief Returns the bilinear 2D sampler configured for BRDF LUT sampling.
     */
    Sampler GetLUTSampler() const;

private:
    std::unique_ptr<Internal::EnvironmentMapImpl> m_impl;
};

} // namespace Sunshine
