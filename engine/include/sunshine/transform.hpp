#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Sunshine {

/**
 * @brief Represents 3D position, orientation, and scale of an entity in space.
 */
struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion (w, x, y, z)
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    Transform() = default;
    Transform(const glm::vec3& pos) : position(pos) {}
    Transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl = glm::vec3(1.0f))
        : position(pos), rotation(rot), scale(scl) {}

    [[nodiscard]] glm::mat4 GetMatrix() const noexcept {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }

    void SetEulerAngles(const glm::vec3& degrees) {
        rotation = glm::quat(glm::radians(degrees));
    }
};

} // namespace Sunshine
