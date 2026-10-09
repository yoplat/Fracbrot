#include "app/KeyRepeat.h"
#include "fractal/DistanceEstimator.h"
#include "fractal/Navigator3D.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace {

using namespace fb;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

// A Retina-sized framebuffer rendered at the default 0.75 scale, as the app does.
constexpr float kW = 2560.0f, kH = 1600.0f;
constexpr double kRenderScale = 0.75;

double pixelAngle(const OrbitCamera& cam) { return 2.0 * std::tan(0.5 * double(cam.fovY)) / (kH * kRenderScale); }

MarchParams formula(Formula3D f) {
    MarchParams m;
    applyFormulaDefaults(m, f);
    return m;
}

// The app's home views.
OrbitCamera quatHome() { return marchHome(formula(Formula3D::QuatMandelbrot)); }
OrbitCamera hyperHome() { return marchHome(formula(Formula3D::Hypercomplex)); }

glm::dvec3 rayThrough(const OrbitCamera& cam, glm::dvec2 px) {
    return cam.rayDirD({2.0 * px.x / kW - 1.0, 1.0 - 2.0 * px.y / kH}, double(kW) / kH);
}

// World point -> framebuffer pixel, in double precision.
glm::dvec2 project(const OrbitCamera& cam, glm::dvec3 p) {
    const glm::dvec3 v = p - cam.positionD();
    const double t = std::tan(0.5 * double(cam.fovY));
    const double z = glm::dot(v, glm::dvec3(cam.forward()));
    const double x = glm::dot(v, glm::dvec3(cam.right())) / (z * t * kW / kH);
    const double y = glm::dot(v, glm::dvec3(cam.up())) / (z * t);
    return {(x + 1.0) * 0.5 * kW, (1.0 - y) * 0.5 * kH};
}

NavInput frameInput(const OrbitCamera& cam, glm::dvec2 cursor) {
    NavInput in;
    in.cursor = in.prevCursor = cursor;
    in.dt = 1.0f / 60.0f;
    in.fbWidth = kW;
    in.fbHeight = kH;
    in.pixelAngle = pixelAngle(cam);
    return in;
}

void testScrollParity() {
    const MarchParams m = formula(Formula3D::QuatMandelbrot);
    OrbitCamera cam = quatHome();
    Navigator3D nav;
    const glm::dvec2 cursor{1500.0, 900.0};
    const glm::dvec3 dir = rayThrough(cam, cursor);
    const auto hit0 = raycast(m, cam.positionD(), dir, pixelAngle(cam));
    check(hit0.has_value(), "scroll parity: cursor ray hits");
    if (!hit0) return;
    const glm::dvec3 p = cam.positionD() + dir * *hit0;
    const glm::dvec3 eye0 = cam.positionD();

    check(kZoomPerScrollUnit == 0.9, "scroll parity: the 2D zoom rate");
    NavInput in = frameInput(cam, cursor);
    in.scroll = 1.0;
    nav.update(cam, m, in);
    // One unit moves 1 - 0.9 of the way to the hit: its depth shrinks to 0.9, the 2D zoom rate.
    const double moved = glm::length(cam.positionD() - eye0);
    check(std::fabs(moved - (1.0 - kZoomPerScrollUnit) * *hit0) < 1e-9 * *hit0, "scroll parity: moves 10% of the hit depth");
    check(std::fabs(glm::length(p - cam.positionD()) - kZoomPerScrollUnit * *hit0) < 1e-9 * *hit0,
          "scroll parity: hit depth magnified by 1/0.9");
    check(glm::length(project(cam, p) - cursor) < 1e-3, "scroll parity: point under the cursor stays put");
    const auto hit1 = raycast(m, cam.positionD(), rayThrough(cam, cursor), pixelAngle(cam));
    check(hit1 && std::fabs(*hit1 / *hit0 - kZoomPerScrollUnit) < 1e-3, "scroll parity: raycast agrees");
}

