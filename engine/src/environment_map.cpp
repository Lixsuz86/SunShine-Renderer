#include "sunshine/environment_map.hpp"
#include "sunshine/device.hpp"
#include "sunshine/error.hpp"

#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <vector>
#include <cmath>
#include <cstring>
#include <thread>
#include <algorithm>
#include <iostream>

namespace Sunshine {

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

inline uint16_t FloatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(float));
    uint32_t sign = (x >> 31) & 0x1;
    int32_t exp = ((x >> 23) & 0xff) - 127 + 15;
    uint32_t mant = x & 0x7fffff;
    if (exp <= 0) {
        return static_cast<uint16_t>(sign << 15);
    } else if (exp >= 31) {
        return static_cast<uint16_t>((sign << 15) | 0x7c00);
    }
    return static_cast<uint16_t>((sign << 15) | (exp << 10) | (mant >> 13));
}

// Convert face and normalized coordinates (u, v in [-1, 1]) to 3D direction
inline glm::vec3 CubeUVToDirection(uint32_t face, float u, float v) {
    glm::vec3 dir{0.0f};
    switch (face) {
        case 0: dir = glm::vec3( 1.0f,   -v,   -u); break; // +X
        case 1: dir = glm::vec3(-1.0f,   -v,    u); break; // -X
        case 2: dir = glm::vec3(    u, 1.0f,    v); break; // +Y
        case 3: dir = glm::vec3(    u,-1.0f,   -v); break; // -Y
        case 4: dir = glm::vec3(    u,   -v, 1.0f); break; // +Z
        case 5: dir = glm::vec3(   -u,   -v,-1.0f); break; // -Z
    }
    return glm::normalize(dir);
}

// Bilinear sample from equirectangular HDR buffer (float4 RGBA)
inline glm::vec3 SampleEquirectangular(const float* pixels, uint32_t width, uint32_t height, const glm::vec3& dir) {
    float phi = std::atan2(dir.z, dir.x);
    float theta = std::asin(std::clamp(dir.y, -1.0f, 1.0f));

    float u = (phi / TWO_PI) + 0.5f;
    float v = 0.5f - (theta / PI);

    // Bilinear filtering
    float x = u * static_cast<float>(width) - 0.5f;
    float y = v * static_cast<float>(height) - 0.5f;

    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = x - static_cast<float>(x0);
    float fy = y - static_cast<float>(y0);

    // Wrap horizontally, clamp vertically
    auto wrapX = [width](int px) {
        return (px % static_cast<int>(width) + width) % width;
    };
    auto clampY = [height](int py) {
        return std::clamp(py, 0, static_cast<int>(height) - 1);
    };

    uint32_t ix0 = wrapX(x0);
    uint32_t ix1 = wrapX(x1);
    uint32_t iy0 = clampY(y0);
    uint32_t iy1 = clampY(y1);

    auto getPixel = [pixels, width](uint32_t px, uint32_t py) -> glm::vec3 {
        size_t idx = (static_cast<size_t>(py) * width + px) * 4;
        return glm::vec3(pixels[idx], pixels[idx + 1], pixels[idx + 2]);
    };

    glm::vec3 c00 = getPixel(ix0, iy0);
    glm::vec3 c10 = getPixel(ix1, iy0);
    glm::vec3 c01 = getPixel(ix0, iy1);
    glm::vec3 c11 = getPixel(ix1, iy1);

    glm::vec3 top = glm::mix(c00, c10, fx);
    glm::vec3 bottom = glm::mix(c01, c11, fx);
    return glm::mix(top, bottom, fy);
}

// Low-discrepancy Hammersley sequence
inline float RadicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

inline glm::vec2 Hammersley(uint32_t i, uint32_t N) {
    return glm::vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
}

