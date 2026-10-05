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

// Moves the orbit pivot onto the surface point at the screen center (or closer, if other
// geometry is nearer to the camera) without moving the camera, so rotation, zoom and
// movement scale with the distance to what's around you. Returns false if unchanged.
bool pivotOnSurface(OrbitCamera& cam, const MarchParams& m, float pixelAngle);

// Moves the camera straight ahead by (1 - factor) of the distance to the surface in view
// (factor > 1 backs off). Forward-only motion can't cross what's ahead, so this uses the
// surface distance rather than the nearest geometry and stays quick in narrow gaps.
void zoomTowardSurface(OrbitCamera& cam, const MarchParams& m, float pixelAngle, float factor);

} // namespace fb