void testBackgroundZoom() {
    const MarchParams m = formula(Formula3D::Hypercomplex);
    OrbitCamera cam = hyperHome();
    const OrbitCamera start = cam;
    Navigator3D nav;
    const glm::dvec2 cursor{200.0, 200.0};
    check(!raycast(m, cam.positionD(), rayThrough(cam, cursor), pixelAngle(cam)), "background: cursor ray misses");

    // Past ~66 notches the focus drops below 1e-3 clearances; it must still retrace.
    constexpr int kNotches = 100;
    std::vector<double> steps;
    for (int dir : {1, -1}) {
        for (int i = 0; i < kNotches; ++i) {
            const glm::dvec3 before = cam.positionD();
            NavInput in = frameInput(cam, cursor);
            in.scroll = dir;
            nav.update(cam, m, in);
            steps.push_back(glm::length(cam.positionD() - before));
        }
    }
    const double err = glm::length(cam.positionD() - start.positionD()) / glm::length(start.positionD());
    check(err < 1e-9, "background: 100 in then 100 out returns to the start");
    check(std::fabs(cam.distance - start.distance) < 1e-9 * start.distance, "background: focus restored");
    check(steps[0] > 0.0 && *std::max_element(steps.begin(), steps.end()) <= steps[0] * (1.0 + 1e-9),
          "background: no step exceeds the first");
    check(steps[39] < steps[0] * 0.05, "background: zooming in converges geometrically");
    std::printf("background zoom: first step %.4f, 40th %.2e, round trip error %.1e\n", steps[0], steps[39], err);

    // Near the silhouette the cursor ray passes within focus / 100 of the surface without
    // hitting it. Capping the focus at 100 clearances on scroll frames used to shrink it there,
    // so zooming back out stopped about 2x short of the start.
    {
        const MarchParams q = formula(Formula3D::QuatMandelbrot);
        OrbitCamera c = quatHome();
        const OrbitCamera from = c;
        Navigator3D n;
        const glm::dvec2 near{1304.0, 480.0};
        bool missed = true;
        for (int dir : {1, -1}) {
            for (int i = 0; i < 40; ++i) {
                missed = missed && !raycast(q, c.positionD(), rayThrough(c, near), pixelAngle(c));
                NavInput in = frameInput(c, near);
                in.scroll = dir;
                n.update(c, q, in);
            }
        }
        const double e = glm::length(c.positionD() - from.positionD()) / glm::length(from.positionD());
        check(missed, "background: the near-silhouette ray misses throughout");
        check(e < 1e-9, "background: near the silhouette, 40 in then 40 out returns to the start");
    }

    // Fly-away limit: holding S (or X) accelerates with the clearance; it stops at the far
    // limit instead of running off to infinity / NaN.
    for (bool zoomOut : {false, true}) {
        OrbitCamera c = hyperHome();
        Navigator3D n;
        for (int i = 0; i < (zoomOut ? 6000 : 1200); ++i) {
            NavInput in = frameInput(c, {kW / 2, kH / 2});
            (zoomOut ? in.zoomOut : in.back) = true;
            n.update(c, m, in);
        }
        const double d = glm::length(c.positionD() - glm::dvec3(m.boundCenter));
        check(std::isfinite(d) && std::isfinite(c.distance) && d <= 1000.0 * m.boundRadius && d > 10.0,
              zoomOut ? "background: holding X stops at the far limit" : "background: holding S stops at the far limit");
    }
}

void testOrbit() {
    const MarchParams m = formula(Formula3D::QuatMandelbrot);
    OrbitCamera cam = quatHome();
    cam.distance = 6.0; // far enough that a half turn around the grabbed point stays outside
    Navigator3D nav;
    glm::dvec2 cursor{1400.0, 820.0};
    const glm::dvec3 dir = rayThrough(cam, cursor);
    const auto hit = raycast(m, cam.positionD(), dir, pixelAngle(cam));
    check(hit.has_value(), "orbit: cursor ray hits");
    if (!hit) return;
    const glm::dvec3 grab = cam.positionD() + dir * *hit;
    const glm::dvec2 grabPx = project(cam, grab);
    const float yaw0 = cam.yaw;

    NavInput in = frameInput(cam, cursor);
    in.left = true;
    nav.update(cam, m, in); // press: grab
    bool kept = true;
    for (int i = 0; i < 16; ++i) { // a full framebuffer height to the right, in 16 moves
        in.prevCursor = cursor;
        cursor.x += kH / 16.0;
        in.cursor = cursor;
        nav.update(cam, m, in);
        if (glm::length(project(cam, grab) - grabPx) > 0.05) kept = false;
    }
    check(kept, "orbit: the grabbed point keeps its screen position");
    check(std::fabs(double(cam.yaw - yaw0) + std::numbers::pi) < 1e-5, "orbit: a full-height drag turns half a turn");
}

