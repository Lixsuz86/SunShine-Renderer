#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "scene_node.hpp"
#include <filesystem>
#include <vector>
#include <memory>
#include <string>

namespace Sunshine {

/**
 * @brief Tangent-space vertex format for glTF 2.0 PBR rendering.
 */
struct ModelVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f}; // xyz = tangent, w = bitangent sign
};

/**
 * @brief A single drawable part of a 3D model sharing a common material.
 */
struct Submesh {
    std::string name;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    uint32_t indexCount{0};
    uint32_t vertexCount{0};
    int32_t materialIndex{-1};
};

/**
 * @brief Represents a fully loaded 3D glTF model with meshes, materials, and scene graph.
 */
class Model {
public:
    Model() = default;

    /**
     * @brief Loads a glTF 2.0 (.gltf or .glb) model from disk.
     * @param device The Sunshine Device to allocate GPU buffers and textures on.
     * @param filePath Path to the glTF / GLB file.
     * @return Shared pointer to the loaded Model.
     */
    static std::shared_ptr<Model> LoadGLTF(Device& device, const std::filesystem::path& filePath);

    [[nodiscard]] const std::vector<Submesh>& GetSubmeshes() const noexcept { return m_submeshes; }
    [[nodiscard]] const std::vector<Material>& GetMaterials() const noexcept { return m_materials; }
    [[nodiscard]] const std::vector<std::shared_ptr<SceneNode>>& GetRootNodes() const noexcept { return m_rootNodes; }

    [[nodiscard]] const Material* GetMaterial(int32_t index) const noexcept {
        if (index >= 0 && index < static_cast<int32_t>(m_materials.size())) {
            return &m_materials[index];
        }
        return m_defaultMaterial.get();
    }

    std::shared_ptr<Texture> defaultWhiteTexture;
    std::shared_ptr<Texture> defaultNormalTexture;
    std::shared_ptr<Texture> defaultMetallicRoughnessTexture;
    std::shared_ptr<Sampler> defaultSampler;

private:
    friend class ModelLoader;

    std::vector<Submesh> m_submeshes;
    std::vector<Material> m_materials;
    std::vector<std::shared_ptr<SceneNode>> m_rootNodes;
    std::unique_ptr<Material> m_defaultMaterial;
};

} // namespace Sunshine
