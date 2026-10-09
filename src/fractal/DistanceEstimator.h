#pragma once

#include "camera/OrbitCamera.h"
#include "fractal/Raymarcher.h"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace fb {

// CPU mirror of de() in shaders/raymarch.wgsl (keep the two in sync), in double precision. Used to find
// what the camera is looking at so navigation can scale with the distance to the surface.
double distanceEstimate(const MarchParams& m, glm::dvec3 pos);

// The world -> 4D slice mapping used by the quaternion-family formulas (see toSlice in the shader).
glm::dvec4 toSlice(const MarchParams& m, glm::dvec3 pos);

// Reference orbit Z_0, Z_1, ... at world point `origin` for the perturbation path of the
// quaternion family / hypercomplex formulas, computed in double precision. Stops after the
// orbit escapes far outside the set or after `maxLen` entries; always at least 2 entries.
std::vector<glm::vec4> referenceOrbit(const MarchParams& m, glm::dvec3 origin, uint32_t maxLen);

// Marches from `origin` along unit `dir` against the fractal. `pixelAngle` (that of the
// rendered image) sets the hit threshold and m.maxSteps the step budget, like the shader.
// Returns the hit distance along the ray, if any.
std::optional<double> raycast(const MarchParams& m, glm::dvec3 origin, glm::dvec3 dir, double pixelAngle);

// Moves the camera along unit `dir` by (1 - factor) of the distance to the surface that
// direction hits (factor > 1 backs off), so the point under the cursor stays under it.
// When the ray misses it uses the camera's focus distance (cam.distance) instead. Either way
// the focus becomes the remaining distance (ahead * factor), so background zooms converge
// geometrically and reverse exactly. It never moves onto a surface: forward motion stops
// short of the hit, and backing off that is blocked (a surface behind closer than twice the
// move, a destination inside the set, or, deep in, one that gives up most of the
// clearance) turns into backing away from the nearest surface. A move that would still end
// inside the set is shortened (to 0.9 x the clearance, halved further where the estimate
// overshoots).
void zoomAlongRay(OrbitCamera& cam, const MarchParams& m, glm::dvec3 dir, double pixelAngle, double factor);

// Distance to the nearest surface from the camera, for scaling fly speed.
double clearance(const OrbitCamera& cam, const MarchParams& m);

} // namespace fb
