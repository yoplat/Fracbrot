#pragma once

#include "app/Gui.h"
#include "camera/Camera2D.h"
#include "camera/OrbitCamera.h"
#include "fractal/Bifurcation.h"
#include "fractal/Mandel2D.h"
#include "fractal/OrbitTracer.h"
#include "fractal/Palette.h"
#include "fractal/Raymarcher.h"
#include "fractal/Terrain.h"
#include "gfx/Context.h"
#include "platform/Window.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace fb {

enum class ViewMode : int { Plane2D = 0, Heightfield = 1, Fractal3D = 2, LogisticMap = 3, Count };

struct AppOptions {
    int width = 1280;
    int height = 800;
    int64_t maxFrames = -1;  // quit after N frames (for automated runs), -1 = forever
    std::string screenshot;  // if set, save the last frame to this PNG before quitting
    bool hideUi = false;
    std::optional<Camera2D> camera;
    std::optional<OrbitCamera> orbit; // applies to the starting 3D mode
    uint32_t maxIter = 256;
    int palette = 0;
    ViewMode mode = ViewMode::Plane2D;
    int formula = 0; // Formula3D
    std::optional<std::pair<double, double>> traceC; // pin the 2D orbit tracer to this c
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
    void handleInputOrbit(OrbitCamera& cam, float dt, double mx, double my, double scroll, bool mouseFree,
                          bool keysFree);
    void resetCameras();
    void animate(double time);
    void drawUi();
    void setPalette(int index);
    void updateScene(const OrbitCamera& orbit);
    OrbitCamera& activeOrbit();
    std::string nextScreenshotPath() const;

    AppOptions m_opts;
    Window m_window;
    Context m_ctx;
    Gui m_gui;
    PaletteTexture m_palette;
    Mandel2D m_mandel;
    Terrain m_terrain;
    Raymarcher m_raymarcher;
    Bifurcation m_bifurcation;
    OrbitTracer m_orbitTracer;

    ViewMode m_mode = ViewMode::Plane2D;
    Camera2D m_camera;   // region of the complex plane (also the terrain extent)
    OrbitCamera m_orbit; // heightfield viewpoint
    OrbitCamera m_marchOrbit; // 3D fractal viewpoint
    MarchParams m_march;
    float m_renderScale = 0.75f;
    bool m_animate = false;   // sweeps sliceW / power / Julia constant over time
    OrbitCamera m_logisticOrbit;
    BifurcationParams m_bif;
    float m_logisticR = 3.5f;
    bool m_highlightR = true;
    bool m_autoRotate = false;
    bool m_showOrbit = false;
    int m_orbitEscape = -1;
    double m_orbitC[2] = {0.0, 0.0};
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
