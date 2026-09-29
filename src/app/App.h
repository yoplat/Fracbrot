#pragma once

#include "app/Gui.h"
#include "camera/Camera2D.h"
#include "camera/OrbitCamera.h"
#include "fractal/Mandel2D.h"
#include "fractal/Palette.h"
#include "fractal/Terrain.h"
#include "gfx/Context.h"
#include "platform/Window.h"

#include <cstdint>
#include <optional>
#include <string>

namespace fb {

enum class ViewMode : int { Plane2D = 0, Heightfield = 1, Count };

struct AppOptions {
    int width = 1280;
    int height = 800;
    int64_t maxFrames = -1;  // quit after N frames (for automated runs), -1 = forever
    std::string screenshot;  // if set, save the last frame to this PNG before quitting
    bool hideUi = false;
    std::optional<Camera2D> camera;
    std::optional<OrbitCamera> orbit;
    uint32_t maxIter = 256;
    int palette = 0;
    ViewMode mode = ViewMode::Plane2D;
};

class App {
public:
    bool init(const AppOptions& opts);
    // Renders one frame. Returns false when the app should quit.
    bool frame();
    void shutdown();

private:
    void handleInput(float dt);
    void handleInput2D(float dt, double mx, double my, double scroll, bool mouseFree, bool keysFree);
    void handleInput3D(float dt, double mx, double my, double scroll, bool mouseFree, bool keysFree);
    void drawUi();
    void setPalette(int index);
    void updateScene();
    std::string nextScreenshotPath() const;

    AppOptions m_opts;
    Window m_window;
    Context m_ctx;
    Gui m_gui;
    PaletteTexture m_palette;
    Mandel2D m_mandel;
    Terrain m_terrain;

    ViewMode m_mode = ViewMode::Plane2D;
    Camera2D m_camera;   // region of the complex plane (also the terrain extent)
    OrbitCamera m_orbit; // 3D viewpoint
    FractalParams m_params;
    SceneParams m_scene;
    float m_lightAzimuth = 0.9f;
    float m_lightElevation = 0.6f;

    int64_t m_frame = 0;
    double m_lastTime = 0.0;
    double m_lastCursor[2] = {0.0, 0.0};
    bool m_dragging = false;
    int m_paletteIndex = 0;
    float m_colorCycleSpeed = 0.0f;
    bool m_screenshotRequested = false;
    bool m_screenshotWithUi = false;
    bool m_keyWasDown[512] = {};
    std::string m_lastScreenshot;

    bool keyPressed(int key); // edge-triggered
};

} // namespace fb
