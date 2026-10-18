#pragma once

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Sunshine {

/**
 * @brief 3D Perspective Camera configured specifically for Vulkan clip-space coordinates.
 * 
 * Automatically handles Vulkan-specific requirements:
 * - Depth range [0, 1] via GLM_FORCE_DEPTH_ZERO_TO_ONE
 * - Inverted Y axis in clip space (proj[1][1] *= -1.0f)
 */
class Camera {
public:
    /**
     * @brief Creates a camera with standard perspective parameters.
     * @param fovDegrees Field of view in vertical degrees.
     * @param aspectRatio Width / Height viewport ratio.
     * @param nearPlane Distance to near clipping plane.
     * @param farPlane Distance to far clipping plane.
     */
    Camera(float fovDegrees = 45.0f, float aspectRatio = 16.0f / 9.0f, float nearPlane = 0.1f, float farPlane = 100.0f)
        : m_fov(glm::radians(fovDegrees)),
          m_aspectRatio(aspectRatio),
          m_nearPlane(nearPlane),
          m_farPlane(farPlane),
          m_position(0.0f, 0.0f, 3.0f),
          m_target(0.0f, 0.0f, 0.0f),
          m_up(0.0f, 1.0f, 0.0f) {}

    /**
     * @brief Updates the camera's perspective projection parameters.
     */
    void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
        m_fov = glm::radians(fovDegrees);
        m_aspectRatio = aspectRatio;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
    }

    /**
     * @brief Updates the aspect ratio (called on window resize).
     */
    void SetAspectRatio(float aspectRatio) {
        m_aspectRatio = aspectRatio;
    }

    /**
     * @brief Positions the camera looking towards a target point.
     */
    void LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) {
        m_position = position;
        m_target = target;
        m_up = up;
    }

    void SetPosition(const glm::vec3& position) { m_position = position; }
    const glm::vec3& GetPosition() const noexcept { return m_position; }

    void SetTarget(const glm::vec3& target) { m_target = target; }
    const glm::vec3& GetTarget() const noexcept { return m_target; }

    /**
     * @brief Calculates and returns the view transformation matrix.
     */
    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(m_position, m_target, m_up);
    }

    /**
     * @brief Calculates and returns the Vulkan-compatible projection matrix.
     */
    glm::mat4 GetProjectionMatrix() const {
        glm::mat4 proj = glm::perspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
        // Invert Y axis for Vulkan clip coordinates
        proj[1][1] *= -1.0f;
        return proj;
    }

    /**
     * @brief Computes combined Projection * View matrix.
     */
    glm::mat4 GetViewProjectionMatrix() const {
        return GetProjectionMatrix() * GetViewMatrix();
    }

private:
    float m_fov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
};

} // namespace Sunshine
