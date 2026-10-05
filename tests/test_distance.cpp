#include "fractal/DistanceEstimator.h"

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

    // Scroll-zooming toward the surface: 60 notches of dolly(0.9), re-pivoting each time like
    // the app does. The camera must approach the surface without ever entering the set.
    {
        MarchParams m;
        applyFormulaDefaults(m, Formula3D::QuatMandelbrot);
        OrbitCamera cam;
        cam.target = {-0.7f, 0.0f, 0.0f};
        cam.yaw = 0.12f;
        cam.pitch = 0.08f;
        cam.distance = 3.6f;
        const float pa = 2.0f * std::tan(0.5f * cam.fovY) / 800.0f;
        const glm::vec3 center = cam.target; // the old, fixed orbit pivot

        bool alwaysOutside = true;
        float worstOld = 0.0f, worstNew = 0.0f;
        auto ahead = [&] {
            const glm::vec3 eye = cam.position();
            return raycast(m, eye, glm::normalize(cam.target - eye), pa).value_or(1e9f);
        };
        const float aheadStart = ahead();
        float aheadPrev = aheadStart;
        for (int i = 0; i < 120; ++i) {
            // One pixel of drag rotates the orbit by 0.005 rad, moving the camera R * 0.005.
            // The nearest geometry, at distance e, then sweeps R * 0.005 / e rad across the screen.
            const glm::vec3 eye = cam.position();
            const float e = distanceEstimate(m, eye);
            worstOld = std::max(worstOld, glm::length(eye - center) * 0.005f / e / pa);
            pivotOnSurface(cam, m, pa);
            worstNew = std::max(worstNew, cam.distance * 0.005f / e / pa);
            zoomTowardSurface(cam, m, pa, 0.9f); // one scroll notch
            if (distanceEstimate(m, cam.position()) <= 0.0f) alwaysOutside = false;
            aheadPrev = ahead();
        }
        // Sub-pixel fins resolve into gaps as you approach, so "ahead" can jump; overall it
        // must still close in by orders of magnitude without ever entering the set.
        check(alwaysOutside, "zooming never enters the set");
        check(aheadPrev < aheadStart * 0.01f, "zooming gets close to the surface ahead");
        check(worstNew < 10.0f, "drag moves nearby geometry a few pixels per pixel at most");
        std::printf("after 120 zoom steps: surface ahead %.4f -> %.5f, nearest geometry %.5f\n", aheadStart,
                    aheadPrev, distanceEstimate(m, cam.position()));
        std::printf("worst drag speed of nearby geometry: %.0f px/px (old pivot) vs %.1f px/px (new)\n",
                    worstOld, worstNew);
    }

    if (failures == 0) std::printf("distance tests passed\n");
    return failures == 0 ? 0 : 1;
}
