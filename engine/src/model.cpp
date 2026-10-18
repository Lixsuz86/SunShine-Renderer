#include "sunshine/model.hpp"
#include "sunshine/error.hpp"

#include <iostream>
#include <cmath>
#include <cstring>

// tinygltf implementation
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#include <stb_image.h>
#include <tiny_gltf.h>

namespace Sunshine {

static std::filesystem::path ResolveModelPath(const std::filesystem::path& p) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec)) return p;
    if (std::filesystem::exists("../" / p, ec)) return "../" / p;
    if (std::filesystem::exists("../../" / p, ec)) return "../../" / p;
    return p;
}

// Compute tangents if the glTF file doesn't provide them
static void ComputeTangents(std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices) {
    std::vector<glm::vec3> tan1(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> tan2(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i1 = indices[i];
        uint32_t i2 = indices[i + 1];
        uint32_t i3 = indices[i + 2];

        const auto& v1 = vertices[i1];
        const auto& v2 = vertices[i2];
        const auto& v3 = vertices[i3];

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;

        glm::vec2 deltaUV1 = v2.texCoord - v1.texCoord;
        glm::vec2 deltaUV2 = v3.texCoord - v1.texCoord;

        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(f) > 1e-6f) {
            float r = 1.0f / f;
            glm::vec3 sdir = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * r;
            glm::vec3 tdir = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * r;

            tan1[i1] += sdir;
            tan1[i2] += sdir;
            tan1[i3] += sdir;

            tan2[i1] += tdir;
            tan2[i2] += tdir;
            tan2[i3] += tdir;
        }
    }

    for (size_t a = 0; a < vertices.size(); ++a) {
        const glm::vec3& n = vertices[a].normal;
        const glm::vec3& t = tan1[a];

        if (glm::length(t) > 1e-6f) {
            // Gram-Schmidt orthogonalize
            glm::vec3 tangent = glm::normalize(t - n * glm::dot(n, t));
            float w = (glm::dot(glm::cross(n, t), tan2[a]) < 0.0f) ? -1.0f : 1.0f;
            vertices[a].tangent = glm::vec4(tangent, w);
        } else {
            // Fallback default tangent
            glm::vec3 c1 = glm::cross(n, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 c2 = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 tangent = (glm::length(c1) > glm::length(c2)) ? glm::normalize(c1) : glm::normalize(c2);
            vertices[a].tangent = glm::vec4(tangent, 1.0f);
        }
    }
}

std::shared_ptr<Model> Model::LoadGLTF(Device& device, const std::filesystem::path& filePath) {
    auto resolvedPath = ResolveModelPath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        throw GraphicsException(
            "Could not locate 3D model file: " + filePath.string(),
            "Check that the model file exists and is located in the assets or examples directory."
        );
    }

    auto model = std::make_shared<Model>();

    // 1. Create Default 1x1 Fallback Textures
    uint32_t whitePixel = 0xFFFFFFFF; // 1x1 White (Albedo)
    model->defaultWhiteTexture = std::make_shared<Texture>(device.CreateTexture2D(1, 1, &whitePixel));

    uint32_t normalPixel = 0xFFFF8080; // 1x1 Neutral Tangent Normal (0.5, 0.5, 1.0) in RGBA8
    model->defaultNormalTexture = std::make_shared<Texture>(device.CreateTexture2D(1, 1, &normalPixel));

    uint32_t metallicRoughnessPixel = 0xFF80FF00; // 1x1 (R=0, G=1.0 Roughness, B=1.0 Metallic)
    model->defaultMetallicRoughnessTexture = std::make_shared<Texture>(device.CreateTexture2D(1, 1, &metallicRoughnessPixel));

    RHI::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = RHI::FilterMode::Linear;
    samplerDesc.magFilter = RHI::FilterMode::Linear;
    samplerDesc.addressModeU = RHI::AddressMode::Repeat;
    samplerDesc.addressModeV = RHI::AddressMode::Repeat;
    model->defaultSampler = std::make_shared<Sampler>(device.CreateSampler(samplerDesc));

    model->m_defaultMaterial = std::make_unique<Material>();
    model->m_defaultMaterial->baseColorTexture = model->defaultWhiteTexture;
    model->m_defaultMaterial->normalTexture = model->defaultNormalTexture;
    model->m_defaultMaterial->metallicRoughnessTexture = model->defaultMetallicRoughnessTexture;
    model->m_defaultMaterial->sampler = model->defaultSampler;

    // 2. Load glTF / GLB via tinygltf
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool success = false;
    std::string ext = resolvedPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".glb") {
        success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, resolvedPath.string());
    } else {
        success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, resolvedPath.string());
    }

    if (!warn.empty()) {
        std::cout << "[Sunshine Model Warning] " << warn << std::endl;
    }

    if (!success || !err.empty()) {
        throw GraphicsException(
            "Failed to load glTF model: " + err,
            "Ensure the glTF file is valid and follows glTF 2.0 specifications."
        );
    }

    // 3. Load Textures & Images
    std::vector<std::shared_ptr<Texture>> loadedTextures;
    loadedTextures.reserve(gltfModel.textures.size());

    for (const auto& gltfTex : gltfModel.textures) {
        if (gltfTex.source >= 0 && gltfTex.source < static_cast<int>(gltfModel.images.size())) {
            const auto& img = gltfModel.images[gltfTex.source];
            if (img.width > 0 && img.height > 0) {
                if (img.component == 4) {
                    loadedTextures.push_back(std::make_shared<Texture>(
                        device.CreateTexture2D(img.width, img.height, img.image.data())
                    ));
                } else if (img.component == 3) {
                    // Convert RGB to RGBA
                    std::vector<uint8_t> rgba(img.width * img.height * 4);
                    for (size_t p = 0; p < static_cast<size_t>(img.width * img.height); ++p) {
                        rgba[p * 4 + 0] = img.image[p * 3 + 0];
                        rgba[p * 4 + 1] = img.image[p * 3 + 1];
                        rgba[p * 4 + 2] = img.image[p * 3 + 2];
                        rgba[p * 4 + 3] = 255;
                    }
                    loadedTextures.push_back(std::make_shared<Texture>(
                        device.CreateTexture2D(img.width, img.height, rgba.data())
                    ));
                } else {
                    loadedTextures.push_back(model->defaultWhiteTexture);
                }
            } else {
                loadedTextures.push_back(model->defaultWhiteTexture);
            }
        } else {
            loadedTextures.push_back(model->defaultWhiteTexture);
        }
    }

    // 4. Load Materials
    model->m_materials.reserve(gltfModel.materials.size());
    for (const auto& gltfMat : gltfModel.materials) {
        Material mat{};
        mat.name = gltfMat.name.empty() ? "Material_" + std::to_string(model->m_materials.size()) : gltfMat.name;
        mat.sampler = model->defaultSampler;

        // Base Color
        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0 && 
            gltfMat.pbrMetallicRoughness.baseColorTexture.index < static_cast<int>(loadedTextures.size())) {
            mat.baseColorTexture = loadedTextures[gltfMat.pbrMetallicRoughness.baseColorTexture.index];
            mat.hasBaseColorTexture = true;
        } else {
            mat.baseColorTexture = model->defaultWhiteTexture;
        }

        const auto& bcf = gltfMat.pbrMetallicRoughness.baseColorFactor;
        if (bcf.size() == 4) {
            mat.baseColorFactor = glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3]);
        }

        // Metallic - Roughness
        if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0 &&
            gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index < static_cast<int>(loadedTextures.size())) {
            mat.metallicRoughnessTexture = loadedTextures[gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index];
            mat.hasMetallicRoughnessTexture = true;
        } else {
            mat.metallicRoughnessTexture = model->defaultMetallicRoughnessTexture;
        }

        mat.metallicFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
        mat.roughnessFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);

        // Normal Texture
        if (gltfMat.normalTexture.index >= 0 && 
            gltfMat.normalTexture.index < static_cast<int>(loadedTextures.size())) {
            mat.normalTexture = loadedTextures[gltfMat.normalTexture.index];
            mat.hasNormalTexture = true;
            mat.normalScale = static_cast<float>(gltfMat.normalTexture.scale);
        } else {
            mat.normalTexture = model->defaultNormalTexture;
        }

        // Occlusion Texture (AO)
        if (gltfMat.occlusionTexture.index >= 0 && 
            gltfMat.occlusionTexture.index < static_cast<int>(loadedTextures.size())) {
            mat.occlusionTexture = loadedTextures[gltfMat.occlusionTexture.index];
            mat.hasOcclusionTexture = true;
            mat.occlusionStrength = static_cast<float>(gltfMat.occlusionTexture.strength);
        } else {
            mat.occlusionTexture = model->defaultWhiteTexture;
        }

        // Emissive Texture
        if (gltfMat.emissiveTexture.index >= 0 &&
            gltfMat.emissiveTexture.index < static_cast<int>(loadedTextures.size())) {
            mat.emissiveTexture = loadedTextures[gltfMat.emissiveTexture.index];
            mat.hasEmissiveTexture = true;
        }
        if (gltfMat.emissiveFactor.size() == 3) {
            mat.emissiveFactor = glm::vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);
        }

        model->m_materials.push_back(std::move(mat));
    }

    if (model->m_materials.empty()) {
        model->m_materials.push_back(*model->m_defaultMaterial);
    }

    // 5. Load Meshes & Primitives
    // Map of (meshIndex, primitiveIndex) to submeshIndex
    std::vector<std::vector<uint32_t>> meshSubmeshIndices(gltfModel.meshes.size());

    for (size_t m = 0; m < gltfModel.meshes.size(); ++m) {
        const auto& gltfMesh = gltfModel.meshes[m];
        meshSubmeshIndices[m].reserve(gltfMesh.primitives.size());

        for (size_t p = 0; p < gltfMesh.primitives.size(); ++p) {
            const auto& primitive = gltfMesh.primitives[p];

            // Position accessor (required)
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) continue;

            const auto& posAccessor = gltfModel.accessors[posIt->second];
            const auto& posView = gltfModel.bufferViews[posAccessor.bufferView];
            const auto& posBuffer = gltfModel.buffers[posView.buffer];
            const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);
            size_t posStride = posView.byteStride ? (posView.byteStride / sizeof(float)) : 3;

            // Normal accessor
            const float* normData = nullptr;
            size_t normStride = 3;
            auto normIt = primitive.attributes.find("NORMAL");
            if (normIt != primitive.attributes.end()) {
                const auto& normAccessor = gltfModel.accessors[normIt->second];
                const auto& normView = gltfModel.bufferViews[normAccessor.bufferView];
                const auto& normBuffer = gltfModel.buffers[normView.buffer];
                normData = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);
                normStride = normView.byteStride ? (normView.byteStride / sizeof(float)) : 3;
            }

            // TexCoord accessor
            const float* uvData = nullptr;
            size_t uvStride = 2;
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end()) {
                const auto& uvAccessor = gltfModel.accessors[uvIt->second];
                const auto& uvView = gltfModel.bufferViews[uvAccessor.bufferView];
                const auto& uvBuffer = gltfModel.buffers[uvView.buffer];
                uvData = reinterpret_cast<const float*>(&uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset]);
                uvStride = uvView.byteStride ? (uvView.byteStride / sizeof(float)) : 2;
            }

            // Tangent accessor
            const float* tanData = nullptr;
            size_t tanStride = 4;
            auto tanIt = primitive.attributes.find("TANGENT");
            if (tanIt != primitive.attributes.end()) {
                const auto& tanAccessor = gltfModel.accessors[tanIt->second];
                const auto& tanView = gltfModel.bufferViews[tanAccessor.bufferView];
                const auto& tanBuffer = gltfModel.buffers[tanView.buffer];
                tanData = reinterpret_cast<const float*>(&tanBuffer.data[tanView.byteOffset + tanAccessor.byteOffset]);
                tanStride = tanView.byteStride ? (tanView.byteStride / sizeof(float)) : 4;
            }

            // Assemble vertices
            std::vector<ModelVertex> vertices(posAccessor.count);
            for (size_t v = 0; v < posAccessor.count; ++v) {
                vertices[v].position = glm::vec3(
                    posData[v * posStride + 0],
                    posData[v * posStride + 1],
                    posData[v * posStride + 2]
                );

                if (normData) {
                    vertices[v].normal = glm::vec3(
                        normData[v * normStride + 0],
                        normData[v * normStride + 1],
                        normData[v * normStride + 2]
                    );
                }

                if (uvData) {
                    vertices[v].texCoord = glm::vec2(
                        uvData[v * uvStride + 0],
                        uvData[v * uvStride + 1]
                    );
                }

                if (tanData) {
                    vertices[v].tangent = glm::vec4(
                        tanData[v * tanStride + 0],
                        tanData[v * tanStride + 1],
                        tanData[v * tanStride + 2],
                        tanData[v * tanStride + 3]
                    );
                }
            }

            // Assemble indices
            std::vector<uint32_t> indices;
            if (primitive.indices >= 0) {
                const auto& idxAccessor = gltfModel.accessors[primitive.indices];
                const auto& idxView = gltfModel.bufferViews[idxAccessor.bufferView];
                const auto& idxBuffer = gltfModel.buffers[idxView.buffer];
                const uint8_t* rawData = &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset];

                indices.resize(idxAccessor.count);
                if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* u16 = reinterpret_cast<const uint16_t*>(rawData);
                    for (size_t i = 0; i < idxAccessor.count; ++i) indices[i] = u16[i];
                } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* u32 = reinterpret_cast<const uint32_t*>(rawData);
                    for (size_t i = 0; i < idxAccessor.count; ++i) indices[i] = u32[i];
                } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    for (size_t i = 0; i < idxAccessor.count; ++i) indices[i] = rawData[i];
                }
            } else {
                indices.resize(vertices.size());
                for (size_t i = 0; i < vertices.size(); ++i) indices[i] = static_cast<uint32_t>(i);
            }

            // If tangents weren't provided in the glTF file, compute them automatically
            if (!tanData) {
                ComputeTangents(vertices, indices);
            }

            Submesh submesh{};
            submesh.name = gltfMesh.name + "_sub_" + std::to_string(p);
            submesh.indexCount = static_cast<uint32_t>(indices.size());
            submesh.vertexCount = static_cast<uint32_t>(vertices.size());
            submesh.materialIndex = (primitive.material >= 0) ? primitive.material : 0;
            submesh.vertexBuffer = device.CreateVertexBuffer(vertices);
            submesh.indexBuffer = device.CreateIndexBuffer(indices);

            uint32_t submeshIdx = static_cast<uint32_t>(model->m_submeshes.size());
            model->m_submeshes.push_back(std::move(submesh));
            meshSubmeshIndices[m].push_back(submeshIdx);
        }
    }

    // 6. Build Scene Graph
    auto parseNode = [&](auto& self, int nodeIdx, SceneNode* parent) -> std::shared_ptr<SceneNode> {
        if (nodeIdx < 0 || nodeIdx >= static_cast<int>(gltfModel.nodes.size())) return nullptr;
        const auto& gltfNode = gltfModel.nodes[nodeIdx];

        auto node = std::make_shared<SceneNode>(gltfNode.name.empty() ? "Node_" + std::to_string(nodeIdx) : gltfNode.name);
        node->parent = parent;

        // Parse transform
        if (gltfNode.matrix.size() == 16) {
            glm::mat4 m{};
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    m[col][row] = static_cast<float>(gltfNode.matrix[col * 4 + row]);
                }
            }
            node->transform.position = glm::vec3(m[3]);
            node->transform.rotation = glm::quat_cast(m);
            node->transform.scale = glm::vec3(glm::length(m[0]), glm::length(m[1]), glm::length(m[2]));
        } else {
            if (gltfNode.translation.size() == 3) {
                node->transform.position = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
            }
            if (gltfNode.rotation.size() == 4) {
                // glTF stores quaternion as (x, y, z, w); glm::quat constructor takes (w, x, y, z)
                node->transform.rotation = glm::quat(
                    static_cast<float>(gltfNode.rotation[3]),
                    static_cast<float>(gltfNode.rotation[0]),
                    static_cast<float>(gltfNode.rotation[1]),
                    static_cast<float>(gltfNode.rotation[2])
                );
            }
            if (gltfNode.scale.size() == 3) {
                node->transform.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
            }
        }

        // Attach submeshes
        if (gltfNode.mesh >= 0 && gltfNode.mesh < static_cast<int>(meshSubmeshIndices.size())) {
            node->submeshIndices = meshSubmeshIndices[gltfNode.mesh];
        }

        // Parse children
        for (int childIdx : gltfNode.children) {
            auto childNode = self(self, childIdx, node.get());
            if (childNode) {
                node->children.push_back(childNode);
            }
        }

        return node;
    };

    int activeScene = gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0;
    if (activeScene < static_cast<int>(gltfModel.scenes.size())) {
        const auto& scene = gltfModel.scenes[activeScene];
        for (int rootNodeIdx : scene.nodes) {
            auto rootNode = parseNode(parseNode, rootNodeIdx, nullptr);
            if (rootNode) {
                model->m_rootNodes.push_back(rootNode);
            }
        }
    }

    std::cout << "[Sunshine Model] Successfully loaded glTF: " << resolvedPath.filename().string()
              << " (" << model->m_submeshes.size() << " submeshes, "
              << model->m_materials.size() << " materials, "
              << model->m_rootNodes.size() << " root nodes)" << std::endl;

    return model;
}

} // namespace Sunshine
