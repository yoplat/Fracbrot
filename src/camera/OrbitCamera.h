#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace fb {

// Orbits `target` at `distance`; yaw around +y, pitch above the xz plane (radians).
struct OrbitCamera {
    glm::vec3 target{0.0f, 0.2f, 0.0f};
    float yaw = 0.6f;
    float pitch = 0.65f;
    float distance = 2.6f;
    float fovY = glm::radians(50.0f);

    void reset() { *this = {}; }

    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -1.5f, 1.5f);
    }

    void dolly(float factor) { distance = std::clamp(distance * factor, 1e-5f, 50.0f); }

    glm::vec3 position() const {
        const float cp = std::cos(pitch);
        return target + distance * glm::vec3(cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw));
    }

    // View basis: right, up and back (the camera looks along -back()).
    glm::vec3 back() const {
        const float cp = std::cos(pitch);
        return glm::vec3(cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw));
    }
    glm::vec3 right() const { return glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)); }
    glm::vec3 up() const { return glm::cross(back(), right()); }
    glm::vec3 forward() const { return -back(); }

    // Moves the camera (and its target) without turning it.
    void translate(glm::vec3 delta) { target += delta; }

    // Direction of the ray through normalized device coordinates (x right, y up, in [-1, 1]).
    glm::vec3 rayDir(glm::vec2 ndc, float aspect) const {
        const float t = std::tan(0.5f * fovY);
        return glm::normalize(forward() + right() * (ndc.x * t * aspect) + up() * (ndc.y * t));
    }

    // Swings the camera around `pivot` (yaw about world up, pitch about the camera's right
    // axis) and turns it by the same amount, so `pivot` keeps its place on screen.
    void orbitAround(glm::vec3 pivot, float dYaw, float dPitch) {
        const glm::mat3 before(right(), up(), back());
        const glm::vec3 pos = position();
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -1.5f, 1.5f);
        const glm::mat3 after(right(), up(), back());
        const glm::vec3 newPos = pivot + after * (glm::transpose(before) * (pos - pivot));
        target = newPos - back() * distance;
    }

    // Horizontal basis for moving over the ground plane.
    glm::vec3 groundForward() const { return -glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw)); }
    glm::vec3 groundRight() const { return glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)); }

    glm::mat4 view() const { return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f)); }

    glm::mat4 projection(float aspect) const {
        return glm::perspective(fovY, aspect, 0.01f * distance, 100.0f + distance);
    }
};

} // namespace fb
