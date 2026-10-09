#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace fb {

// Orbits `target` at `distance`; yaw around +y, pitch above the xz plane (radians).
// Position is kept in double precision so deep 3D zooms can move in steps far below
// float resolution; directions and matrices stay in float.
struct OrbitCamera {
    glm::dvec3 target{0.0, 0.2, 0.0};
    float yaw = 0.6f;
    float pitch = 0.65f;
    double distance = 2.6;
    float fovY = glm::radians(50.0f);

    void reset() { *this = {}; }

    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -1.5f, 1.5f);
    }

    void dolly(float factor) { distance = std::clamp(distance * double(factor), 1e-5, 50.0); }

    // View basis: right, up and back (the camera looks along -back()).
    glm::vec3 back() const {
        const float cp = std::cos(pitch);
        return glm::vec3(cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw));
    }
    glm::vec3 right() const { return glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)); }
    glm::vec3 up() const { return glm::cross(back(), right()); }
    glm::vec3 forward() const { return -back(); }

    glm::dvec3 positionD() const { return target + distance * glm::dvec3(back()); }
    glm::vec3 position() const { return glm::vec3(positionD()); }

    // Moves the camera (and its target) without turning it.
    void translate(glm::dvec3 delta) { target += delta; }

    // Direction of the ray through normalized device coordinates (x right, y up, in [-1, 1]).
    glm::vec3 rayDir(glm::vec2 ndc, float aspect) const {
        const float t = std::tan(0.5f * fovY);
        return glm::normalize(forward() + right() * (ndc.x * t * aspect) + up() * (ndc.y * t));
    }

    // Swings the camera around `pivot` (yaw about world up, pitch about the camera's right
    // axis) and turns it by the same amount, so `pivot` keeps its place on screen.
    void orbitAround(glm::dvec3 pivot, float dYaw, float dPitch) {
        const glm::dmat3 before{glm::dvec3(right()), glm::dvec3(up()), glm::dvec3(back())};
        const glm::dvec3 pos = positionD();
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -1.5f, 1.5f);
        const glm::dmat3 after{glm::dvec3(right()), glm::dvec3(up()), glm::dvec3(back())};
        const glm::dvec3 newPos = pivot + after * (glm::transpose(before) * (pos - pivot));
        target = newPos - glm::dvec3(back()) * distance;
    }

    // Horizontal basis for moving over the ground plane.
    glm::vec3 groundForward() const { return -glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw)); }
    glm::vec3 groundRight() const { return glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)); }

    glm::mat4 view() const { return glm::lookAt(position(), glm::vec3(target), glm::vec3(0.0f, 1.0f, 0.0f)); }

    // View matrix with `origin` moved to (0, 0, 0): keeps precision when the detail on
    // screen is far smaller than the camera's distance from the world origin.
    glm::mat4 viewFrom(glm::dvec3 origin) const {
        const glm::vec3 eye(positionD() - origin);
        return glm::lookAt(eye, eye + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 projection(float aspect) const {
        const float d = float(distance);
        return glm::perspective(fovY, aspect, 0.01f * d, 100.0f + d);
    }
};

} // namespace fb
