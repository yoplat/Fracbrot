#pragma once

#include "camera/OrbitCamera.h"
#include "fractal/Raymarcher.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace fb {

// Zoom rates shared by every mode's navigation (the 2D and logistic handlers in App use them
// too), so scroll and Z/X feel the same everywhere.
inline constexpr double kZoomPerScrollUnit = 0.9; // view scale per scroll unit
inline constexpr double kKeyZoomPerSecond = 4.0;  // Z / X zoom rate

// One frame of input for the 3D view. Positions are framebuffer pixels, origin top-left.
struct NavInput {
    glm::dvec2 cursor{0.0};
    glm::dvec2 prevCursor{0.0};
    double scroll = 0.0; // scroll units this frame, > 0 zooms in
    bool left = false;   // left mouse button
    bool shift = false;  // at the press: the drag pans instead of orbiting
    bool forward = false, back = false, leftKey = false, rightKey = false, up = false, down = false;
    bool zoomIn = false, zoomOut = false;
    float dt = 0.0f;
    float fbWidth = 1.0f, fbHeight = 1.0f;
    double pixelAngle = 1e-3; // angular size of a rendered pixel, for CPU raycasts
};

// 3D fractal navigation, anchored to the surface under the cursor: scroll / Z X move toward
// what the ray hits, drag orbits the grabbed point, shift-drag pans so it follows the cursor,
// and keys fly at speeds scaled to the clearance. cam.distance is the focus distance, which
// stands in for the depth wherever a ray misses. The camera never ends a frame inside the set.
class Navigator3D {
public:
    // At most this many scroll units are applied per frame; the rest carries over.
    static constexpr double kMaxScrollPerFrame = 4.0;

    void update(OrbitCamera& cam, const MarchParams& m, const NavInput& in);
    // Forgets the drag in progress and any scroll still to apply, e.g. after a view reset or
    // a formula change (so leftover scroll doesn't zoom the new view) or leaving the 3D mode.
    void reset() { *this = {}; }

private:
    bool m_dragging = false;
    bool m_pan = false;         // this drag pans instead of orbiting
    glm::dvec3 m_grab{0.0};     // world point grabbed at the press
    double m_scrollCarry = 0.0; // scroll not applied yet
};

// The 3D view's home camera for the formula in `m`.
OrbitCamera marchHome(const MarchParams& m);

// `next` as the 3D iteration count, or the current count when lowering it to `next` would
// put the camera inside the (fatter, less iterated) set.
uint32_t guardIterations(const OrbitCamera& cam, const MarchParams& m, uint32_t next);

} // namespace fb