inline glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, const glm::vec3& N, float roughness) {
    float a = roughness * roughness;
    float phi = TWO_PI * Xi.x;
    float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

    // Spherical to Cartesian in tangent space
    glm::vec3 H{
        sinTheta * std::cos(phi),
        sinTheta * std::sin(phi),
        cosTheta
    };

    // Tangent to World space
    glm::vec3 up = std::abs(N.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangentX = glm::normalize(glm::cross(up, N));
    glm::vec3 tangentY = glm::cross(N, tangentX);

    return glm::normalize(tangentX * H.x + tangentY * H.y + N * H.z);
}

inline float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness * roughness) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

inline float GeometrySmith(const glm::vec3& N, const glm::vec3& V, const glm::vec3& L, float roughness) {
    float NdotV = std::max(glm::dot(N, V), 0.0f);
    float NdotL = std::max(glm::dot(N, L), 0.0f);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Procedural high-dynamic-range atmospheric sky generator
std::vector<float> GenerateProceduralHDRImage(uint32_t width, uint32_t height) {
    std::vector<float> pixels(static_cast<size_t>(width) * height * 4);
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.4f, 0.45f, 0.8f));
    glm::vec3 sunColor = glm::vec3(18.0f, 14.5f, 10.0f); // Bright HDR sun
    glm::vec3 skyZenith = glm::vec3(0.12f, 0.32f, 0.72f);
    glm::vec3 skyHorizon = glm::vec3(0.65f, 0.80f, 0.98f);
    glm::vec3 groundColor = glm::vec3(0.10f, 0.08f, 0.06f);

    for (uint32_t y = 0; y < height; ++y) {
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        float theta = (0.5f - v) * PI; // latitude in [-PI/2, PI/2]

        for (uint32_t x = 0; x < width; ++x) {
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            float phi = (u - 0.5f) * TWO_PI;

            glm::vec3 dir = glm::normalize(glm::vec3(
                std::cos(theta) * std::cos(phi),
                std::sin(theta),
                std::cos(theta) * std::sin(phi)
            ));

            glm::vec3 color{0.0f};
            if (dir.y >= 0.0f) {
                // Sky gradient with atmospheric haze
                float horizonFactor = std::pow(1.0f - dir.y, 2.5f);
                color = glm::mix(skyZenith, skyHorizon, horizonFactor);

                // Add sun disk and glare
                float cosAlpha = glm::dot(dir, sunDir);
                if (cosAlpha > 0.0f) {
                    float sunDisk = std::exp(800.0f * (cosAlpha - 1.0f));
                    float sunGlow = std::exp(12.0f * (cosAlpha - 1.0f)) * 0.4f;
                    color += (sunDisk + sunGlow) * sunColor;
                }
            } else {
                // Ground with smooth horizon blend
                float groundFactor = std::clamp(-dir.y * 8.0f, 0.0f, 1.0f);
                color = glm::mix(skyHorizon * 0.5f, groundColor, groundFactor);
            }

            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            pixels[idx + 0] = color.r;
            pixels[idx + 1] = color.g;
            pixels[idx + 2] = color.b;
            pixels[idx + 3] = 1.0f;
        }
    }

    return pixels;
}

} // anonymous namespace

namespace Internal {

struct EnvironmentMapImpl {
    Texture skyboxTexture;
    Texture irradianceTexture;
    Texture prefilteredTexture;
    Texture brdfLutTexture;
    Sampler cubemapSampler;
    Sampler lutSampler;
};

} // namespace Internal