void testPan() {
    const MarchParams m = formula(Formula3D::Hypercomplex);
    OrbitCamera cam = hyperHome();
    Navigator3D nav;
    glm::dvec2 cursor{1300.0, 790.0};
    const glm::dvec3 dir = rayThrough(cam, cursor);
    const auto hit = raycast(m, cam.positionD(), dir, pixelAngle(cam));
    check(hit.has_value(), "pan: cursor ray hits");
    const glm::dvec3 grab = cam.positionD() + dir * hit.value_or(cam.distance);

    NavInput in = frameInput(cam, cursor);
    in.left = true;
    in.shift = true;
    nav.update(cam, m, in);
    in.shift = false; // releasing shift mid-drag keeps panning
    bool follows = true;
    for (int i = 0; i < 10; ++i) {
        in.prevCursor = cursor;
        cursor += glm::dvec2(37.0, -23.0);
        in.cursor = cursor;
        nav.update(cam, m, in);
        if (glm::length(project(cam, grab) - cursor) > 0.01) follows = false;
    }
    check(follows, "pan: the grabbed point follows the cursor 1:1");
}

void testNeverInside() {
    std::mt19937 rng(1234);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    int inside = 0, actions = 0;
    for (Formula3D f : {Formula3D::Hypercomplex, Formula3D::QuatMandelbrot, Formula3D::Mandelbulb}) {
        const MarchParams m = formula(f);
        for (int depth : {0, 20, 60, 120}) {
            OrbitCamera cam = marchHome(m);
            Navigator3D nav;
            // Start at varied depths: scroll toward the surface at the screen center.
            for (int i = 0; i < depth; ++i) {
                NavInput in = frameInput(cam, {kW / 2 + 40.0, kH / 2 + 25.0});
                in.scroll = 1.0;
                nav.update(cam, m, in);
            }
            glm::dvec2 cursor{kW / 2, kH / 2};
            for (int i = 0; i < 200; ++i) {
                NavInput in = frameInput(cam, cursor);
                in.dt = float(uni(0.005, 0.1));
                switch (int(uni(0.0, 4.0))) {
                case 0: // scroll at a random cursor
                    in.cursor = in.prevCursor = cursor = {uni(0.0, kW), uni(0.0, kH)};
                    in.scroll = std::round(uni(-6.0, 6.0));
                    nav.update(cam, m, in);
                    break;
                case 1: { // drag (orbit or pan) from a random cursor
                    in.cursor = in.prevCursor = cursor = {uni(0.0, kW), uni(0.0, kH)};
                    in.left = true;
                    in.shift = uni(0.0, 1.0) < 0.4;
                    nav.update(cam, m, in);
                    for (int k = 0; k < 4; ++k) {
                        in.prevCursor = cursor;
                        cursor += glm::dvec2(uni(-300.0, 300.0), uni(-300.0, 300.0));
                        in.cursor = cursor;
                        nav.update(cam, m, in);
                    }
                    in.left = false;
                    nav.update(cam, m, in);
                    break;
                }
                default: // keys
                    in.forward = uni(0.0, 1.0) < 0.5;
                    in.back = !in.forward && uni(0.0, 1.0) < 0.3;
                    in.leftKey = uni(0.0, 1.0) < 0.3;
                    in.rightKey = !in.leftKey && uni(0.0, 1.0) < 0.3;
                    in.up = uni(0.0, 1.0) < 0.3;
                    in.down = !in.up && uni(0.0, 1.0) < 0.3;
                    in.zoomIn = uni(0.0, 1.0) < 0.3;
                    in.zoomOut = !in.zoomIn && uni(0.0, 1.0) < 0.2;
                    for (int k = 0; k < 5; ++k) nav.update(cam, m, in);
                    break;
                }
                ++actions;
                if (!(distanceEstimate(m, cam.positionD()) > 0.0) || !std::isfinite(cam.distance)) ++inside;
            }
        }
    }
    check(inside == 0, "random navigation never enters the set");
    std::printf("random navigation: %d actions, %d inside\n", actions, inside);
}


