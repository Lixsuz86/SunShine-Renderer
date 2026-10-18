#pragma once

#include "transform.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Sunshine {

/**
 * @brief Hierarchical Scene Graph node with parent-child transform cascading.
 */
class SceneNode {
public:
    std::string name{"Node"};
    Transform transform;
    std::vector<uint32_t> submeshIndices; // Indices referencing Model submeshes

    SceneNode* parent{nullptr};
    std::vector<std::shared_ptr<SceneNode>> children;

    SceneNode() = default;
    explicit SceneNode(std::string nodeName) : name(std::move(nodeName)) {}

    void AddChild(std::shared_ptr<SceneNode> child) {
        if (child) {
            child->parent = this;
            children.push_back(std::move(child));
        }
    }

    [[nodiscard]] glm::mat4 GetWorldMatrix() const noexcept {
        if (parent) {
            return parent->GetWorldMatrix() * transform.GetMatrix();
        }
        return transform.GetMatrix();
    }
};

} // namespace Sunshine