EnvironmentMap::EnvironmentMap(Device& device, const std::string& hdrFilePath, const EnvironmentMapDesc& desc)
    : m_impl(std::make_unique<Internal::EnvironmentMapImpl>()) 
{
    std::cout << "[EnvironmentMap] Initializing IBL from HDR asset: " << hdrFilePath << "..." << std::endl;

    int width = 0, height = 0, channels = 0;
    float* hdrPixels = nullptr;

    if (!hdrFilePath.empty()) {
        hdrPixels = stbi_loadf(hdrFilePath.c_str(), &width, &height, &channels, 4);
    }

    std::vector<float> proceduralPixels;
    if (!hdrPixels) {
        if (!hdrFilePath.empty()) {
            std::cout << "[EnvironmentMap] Note: Could not open '" << hdrFilePath << "'. Generating procedural HDR physical sky..." << std::endl;
        } else {
            std::cout << "[EnvironmentMap] Generating procedural HDR physical sky..." << std::endl;
        }
        width = 1024;
        height = 512;
        proceduralPixels = GenerateProceduralHDRImage(width, height);
        hdrPixels = proceduralPixels.data();
    }

    const uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency());

    // -------------------------------------------------------------------------
    // 1. Generate 360 Skybox Environment Cubemap (512x512 per face)
    // -------------------------------------------------------------------------
    const uint32_t skyRes = desc.skyboxResolution;
    std::vector<uint16_t> skyboxHalfData(static_cast<size_t>(skyRes) * skyRes * 6 * 4);

    {
        std::vector<std::thread> workers;
        for (uint32_t t = 0; t < numThreads; ++t) {
            workers.emplace_back([&, t]() {
                for (uint32_t face = t; face < 6; face += numThreads) {
                    for (uint32_t y = 0; y < skyRes; ++y) {
                        float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(skyRes)) - 1.0f;
                        for (uint32_t x = 0; x < skyRes; ++x) {
                            float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(skyRes)) - 1.0f;
                            glm::vec3 dir = CubeUVToDirection(face, u, v);
                            glm::vec3 sample = SampleEquirectangular(hdrPixels, width, height, dir);

                            size_t idx = (static_cast<size_t>(face) * skyRes * skyRes + y * skyRes + x) * 4;
                            skyboxHalfData[idx + 0] = FloatToHalf(sample.r);
                            skyboxHalfData[idx + 1] = FloatToHalf(sample.g);
                            skyboxHalfData[idx + 2] = FloatToHalf(sample.b);
                            skyboxHalfData[idx + 3] = FloatToHalf(1.0f);
                        }
                    }
                }
            });
        }
        for (auto& w : workers) w.join();
    }

    m_impl->skyboxTexture = device.CreateTextureCube(skyRes, skyRes, skyboxHalfData.data(), RHI::TextureFormat::RGBA16_FLOAT, 1);
    std::cout << "[EnvironmentMap] Skybox Cubemap created: " << skyRes << "x" << skyRes << " (6 faces, RGBA16F)." << std::endl;

    // -------------------------------------------------------------------------
    // 2. Generate Diffuse Irradiance Cubemap (32x32 per face)
    // -------------------------------------------------------------------------
    const uint32_t irradRes = desc.irradianceResolution;
    std::vector<uint16_t> irradianceHalfData(static_cast<size_t>(irradRes) * irradRes * 6 * 4);

    {
        std::vector<std::thread> workers;
        for (uint32_t t = 0; t < numThreads; ++t) {
            workers.emplace_back([&, t]() {
                for (uint32_t face = t; face < 6; face += numThreads) {
                    for (uint32_t y = 0; y < irradRes; ++y) {
                        float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(irradRes)) - 1.0f;
                        for (uint32_t x = 0; x < irradRes; ++x) {
                            float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(irradRes)) - 1.0f;
                            glm::vec3 N = CubeUVToDirection(face, u, v);

                            glm::vec3 up = std::abs(N.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
                            glm::vec3 tangentX = glm::normalize(glm::cross(up, N));
                            glm::vec3 tangentY = glm::cross(N, tangentX);

                            glm::vec3 irradiance{0.0f};
                            uint32_t nrSamples = 0;
                            const float sampleDelta = 0.05f;

                            for (float phi = 0.0f; phi < TWO_PI; phi += sampleDelta) {
                                for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta) {
                                    glm::vec3 tangentSample(
                                        std::sin(theta) * std::cos(phi),
                                        std::sin(theta) * std::sin(phi),
                                        std::cos(theta)
                                    );
                                    glm::vec3 sampleVec = tangentX * tangentSample.x + tangentY * tangentSample.y + N * tangentSample.z;
                                    glm::vec3 color = SampleEquirectangular(hdrPixels, width, height, sampleVec);

                                    irradiance += color * std::cos(theta) * std::sin(theta);
                                    nrSamples++;
                                }
                            }

                            irradiance = PI * irradiance * (1.0f / static_cast<float>(nrSamples));

                            size_t idx = (static_cast<size_t>(face) * irradRes * irradRes + y * irradRes + x) * 4;
                            irradianceHalfData[idx + 0] = FloatToHalf(irradiance.r);
                            irradianceHalfData[idx + 1] = FloatToHalf(irradiance.g);
                            irradianceHalfData[idx + 2] = FloatToHalf(irradiance.b);
                            irradianceHalfData[idx + 3] = FloatToHalf(1.0f);
                        }
                    }
                }
            });
        }
        for (auto& w : workers) w.join();
    }

    m_impl->irradianceTexture = device.CreateTextureCube(irradRes, irradRes, irradianceHalfData.data(), RHI::TextureFormat::RGBA16_FLOAT, 1);
    std::cout << "[EnvironmentMap] Diffuse Irradiance Cubemap created: " << irradRes << "x" << irradRes << " (6 faces, RGBA16F)." << std::endl;

    // -------------------------------------------------------------------------
    // 3. Generate Specular Pre-filtered Cubemap with Mipmaps (128x128, 5 mips)
    // -------------------------------------------------------------------------
    const uint32_t prefRes = desc.prefilteredResolution;
    const uint32_t mipLevels = desc.prefilteredMipLevels;

    size_t totalPrefilterHalfWords = 0;
    for (uint32_t m = 0; m < mipLevels; ++m) {
        uint32_t mipW = std::max(1u, prefRes >> m);
        uint32_t mipH = std::max(1u, prefRes >> m);
        totalPrefilterHalfWords += static_cast<size_t>(mipW) * mipH * 6 * 4;
    }
    std::vector<uint16_t> prefilteredHalfData(totalPrefilterHalfWords);

    size_t mipDataOffset = 0;
    for (uint32_t mip = 0; mip < mipLevels; ++mip) {
        uint32_t mipW = std::max(1u, prefRes >> mip);
        uint32_t mipH = std::max(1u, prefRes >> mip);
        float roughness = (mipLevels == 1) ? 0.0f : static_cast<float>(mip) / static_cast<float>(mipLevels - 1);

        std::vector<std::thread> workers;
        for (uint32_t t = 0; t < numThreads; ++t) {
            workers.emplace_back([&, t, mipW, mipH, roughness, mipDataOffset]() {
                for (uint32_t face = t; face < 6; face += numThreads) {
                    for (uint32_t y = 0; y < mipH; ++y) {
                        float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(mipH)) - 1.0f;
                        for (uint32_t x = 0; x < mipW; ++x) {
                            float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(mipW)) - 1.0f;
                            glm::vec3 N = CubeUVToDirection(face, u, v);
                            glm::vec3 R = N;
                            glm::vec3 V = R;

                            const uint32_t SAMPLE_COUNT = 512;
                            glm::vec3 prefilteredColor{0.0f};
                            float totalWeight = 0.0f;

                            for (uint32_t i = 0; i < SAMPLE_COUNT; ++i) {
                                glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                                glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                                float NdotL = std::max(glm::dot(N, L), 0.0f);
                                if (NdotL > 0.0f) {
                                    prefilteredColor += SampleEquirectangular(hdrPixels, width, height, L) * NdotL;
                                    totalWeight += NdotL;
                                }
                            }

                            prefilteredColor = (totalWeight > 0.0f) ? (prefilteredColor / totalWeight) : glm::vec3(0.0f);

                            size_t pixelIdx = mipDataOffset + (static_cast<size_t>(face) * mipW * mipH + y * mipW + x) * 4;
                            prefilteredHalfData[pixelIdx + 0] = FloatToHalf(prefilteredColor.r);
                            prefilteredHalfData[pixelIdx + 1] = FloatToHalf(prefilteredColor.g);
                            prefilteredHalfData[pixelIdx + 2] = FloatToHalf(prefilteredColor.b);
                            prefilteredHalfData[pixelIdx + 3] = FloatToHalf(1.0f);
                        }
                    }
                }
            });
        }
        for (auto& w : workers) w.join();

        mipDataOffset += static_cast<size_t>(mipW) * mipH * 6 * 4;
    }

    m_impl->prefilteredTexture = device.CreateTextureCube(prefRes, prefRes, prefilteredHalfData.data(), RHI::TextureFormat::RGBA16_FLOAT, mipLevels);
    std::cout << "[EnvironmentMap] Specular Pre-filtered Cubemap created: " << prefRes << "x" << prefRes << " (" << mipLevels << " mips, RGBA16F)." << std::endl;

    // Free loaded HDR pixels if from file
    if (!proceduralPixels.empty()) {
        // procedural vector cleans itself
    } else if (hdrPixels) {
        stbi_image_free(hdrPixels);
        hdrPixels = nullptr;
    }

    // -------------------------------------------------------------------------
    // 4. Generate 2D BRDF Integration Look-Up Table (512x512)
    // -------------------------------------------------------------------------
    const uint32_t lutRes = desc.brdfLutResolution;
    std::vector<uint16_t> brdfHalfData(static_cast<size_t>(lutRes) * lutRes * 4);

    {
        std::vector<std::thread> workers;
        for (uint32_t t = 0; t < numThreads; ++t) {
            workers.emplace_back([&, t]() {
                for (uint32_t y = t; y < lutRes; y += numThreads) {
                    float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(lutRes);
                    for (uint32_t x = 0; x < lutRes; ++x) {
                        float NdotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(lutRes);

                        glm::vec3 V{
                            std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV)),
                            0.0f,
                            NdotV
                        };

                        float A = 0.0f;
                        float B = 0.0f;
                        glm::vec3 N(0.0f, 0.0f, 1.0f);

                        const uint32_t SAMPLE_COUNT = 1024;
                        for (uint32_t i = 0; i < SAMPLE_COUNT; ++i) {
                            glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                            glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                            glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                            float NdotL = std::max(L.z, 0.0f);
                            float NdotH = std::max(H.z, 0.0f);
                            float VdotH = std::max(glm::dot(V, H), 0.0f);

                            if (NdotL > 0.0f) {
                                float G = GeometrySmith(N, V, L, roughness);
                                float G_Vis = (G * VdotH) / (NdotH * NdotV);
                                float Fc = std::pow(1.0f - VdotH, 5.0f);

                                A += (1.0f - Fc) * G_Vis;
                                B += Fc * G_Vis;
                            }
                        }

                        A /= static_cast<float>(SAMPLE_COUNT);
                        B /= static_cast<float>(SAMPLE_COUNT);

                        size_t idx = (static_cast<size_t>(y) * lutRes + x) * 4;
                        brdfHalfData[idx + 0] = FloatToHalf(A);
                        brdfHalfData[idx + 1] = FloatToHalf(B);
                        brdfHalfData[idx + 2] = FloatToHalf(0.0f);
                        brdfHalfData[idx + 3] = FloatToHalf(1.0f);
                    }
                }
            });
        }
        for (auto& w : workers) w.join();
    }

    RHI::TextureDesc lutDesc{};
    lutDesc.width = lutRes;
    lutDesc.height = lutRes;
    lutDesc.format = RHI::TextureFormat::RGBA16_FLOAT;
    lutDesc.type = RHI::TextureType::Texture2D;
    lutDesc.mipLevels = 1;
    lutDesc.debugName = "BRDF_LUT_2D";
    m_impl->brdfLutTexture = device.CreateTexture(lutDesc, brdfHalfData.data());
    std::cout << "[EnvironmentMap] 2D BRDF Integration LUT created: " << lutRes << "x" << lutRes << " (RGBA16F)." << std::endl;

    // -------------------------------------------------------------------------
    // 5. Create Hardware Samplers
    // -------------------------------------------------------------------------
    RHI::SamplerDesc cubeSamplerDesc{};
    cubeSamplerDesc.minFilter = RHI::FilterMode::Linear;
    cubeSamplerDesc.magFilter = RHI::FilterMode::Linear;
    cubeSamplerDesc.addressModeU = RHI::AddressMode::ClampToEdge;
    cubeSamplerDesc.addressModeV = RHI::AddressMode::ClampToEdge;
    cubeSamplerDesc.addressModeW = RHI::AddressMode::ClampToEdge;
    cubeSamplerDesc.minLod = 0.0f;
    cubeSamplerDesc.maxLod = 16.0f; // Enable sampling across all prefilter mip levels!
    m_impl->cubemapSampler = device.CreateSampler(cubeSamplerDesc);

    RHI::SamplerDesc lutSamplerDesc{};
    lutSamplerDesc.minFilter = RHI::FilterMode::Linear;
    lutSamplerDesc.magFilter = RHI::FilterMode::Linear;
    lutSamplerDesc.addressModeU = RHI::AddressMode::ClampToEdge;
    lutSamplerDesc.addressModeV = RHI::AddressMode::ClampToEdge;
    lutSamplerDesc.addressModeW = RHI::AddressMode::ClampToEdge;
    lutSamplerDesc.minLod = 0.0f;
    lutSamplerDesc.maxLod = 0.0f;
    m_impl->lutSampler = device.CreateSampler(lutSamplerDesc);

    std::cout << "[EnvironmentMap] IBL Setup successfully initialized and uploaded to GPU!\n" << std::endl;
}

