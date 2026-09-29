#pragma once

#include "app/Gui.h"
#include "camera/Camera2D.h"
#include "fractal/Mandel2D.h"
#include "gfx/Context.h"
#include "platform/Window.h"

#include <cstdint>
#include <optional>
#include <string>

namespace fb {

struct AppOptions {
    int width = 1280;
    int height = 800;
    int64_t maxFrames = -1;  // quit after N frames (for automated runs), -1 = forever
    std::string screenshot;  // if set, save the last frame to this PNG before quitting
    bool hideUi = false;
    std::optional<Camera2D> camera;
    uint32_t maxIter = 256;
};

class App {
public:
    bool init(const AppOptions& opts);
    // Renders one frame. Returns false when the app should quit.
    bool frame();
    void shutdown();

private:
    void handleInput(float dt);
    void drawUi();

    AppOptions m_opts;
    Window m_window;
    Context m_ctx;
    Gui m_gui;
    Mandel2D m_mandel;
    Camera2D m_camera;
    FractalParams m_params;

    int64_t m_frame = 0;
    double m_lastTime = 0.0;
    double m_lastCursor[2] = {0.0, 0.0};
    bool m_dragging = false;
};

} // namespace fb
