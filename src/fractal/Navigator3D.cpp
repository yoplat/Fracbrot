#include "fractal/Navigator3D.h"

#include "fractal/DistanceEstimator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fb {

namespace {

// Fly speed along the view axis in clearances per second: slows down near detail and can't
// fly through it.
constexpr double kFlyClearancesPerSecond = 1.5;
// Sideways / vertical speed: a fraction of the focus distance per second (about 2D's 0.375
// view heights per second at the home view), capped in clearances so it can't cross surfaces.
constexpr double kStrafeFocusPerSecond = 0.35;
constexpr double kStrafeClearancesPerSecond = 3.0;
// After a move the focus is capped at this many clearances, so it can't blow up while it
// stands in for the depth of rays that miss. Zooming leaves it alone (see update). There is
// no lower bound: a background zoom-in must shrink it freely to stay reversible; speeds use
// max(focus, clearance) instead.
constexpr double kMaxFocusClearances = 100.0;
// How far the camera may get from the set, in bounding radii (the set is then about a pixel
// wide). Flying out at clearance-scaled speed accelerates without limit otherwise.
constexpr double kMaxBoundRadii = 1000.0;

bool finite(glm::dvec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

} // namespace

OrbitCamera marchHome(const MarchParams& m) {
    OrbitCamera cam;
    cam.target = m.boundCenter;
    cam.yaw = 0.45f;
    cam.pitch = 0.3f;
    cam.distance = 3.2f;
    if (m.formula == Formula3D::Hypercomplex || m.formula == Formula3D::QuatFlip) {
        // Side-on along the needle, like the video.
        cam.target = {-0.9f, 0.0f, 0.0f};
        cam.yaw = 0.15f;
        cam.pitch = 0.06f;
        cam.distance = 2.6f;
    } else if (m.formula == Formula3D::QuatMandelbrot) {
        // Side-on to the real axis, like Ice Fractal's default view.
        cam.target = {-0.7f, 0.0f, 0.0f};
        cam.yaw = 0.12f;
        cam.pitch = 0.08f;
        cam.distance = 3.6f;
    }
    return cam;
}

void Navigator3D::update(OrbitCamera& cam, const MarchParams& m, const NavInput& in) {
    const OrbitCamera start = cam;
    const double startDe = distanceEstimate(m, start.positionD());
    // Inside the set (after a parameter change) the clearance is 0; move at focus-scaled
    // speeds then, so the keys can still get the camera out.
    const bool startInside = !(startDe > 0.0);
    const double w = std::max(double(in.fbWidth), 1.0), h = std::max(double(in.fbHeight), 1.0);
    auto rayThrough = [&](glm::dvec2 px) {
        return cam.rayDirD({2.0 * px.x / w - 1.0, 1.0 - 2.0 * px.y / h}, w / h);
    };
    // The depth scale for rays that miss: the focus, but never below the clearance (a
    // background zoom can shrink the focus far below it).
    auto focusScale = [&] { return std::max(cam.distance, clearance(cam, m)); };
    bool scrolled = false; // zoomed (scroll or Z / X)
    bool moved = false;    // by anything but zooming

    // Scroll: a fast wheel flick is spread over a few frames instead of jumping (or being
    // dropped), so every unit still lands.
    m_scrollCarry += in.scroll;
    const double units = std::clamp(m_scrollCarry, -kMaxScrollPerFrame, kMaxScrollPerFrame);
    m_scrollCarry -= units;
    if (units != 0.0) {
        zoomAlongRay(cam, m, rayThrough(in.cursor), in.pixelAngle, std::pow(kZoomPerScrollUnit, units));
        scrolled = true;
    }

    // Drag: grab the surface point under the cursor (or the point at the focus distance).
    if (in.left && !m_dragging) {
        const glm::dvec3 eye = cam.positionD();
        const glm::dvec3 dir = rayThrough(in.cursor);
        m_grab = eye + dir * raycast(m, eye, dir, in.pixelAngle).value_or(focusScale());
        m_pan = in.shift;
    } else if (in.left) {
        const double dx = in.cursor.x - in.prevCursor.x, dy = in.cursor.y - in.prevCursor.y;
        if (dx != 0.0 || dy != 0.0) {
            if (m_pan) {
                // Move in the view plane so the grabbed point follows the cursor exactly. Its
                // depth is measured now, so zooming mid-drag keeps the pan 1:1.
                const double depth = glm::dot(m_grab - cam.positionD(), glm::dvec3(cam.forward()));
                const double k = 2.0 * std::tan(0.5 * double(cam.fovY)) * depth / h;
                cam.translate(-(glm::dvec3(cam.right()) * dx - glm::dvec3(cam.up()) * dy) * k);
            } else {
                // Half a turn per full-height drag, whatever the display density.
                cam.orbitAround(m_grab, float(-std::numbers::pi * dx / h), float(std::numbers::pi * dy / h));
            }
            moved = true;
        }
    }
    m_dragging = in.left;

    // Keys.
    const double dt = in.dt;
    const double fly = double(in.forward) - double(in.back);
    glm::dvec2 strafe{double(in.rightKey) - double(in.leftKey), double(in.up) - double(in.down)};
    if (fly != 0.0 || strafe != glm::dvec2(0.0)) {
        const double clr = startInside ? cam.distance : clearance(cam, m);
        glm::dvec3 delta = glm::dvec3(cam.forward()) * (fly * kFlyClearancesPerSecond * clr * dt);
        if (strafe != glm::dvec2(0.0)) {
            const double speed =
                std::min(kStrafeFocusPerSecond * std::max(cam.distance, clr), kStrafeClearancesPerSecond * clr);
            strafe = glm::normalize(strafe);
            delta += (glm::dvec3(cam.right()) * strafe.x + glm::dvec3(cam.up()) * strafe.y) * (speed * dt);
        }
        cam.translate(delta);
        moved = true;
    }
    if (in.zoomIn != in.zoomOut) {
        const double factor = std::pow(in.zoomIn ? 1.0 / kKeyZoomPerSecond : kKeyZoomPerSecond, dt);
        // Counts as a scroll: zoomAlongRay already sets the focus, and the focus cap would
        // shrink each back-off with the clearance and walk a zoom-out onto a surface behind.
        zoomAlongRay(cam, m, glm::normalize(glm::dvec3(cam.forward())), in.pixelAngle, factor);
        scrolled = true;
    }

    if (!scrolled && !moved) return;
    // Focus on what the screen center shows, or at least the clearance when it shows
    // background. A pure zoom keeps the focus zoomAlongRay carried instead, so zooming over
    // background stays geometric and reversible.
    if (moved) {
        const glm::dvec3 center = glm::normalize(glm::dvec3(cam.forward()));
        if (const auto hit = raycast(m, cam.positionD(), center, in.pixelAngle); hit && *hit > 0.0)
            cam.refocus(*hit);
        else if (!startInside)
            cam.refocus(focusScale());
    }
    // Only after a move: a pure zoom already changes the focus by a bounded factor, and
    // capping it there shrinks it with the clearance whenever a zoom passes close to a surface,
    // which breaks reversibility and can walk a zoom-out onto that surface.
    if (moved && !startInside) {
        const double cap = kMaxFocusClearances * clearance(cam, m);
        if (cam.distance > cap) cam.refocus(cap);
    }

    // Never end a frame inside the set (a pan or orbit can swing the camera into it), lost in
    // non-finite numbers, or flying further out beyond kMaxBoundRadii. A frame that starts
    // inside (after a parameter change) may move anywhere, so the keys and zoom can get out.
    const glm::dvec3 pos = cam.positionD();
    const glm::dvec3 center = glm::dvec3(m.boundCenter);
    const double far = kMaxBoundRadii * double(m.boundRadius);
    const double dist = glm::length(pos - center);
    const bool lost = !finite(pos) || !std::isfinite(cam.distance) || !(cam.distance > 0.0);
    const bool tooFar = dist > far && dist > glm::length(start.positionD() - center);
    const bool inside = !startInside && !(distanceEstimate(m, pos) > 0.0);
    if (lost || tooFar || inside) cam = start;
}

uint32_t guardIterations(const OrbitCamera& cam, const MarchParams& m, uint32_t next) {
    if (next >= m.iterations) return next;
    MarchParams lower = m;
    lower.iterations = next;
    return distanceEstimate(lower, cam.positionD()) > 0.0 ? next : m.iterations;
}

} // namespace fb
