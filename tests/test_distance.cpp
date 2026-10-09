#include "fractal/DistanceEstimator.h"
#include "fractal/Navigator3D.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

} // namespace

int main() {
    using namespace fb;
    const float pixelAngle = 1e-4f;

    for (float yz : {0.0f, 27.0f}) {
        MarchParams m;
        applyFormulaDefaults(m, Formula3D::QuatMandelbrot);
        m.rotation.y = glm::radians(yz); // YZ rotation leaves the real axis untouched
        m.detail = 1.0f;

        // The set ends at c = 0.25 on the real axis, so a ray along -x from x = 2 hits at 1.75.
        auto hit = raycast(m, {2.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, pixelAngle);
        check(hit && std::fabs(*hit - 1.75f) < 0.01f, "real-axis hit at c = 0.25");

        check(distanceEstimate(m, {-0.5f, 0.0f, 0.0f}) <= 1e-4f, "cardioid interior has ~zero distance");
        const float far = distanceEstimate(m, {3.0f, 2.0f, 1.0f});
        check(far > 0.5f && far < 4.0f, "far point has a sane positive distance");

        // Distance estimates must not overshoot: stepping by d never crosses into the set.
        const glm::vec3 p{1.0f, 0.5f, 0.0f};
        const float d = distanceEstimate(m, p);
        check(distanceEstimate(m, p + glm::vec3(-d, 0.0f, 0.0f)) > 0.0f, "no overshoot");
    }

    MarchParams bulb;
    applyFormulaDefaults(bulb, Formula3D::Mandelbulb);
    check(raycast(bulb, {0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}, pixelAngle).has_value(), "mandelbulb hit");
    check(!raycast(bulb, {0.0f, 3.0f, 3.0f}, {0.0f, 0.0f, -1.0f}, pixelAngle).has_value(), "miss above bulb");

    // Navigation: each operation keeps the grabbed / cursor point fixed on screen.
    {
        MarchParams m;
        applyFormulaDefaults(m, Formula3D::Hypercomplex);
        OrbitCamera cam;
        cam.target = {-0.9f, 0.0f, 0.0f};
        cam.yaw = 0.15f;
        cam.pitch = 0.06f;
        cam.distance = 2.6f;
        const float aspect = 1.6f, H = 1000.0f;
        const float pa = 2.0f * std::tan(0.5f * cam.fovY) / H;
        auto project = [&](glm::vec3 p) {
            const glm::vec4 clip = cam.projection(aspect) * cam.view() * glm::vec4(p, 1.0f);
            return glm::vec2(clip) / clip.w;
        };
        auto near2 = [](glm::vec2 a, glm::vec2 b, float tol) { return glm::length(a - b) < tol; };

        // Zoom toward an off-center cursor: the surface point under it stays put.
        const glm::vec2 cursor{0.3f, -0.2f};
        for (int i = 0; i < 20; ++i) {
            const glm::dvec3 dir(cam.rayDir(cursor, aspect));
            const auto hit = raycast(m, cam.positionD(), dir, pa);
            if (!hit) break;
            const glm::vec3 p(cam.positionD() + dir * *hit);
            zoomAlongRay(cam, m, dir, pa, kZoomPerScrollUnit);
            if (!near2(project(p), cursor, 1e-3f)) {
                check(false, "zoom keeps the point under the cursor");
                break;
            }
        }

        // Orbit around a grabbed point: it keeps its screen position.
        const glm::dvec3 dir(cam.rayDir(cursor, aspect));
        const glm::dvec3 grabD = cam.positionD() + dir * raycast(m, cam.positionD(), dir, pa).value_or(1.0);
        const glm::vec3 grab(grabD);
        const glm::vec2 before = project(grab);
        cam.orbitAround(grabD, 0.4f, -0.25f);
        check(near2(project(grab), before, 1e-3f), "orbit keeps the grabbed point on screen");

        // Pan by (dx, dy) pixels: the grabbed point follows the cursor exactly.
        const float depth = glm::dot(grab - cam.position(), cam.forward());
        const float dx = 40.0f, dy = -25.0f;
        const float k = 2.0f * std::tan(0.5f * cam.fovY) * depth / H;
        const glm::vec2 start = project(grab);
        cam.translate(glm::dvec3(-(cam.right() * dx - cam.up() * dy) * k));
        const glm::vec2 expected = start + glm::vec2(2.0f * dx / (H * aspect), -2.0f * dy / H);
        check(near2(project(grab), expected, 1e-3f), "pan moves the grabbed point with the cursor");
    }

    // Scroll-zooming at the screen center: 120 notches approach the surface without ever
    // entering the set. The center ray soon grazes the needle without hitting it (within the
    // shader's step budget), so this also covers zooming on the carried focus distance.
    {
        MarchParams m;
        applyFormulaDefaults(m, Formula3D::QuatMandelbrot);
        OrbitCamera cam;
        cam.target = {-0.7f, 0.0f, 0.0f};
        cam.yaw = 0.12f;
        cam.pitch = 0.08f;
        cam.distance = 3.6f;
        const float pa = 2.0f * std::tan(0.5f * cam.fovY) / 800.0f;
        const double clearanceStart = clearance(cam, m);
        bool alwaysOutside = true;
        for (int i = 0; i < 120; ++i) {
            zoomAlongRay(cam, m, glm::normalize(glm::dvec3(cam.forward())), pa, kZoomPerScrollUnit);
            if (distanceEstimate(m, cam.positionD()) <= 0.0) alwaysOutside = false;
        }
        check(alwaysOutside, "zooming never enters the set");
        check(clearance(cam, m) < clearanceStart * 0.01, "zooming gets close to the surface");
        std::printf("after 120 zoom steps: clearance %.4f -> %.3e\n", clearanceStart, clearance(cam, m));
    }

    if (failures == 0) std::printf("distance tests passed\n");
    return failures == 0 ? 0 : 1;
}