void testDeepZoomOut() {
    // Deep in, orbit a little so a surface ends up behind the camera, then scroll out at the
    // same cursor: backing off must stop short of what is behind, never freeze the camera
    // (a frame that would land inside used to be reverted, every frame).
    int runs = 0, frozen = 0, slow = 0;
    for (Formula3D f : {Formula3D::Mandelbulb, Formula3D::QuatJulia, Formula3D::Hypercomplex}) {
        const MarchParams m = formula(f);
        for (double x = kW * 0.3; x < kW * 0.7; x += kW * 0.1) {
            for (double y = kH * 0.3; y < kH * 0.7; y += kH * 0.1) {
                OrbitCamera cam = marchHome(m);
                Navigator3D nav;
                const glm::dvec2 c{x, y};
                if (!raycast(m, cam.positionD(), rayThrough(cam, c), pixelAngle(cam))) continue;
                for (int i = 0; i < 600 && clearance(cam, m) > 1e-6; ++i) {
                    NavInput in = frameInput(cam, c);
                    in.scroll = 2.0;
                    nav.update(cam, m, in);
                }
                NavInput in = frameInput(cam, c);
                in.left = true;
                nav.update(cam, m, in);
                for (int k = 0; k < 10; ++k) {
                    in.prevCursor = in.cursor;
                    in.cursor.x += 10.0;
                    nav.update(cam, m, in);
                }
                in.left = false;
                nav.update(cam, m, in);
                ++runs;
                // Back out to about the home scale (further on, the far limit stops it).
                for (int k = 0; k < 100 && clearance(cam, m) < 0.1; ++k) {
                    NavInput out = frameInput(cam, c);
                    out.scroll = -2.0;
                    const glm::dvec3 before = cam.positionD();
                    nav.update(cam, m, out);
                    if (cam.positionD() == before) {
                        ++frozen;
                        break;
                    }
                }
                if (clearance(cam, m) < 0.1) ++slow;
            }
        }
    }
    check(runs > 20 && frozen == 0, "deep zoom out: the camera moves on every frame");
    check(slow == 0, "deep zoom out: 200 units out get back from 1e-6 to the home scale");
    std::printf("deep zoom out: %d runs, %d frozen, %d slow\n", runs, frozen, slow);

    // Cases that used to glue the camera to a surface while scrolling out: backing off past a
    // surface beside the path (the focus cap then shrank every next step with the clearance),
    // and backing into one the march behind the camera misses (each step was cut to 0.9
    // clearances, so the clearance fell 10x per frame). Nothing but R got the camera out.
    struct Trap {
        Formula3D formula;
        glm::dvec2 cursor;
        double orbitDy; // vertical drag (px) at the deepest point before zooming out
    };
    const Trap traps[] = {{Formula3D::Mandelbulb, {1664.0, 560.0}, 0.0}, {Formula3D::Bicomplex, {1472.0, 1160.0}, 100.0}};
    for (const Trap& t : traps) {
        const MarchParams m = formula(t.formula);
        OrbitCamera cam = marchHome(m);
        Navigator3D nav;
        for (int i = 0; i < 800 && clearance(cam, m) > 1e-6; ++i) {
            NavInput in = frameInput(cam, t.cursor);
            in.scroll = 2.0;
            nav.update(cam, m, in);
        }
        const double deep = clearance(cam, m);
        if (t.orbitDy != 0.0) {
            NavInput in = frameInput(cam, t.cursor);
            in.left = true;
            nav.update(cam, m, in);
            for (int k = 0; k < 10; ++k) {
                in.prevCursor = in.cursor;
                in.cursor.y += t.orbitDy / 10.0;
                nav.update(cam, m, in);
            }
            in.left = false;
            nav.update(cam, m, in);
        }
        for (int k = 0; k < 200; ++k) {
            NavInput in = frameInput(cam, t.cursor);
            in.scroll = -1.0;
            nav.update(cam, m, in);
        }
        check(deep < 2e-6 && clearance(cam, m) > 1e-3,
              "deep zoom out: scrolling out never glues the camera to a surface");
        std::printf("deep zoom out trap case: clearance %.1e -> %.1e\n", deep, clearance(cam, m));
    }
}

