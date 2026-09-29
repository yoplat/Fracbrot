#include "app/App.h"

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
        "  --view X,Y,SCALE    initial center and half-height\n"
        "  --iter N            max iterations\n"
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
            fb::Camera2D cam;
            if (std::sscanf(argv[++i], "%lf,%lf,%lf", &cam.cx, &cam.cy, &cam.scale) == 3) opts.camera = cam;
        } else if (!std::strcmp(a, "--iter") && hasNext) {
            opts.maxIter = uint32_t(std::atoi(argv[++i]));
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
    return 0;
}
