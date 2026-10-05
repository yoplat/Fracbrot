#include "app/App.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

void usage() {
    std::printf(
        "usage: fracbrot [options]\n"
        "  --frames N          quit after N frames\n"
        "  --screenshot FILE   save the last frame as PNG (use with --frames)\n"
        "  --size WxH          window size\n"
        "  --view X,Y,SCALE    initial center (any number of digits) and half-height\n"
        "  --perturb           force perturbation rendering in 2D (default: below scale 1e-3)\n"
        "  --iter N            max iterations\n"
        "  --palette N         palette preset index\n"
        "  --mode N            0 = 2D plane, 1 = 3D fractal, 2 = logistic map\n"
        "  --orbit YAW,PITCH,DIST  3D camera (radians)\n"
        "  --target X,Y,Z      3D camera target\n"
        "  --formula N         3D formula: 0 = quaternion Mandelbrot, 1 = quaternion Julia, 2 = Mandelbulb, 3 = bicomplex\n"
        "  --iter3d N          3D fractal iterations\n"
        "  --detail F          3D hit threshold in pixels (smaller = finer)\n"
        "  --power F           Mandelbulb exponent\n"
        "  --rotate XZ,YZ,XW   quaternion per-iteration rotation (degrees)\n"
        "  --render-scale F    3D render resolution multiplier (default 0.75)\n"
        "  --trace X,Y         show the orbit of c = X + iY in 2D mode\n"
        "  --bench [FILE]      time the benchmark scenes, print a table and write JSON to FILE\n"
        "  --headless          render offscreen without showing a window (for scripted runs)\n"
        "  --no-ui             hide the UI panel\n");
}

} // namespace

int main(int argc, char** argv) {
    fb::AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const bool hasNext = i + 1 < argc;
        if (!std::strcmp(a, "--frames") && hasNext) {
            opts.maxFrames = std::atoll(argv[++i]);
        } else if (!std::strcmp(a, "--screenshot") && hasNext) {
            opts.screenshot = argv[++i];
        } else if (!std::strcmp(a, "--size") && hasNext) {
            std::sscanf(argv[++i], "%dx%d", &opts.width, &opts.height);
        } else if (!std::strcmp(a, "--view") && hasNext) {
            opts.camera = fb::Camera2D::parse(argv[++i]);
        } else if (!std::strcmp(a, "--iter") && hasNext) {
            opts.maxIter = uint32_t(std::atoi(argv[++i]));
        } else if (!std::strcmp(a, "--palette") && hasNext) {
            opts.palette = std::atoi(argv[++i]);
        } else if (!std::strcmp(a, "--mode") && hasNext) {
            opts.mode = fb::ViewMode(std::clamp(std::atoi(argv[++i]), 0, int(fb::ViewMode::Count) - 1));
        } else if (!std::strcmp(a, "--orbit") && hasNext) {
            fb::OrbitCamera orbit;
            if (std::sscanf(argv[++i], "%f,%f,%f", &orbit.yaw, &orbit.pitch, &orbit.distance) == 3) opts.orbit = orbit;
        } else if (!std::strcmp(a, "--target") && hasNext) {
            glm::vec3 t;
            if (std::sscanf(argv[++i], "%f,%f,%f", &t.x, &t.y, &t.z) == 3) opts.target = t;
        } else if (!std::strcmp(a, "--formula") && hasNext) {
            opts.formula = std::atoi(argv[++i]);
        } else if (!std::strcmp(a, "--iter3d") && hasNext) {
            opts.iter3d = uint32_t(std::atoi(argv[++i]));
        } else if (!std::strcmp(a, "--detail") && hasNext) {
            opts.detail = float(std::atof(argv[++i]));
        } else if (!std::strcmp(a, "--power") && hasNext) {
            opts.power = float(std::atof(argv[++i]));
        } else if (!std::strcmp(a, "--render-scale") && hasNext) {
            opts.renderScale = float(std::atof(argv[++i]));
        } else if (!std::strcmp(a, "--rotate") && hasNext) {
            glm::vec3 r;
            if (std::sscanf(argv[++i], "%f,%f,%f", &r.x, &r.y, &r.z) == 3) opts.rotation = r;
        } else if (!std::strcmp(a, "--trace") && hasNext) {
            double x, y;
            if (std::sscanf(argv[++i], "%lf,%lf", &x, &y) == 2) opts.traceC = std::make_pair(x, y);
        } else if (!std::strcmp(a, "--perturb")) {
            opts.perturb = true;
        } else if (!std::strcmp(a, "--bench")) {
            opts.bench = true;
            if (hasNext && argv[i + 1][0] != '-') opts.benchOut = argv[++i];
        } else if (!std::strcmp(a, "--headless")) {
            opts.headless = true;
        } else if (!std::strcmp(a, "--no-ui")) {
            opts.hideUi = true;
        } else {
            usage();
            return !std::strcmp(a, "--help") ? 0 : 1;
        }
    }

    static fb::App app;
    if (!app.init(opts)) return 1;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([] { app.frame(); }, 0, false);
#else
    while (app.frame()) {}
    app.shutdown();
#endif
    return fb::Context::errorCount > 0 ? 2 : 0;
}
