#pragma once

#include "texture.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Sunshine {

/**
 * @brief Cook-Torrance PBR Material holding glTF textures and constant parameters.
 */
struct Material {
    std::string name{"DefaultMaterial"};

    // PBR Textures
    std::shared_ptr<Texture> baseColorTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> metallicRoughnessTexture; // G = Roughness, B = Metallic
    std::shared_ptr<Texture> occlusionTexture;         // R = AO
    std::shared_ptr<Texture> emissiveTexture;

    std::shared_ptr<Sampler> sampler;

    // Factors / Coefficients
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    float normalScale{1.0f};
    float occlusionStrength{1.0f};

    // Flags for conditional shader paths
    bool hasBaseColorTexture{false};
    bool hasNormalTexture{false};
    bool hasMetallicRoughnessTexture{false};
    bool hasOcclusionTexture{false};
    bool hasEmissiveTexture{false};
};

} // namespace Sunshine
