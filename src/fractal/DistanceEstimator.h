#pragma once

#include "camera/OrbitCamera.h"
#include "fractal/Raymarcher.h"

#include <glm/glm.hpp>

#include <optional>

namespace fb {

// CPU mirror of de() in shaders/raymarch.wgsl (keep the two in sync). Used to find
// what the camera is looking at so navigation can scale with the distance to the surface.
float distanceEstimate(const MarchParams& m, glm::vec3 pos);

// Marches from `origin` along unit `dir` against the fractal. `pixelAngle` sets the hit
// threshold like the shader does. Returns the hit distance along the ray, if any.
std::optional<float> raycast(const MarchParams& m, glm::vec3 origin, glm::vec3 dir, float pixelAngle);

// Moves the camera along unit `dir` by (1 - factor) of the distance to the surface that
// direction hits (factor > 1 backs off), so the point under the cursor stays under it.
// Forward-only motion can't cross the surface ahead. Falls back to the camera's focus
// distance when the ray misses.
void zoomAlongRay(OrbitCamera& cam, const MarchParams& m, glm::vec3 dir, float pixelAngle, float factor);

// Distance to the nearest surface from the camera, for scaling fly speed.
float clearance(const OrbitCamera& cam, const MarchParams& m);

} // namespace fb