void testZoomOutRetraces() {
    // Ordinary zoom-out at moderate depth: each notch backs straight off along the cursor ray,
    // so the point under the cursor stays put and N notches out undo N in. Escaping sideways
    // there (meant for deep traps) made notches jump ~190 px and even zoom in.
    const MarchParams m = formula(Formula3D::Hypercomplex);
    OrbitCamera cam = hyperHome();
    Navigator3D nav;
    const glm::dvec2 c{2048.0, 1040.0};
    const auto depth0 = raycast(m, cam.positionD(), rayThrough(cam, c), pixelAngle(cam));
    check(depth0.has_value(), "zoom out retraces: cursor ray hits");
    if (!depth0) return;
    for (int i = 0; i < 30; ++i) {
        NavInput in = frameInput(cam, c);
        in.scroll = 1.0;
        nav.update(cam, m, in);
    }
    int jumps = 0;
    for (int i = 0; i < 30; ++i) {
        const glm::dvec3 eye = cam.positionD(), dir = rayThrough(cam, c);
        const auto hit = raycast(m, eye, dir, pixelAngle(cam));
        NavInput in = frameInput(cam, c);
        in.scroll = -1.0;
        nav.update(cam, m, in);
        if (hit && glm::length(project(cam, eye + dir * *hit) - c) > 1.0) ++jumps;
    }
    const auto depth = raycast(m, cam.positionD(), rayThrough(cam, c), pixelAngle(cam));
    check(jumps == 0, "zoom out retraces: the point under the cursor stays put");
    check(depth && *depth > 0.5 * *depth0 && *depth < 2.0 * *depth0, "zoom out retraces: 30 out undo 30 in");
}

void testHoldZoomOutDeep() {
    // Holding X deep in, with background at the screen center: the focus cap used to shrink
    // every back-off with the clearance, walking the camera onto a surface behind it.
    const MarchParams m = formula(Formula3D::QuatJulia);
    OrbitCamera cam = marchHome(m);
    Navigator3D nav;
    const glm::dvec2 c{1856.0, 560.0};
    for (int i = 0; i < 800 && clearance(cam, m) > 1e-6; ++i) {
        NavInput in = frameInput(cam, c);
        in.scroll = 2.0;
        nav.update(cam, m, in);
    }
    const double deep = clearance(cam, m);
    for (int i = 0; i < 300; ++i) {
        NavInput in = frameInput(cam, c);
        in.zoomOut = true;
        nav.update(cam, m, in);
    }
    check(deep < 2e-6 && clearance(cam, m) > deep, "hold X deep in: zooms out instead of onto a surface");
    std::printf("hold X deep in: clearance %.1e -> %.1e\n", deep, clearance(cam, m));
}

void testFocusFloor() {
    // At a near-miss pixel of a thin needle the cursor ray stops hitting after a notch, so
    // scrolling shrinks the focus far below the clearance. Strafing, background grabs and
    // pans must still move at clearance scale.
    const MarchParams m = formula(Formula3D::Hypercomplex);
    OrbitCamera cam = hyperHome();
    Navigator3D nav;
    const glm::dvec2 c{896.0, 800.0};
    for (int i = 0; i < 300 && clearance(cam, m) > 1e-6; ++i) {
        NavInput in = frameInput(cam, c);
        in.scroll = 2.0;
        nav.update(cam, m, in);
    }
    const double clr = clearance(cam, m);
    const glm::dvec3 before = cam.positionD();
    for (int k = 0; k < 60; ++k) {
        NavInput in = frameInput(cam, c);
        in.leftKey = true;
        nav.update(cam, m, in);
    }
    const double moved = glm::length(cam.positionD() - before) / clr;
    check(moved > 0.3, "focus floor: A moves at least 0.3 clearances per second after a deep scroll");
    check(cam.distance >= 0.5 * clearance(cam, m), "focus floor: moving raises the focus to the clearance");
    std::printf("focus floor: strafe %.2f clearances/s\n", moved);
}