EnvironmentMap::EnvironmentMap(Device& device, const EnvironmentMapDesc& desc)
    : EnvironmentMap(device, "", desc) 
{}

EnvironmentMap::~EnvironmentMap() = default;
EnvironmentMap::EnvironmentMap(EnvironmentMap&&) noexcept = default;
EnvironmentMap& EnvironmentMap::operator=(EnvironmentMap&&) noexcept = default;

std::shared_ptr<EnvironmentMap> EnvironmentMap::LoadHDR(Device& device, const std::string& hdrFilePath, const EnvironmentMapDesc& desc) {
    return std::make_shared<EnvironmentMap>(device, hdrFilePath, desc);
}

std::shared_ptr<EnvironmentMap> EnvironmentMap::CreateProcedural(Device& device, const EnvironmentMapDesc& desc) {
    return std::make_shared<EnvironmentMap>(device, desc);
}

Texture EnvironmentMap::GetSkyboxTexture() const {
    return m_impl ? m_impl->skyboxTexture : Texture{};
}

Texture EnvironmentMap::GetIrradianceTexture() const {
    return m_impl ? m_impl->irradianceTexture : Texture{};
}

Texture EnvironmentMap::GetPrefilteredTexture() const {
    return m_impl ? m_impl->prefilteredTexture : Texture{};
}

Texture EnvironmentMap::GetBRDFLUTTexture() const {
    return m_impl ? m_impl->brdfLutTexture : Texture{};
}

Sampler EnvironmentMap::GetCubemapSampler() const {
    return m_impl ? m_impl->cubemapSampler : Sampler{};
}

Sampler EnvironmentMap::GetLUTSampler() const {
    return m_impl ? m_impl->lutSampler : Sampler{};
}

} // namespace Sunshine
