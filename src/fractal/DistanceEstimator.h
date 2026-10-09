#pragma once

#include "camera/OrbitCamera.h"
#include "fractal/Raymarcher.h"

#include <glm/glm.hpp>

#include <optional>

namespace fb {

// CPU mirror of de() in shaders/raymarch.wgsl (keep the two in sync), in double precision. Used to find
// what the camera is looking at so navigation can scale with the distance to the surface.
double distanceEstimate(const MarchParams& m, glm::dvec3 pos);

// The world -> 4D slice mapping used by the quaternion-family formulas (see toSlice in the shader).
glm::dvec4 toSlice(const MarchParams& m, glm::dvec3 pos);

// Marches from `origin` along unit `dir` against the fractal. `pixelAngle` sets the hit
// threshold like the shader does. Returns the hit distance along the ray, if any.
std::optional<double> raycast(const MarchParams& m, glm::dvec3 origin, glm::dvec3 dir, double pixelAngle);

// Moves the camera along unit `dir` by (1 - factor) of the distance to the surface that
// direction hits (factor > 1 backs off), so the point under the cursor stays under it.
// Forward-only motion can't cross the surface ahead. Falls back to the camera's focus
// distance when the ray misses.
void zoomAlongRay(OrbitCamera& cam, const MarchParams& m, glm::dvec3 dir, double pixelAngle, double factor);

// Distance to the nearest surface from the camera, for scaling fly speed.
double clearance(const OrbitCamera& cam, const MarchParams& m);

} // namespace fb