void testScrollCarryAndFocusCap() {
    // A 10-unit flick lands as 4 + 4 + 2 over three frames, nothing dropped.
    const MarchParams m = formula(Formula3D::Hypercomplex);
    OrbitCamera cam = hyperHome();
    Navigator3D nav;
    const glm::dvec2 c{200.0, 200.0}; // background: each frame scales the focus by 0.9^units
    std::vector<double> units;
    for (int i = 0; i < 4; ++i) {
        NavInput in = frameInput(cam, c);
        in.scroll = i == 0 ? 10.0 : 0.0;
        const double focus = cam.distance;
        nav.update(cam, m, in);
        units.push_back(std::log(cam.distance / focus) / std::log(kZoomPerScrollUnit));
    }
    check(std::fabs(units[0] - 4.0) < 1e-9 && std::fabs(units[1] - 4.0) < 1e-9 && std::fabs(units[2] - 2.0) < 1e-9 &&
              std::fabs(units[3]) < 1e-12,
          "scroll carry: 10 units apply as 4, 4, 2");

    // A view reset drops the carry, so a flick doesn't keep zooming the fresh view.
    NavInput in = frameInput(cam, c);
    in.scroll = 20.0;
    nav.update(cam, m, in);
    nav.reset();
    cam = hyperHome();
    const OrbitCamera home = cam;
    nav.update(cam, m, frameInput(cam, c));
    check(cam.positionD() == home.positionD() && cam.distance == home.distance, "scroll carry: reset drops it");

    // After a move the focus is at most 100 clearances, even with nothing at the center.
    cam = hyperHome();
    cam.distance = 1e6; // as if after a long zoom out over background
    cam.target = hyperHome().positionD() - glm::dvec3(cam.back()) * cam.distance;
    NavInput key = frameInput(cam, c);
    key.rightKey = true;
    nav.update(cam, m, key);
    check(cam.distance <= 100.0 * clearance(cam, m) * (1.0 + 1e-12), "focus cap: at most 100 clearances");
}

void testRaycastBudget() {
    // raycast takes exactly m.maxSteps steps, like the shader: count the steps a hit needs by
    // hand, then it must hit with that budget and miss with one less.
    MarchParams m = formula(Formula3D::QuatMandelbrot);
    const OrbitCamera cam = quatHome();
    const glm::dvec3 eye = cam.positionD(), dir = rayThrough(cam, {1500.0, 900.0});
    const double pa = pixelAngle(cam);
    const glm::dvec3 oc = eye - glm::dvec3(m.boundCenter);
    const double b = glm::dot(oc, dir);
    const double h = b * b - (glm::dot(oc, oc) - double(m.boundRadius) * m.boundRadius);
    check(h > 0.0, "budget: ray enters the bounds");
    double t = std::max(-b - std::sqrt(h), 0.0);
    uint32_t steps = 0;
    for (; steps < 10000; ++steps) {
        const double d = distanceEstimate(m, eye + dir * t);
        if (d < std::max(t * pa * m.detail, 1e-15)) break;
        t += d;
    }
    ++steps; // the step that hits
    check(steps > 4 && steps < 10000, "budget: the hit takes a few steps");
    m.maxSteps = steps;
    check(raycast(m, eye, dir, pa).has_value(), "budget: hits within m.maxSteps");
    m.maxSteps = steps - 1;
    check(!raycast(m, eye, dir, pa).has_value(), "budget: no steps beyond m.maxSteps");
}

