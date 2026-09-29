#pragma once

#include "gfx/Context.h"
#include "platform/Window.h"

#include <cstdint>

namespace fb {

struct AppOptions {
    int width = 1280;
    int height = 800;
    int64_t maxFrames = -1; // quit after N frames (for automated runs), -1 = forever
};

class App {
public:
    bool init(const AppOptions& opts);
    // Renders one frame. Returns false when the app should quit.
    bool frame();
    void shutdown();

private:
    AppOptions m_opts;
    Window m_window;
    Context m_ctx;
    int64_t m_frame = 0;
};

} // namespace fb
