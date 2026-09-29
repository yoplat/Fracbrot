#include "app/App.h"

#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

int main(int argc, char** argv) {
    fb::AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) opts.maxFrames = std::atoll(argv[++i]);
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