void testIterationStep() {
    check(stepIterations(256, +1, 16, 20000) == 320, "step: up 25%");
    check(stepIterations(256, -1, 16, 20000) == 205, "step: down 25%");
    check(stepIterations(2, +1, 2, 512) == 3 && stepIterations(3, -1, 2, 512) == 2, "step: small values move by 1+");
    check(stepIterations(4, +1, 2, 512) == 5, "step: at least +1");
    check(stepIterations(2, -1, 2, 512) == 2, "step: clamped at the bottom");
    check(stepIterations(19000, +1, 16, 20000) == 20000 && stepIterations(20000, +1, 16, 20000) == 20000,
          "step: clamped at the top");
    // Out-of-range values (e.g. from the command line) never jump the wrong way.
    check(stepIterations(30000, +1, 16, 20000) == 30000, "step: above range, up stays");
    check(stepIterations(30000, -1, 16, 20000) == 24000, "step: above range, down moves down");
    check(stepIterations(8, -1, 16, 20000) == 8, "step: below range, down stays");
    check(stepIterations(8, +1, 16, 20000) == 10, "step: below range, up moves up");
    check(stepIterations(100, 0, 16, 20000) == 100, "step: no direction");

    // Typematic: one step on the press, nothing until 0.35 s, then one every 0.10 s.
    {
        KeyRepeat r;
        const float dt = 0.05f;
        std::vector<int> fired;
        fired.push_back(r.update(true, dt));
        for (int i = 1; i <= 12; ++i) fired.push_back(r.update(true, dt)); // held for 0.60 s
        const std::vector<int> expected{1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0};
        check(fired == expected, "repeat: press, 0.35 s delay, then every 0.10 s (50 ms frames)");
        check(r.update(false, dt) == 0, "repeat: release fires nothing");
        check(r.update(true, dt) == 1, "repeat: pressing again fires at once");
    }
    {
        // Uneven 120 Hz-ish frames: repeats land within a frame of 0.35, 0.45, 0.55, ...
        KeyRepeat r;
        double t = 0.0;
        std::vector<double> times;
        check(r.update(true, 0.0f) == 1, "repeat: press");
        for (int i = 0; i < 240; ++i) {
            const float dt = i % 3 == 0 ? 0.011f : 0.007f;
            t += dt;
            for (int n = r.update(true, dt); n > 0; --n) times.push_back(t);
        }
        bool onTime = !times.empty();
        for (size_t k = 0; k < times.size(); ++k) {
            const double due = KeyRepeat::kDelay + KeyRepeat::kInterval * double(k);
            if (times[k] < due - 1e-6 || times[k] > due + 0.011 + 1e-6) onTime = false;
        }
        check(onTime && times.size() == size_t((t - KeyRepeat::kDelay) / KeyRepeat::kInterval) + 1,
              "repeat: timed by dt, not by frame count");
    }
}

void testIterationGuard() {
    // On the real axis the hypercomplex formula is the plain Mandelbrot set, whose escape time
    // near the cusp at c = 0.25 grows like pi / sqrt(c - 0.25): a point just right of it
    // escapes within 64 iterations but not within 51 (one step down).
    const MarchParams m = formula(Formula3D::Hypercomplex);
    const uint32_t lower = stepIterations(m.iterations, -1, 2, 512);
    check(m.iterations == 64 && lower == 51, "guard: 64 steps down to 51");
    MarchParams fewer = m;
    fewer.iterations = lower;
    std::optional<glm::dvec3> between;
    for (double x = 0.26; x > 0.25 && !between; x -= 1e-6) {
        const glm::dvec3 p{x, 0.0, 0.0};
        if (distanceEstimate(m, p) > 0.0 && distanceEstimate(fewer, p) <= 0.0) between = p;
    }
    check(between.has_value(), "guard: found a point outside at 64 iterations, inside at 51");
    if (!between) return;
    OrbitCamera cam = hyperHome();
    cam.target = *between - glm::dvec3(cam.back()) * cam.distance; // put the camera there
    check(glm::length(cam.positionD() - *between) < 1e-12, "guard: camera placed");
    check(guardIterations(cam, m, lower) == m.iterations, "guard: refuses a decrease that swallows the camera");
    check(guardIterations(cam, m, 80) == 80, "guard: increases always pass");
    check(guardIterations(hyperHome(), m, lower) == lower, "guard: decreases pass away from the surface");
}

} // namespace

int main() {
    testScrollParity();
    testBackgroundZoom();
    testOrbit();
    testPan();
    testNeverInside();
    testDeepZoomOut();
    testZoomOutRetraces();
    testHoldZoomOutDeep();
    testFocusFloor();
    testScrollCarryAndFocusCap();
    testRaycastBudget();
    testIterationStep();
    testIterationGuard();
    if (failures == 0) std::printf("navigation tests passed\n");
    return failures == 0 ? 0 : 1;
}
