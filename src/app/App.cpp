#include "app/App.h"

#include "gfx/Readback.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace fb {

namespace {

const char* const kModeNames[] = {"2D plane", "Heightfield", "3D fractal", "Logistic map"};
const char* const kFormulaNames[] = {"Quaternion Mandelbrot", "Quaternion Julia", "Mandelbulb",
                                      "Bicomplex Mandelbrot (Tetrabrot)"};
static_assert(std::size(kFormulaNames) == size_t(Formula3D::Count));

OrbitCamera marchHome() {
    OrbitCamera cam;
    cam.target = {-0.75f, 0.0f, 0.0f};
    cam.yaw = 0.45f;   // mostly side-on, so the real-axis needle is visible
    cam.pitch = 0.3f;
    cam.distance = 3.2f;
    return cam;
}

OrbitCamera logisticHome() {
    OrbitCamera cam;
    cam.target = {-0.9f, 0.0f, 0.0f};
    cam.yaw = 0.35f;
    cam.pitch = 0.3f;
    cam.distance = 5.0f;
    return cam;
}
static_assert(std::size(kModeNames) == size_t(ViewMode::Count));

} // namespace

bool App::init(const AppOptions& opts) {
    m_opts = opts;
    if (!m_window.create(opts.width, opts.height, "Fracbrot")) return false;

    int w, h;
    m_window.framebufferSize(w, h);
    if (!m_ctx.init(m_window.handle(), uint32_t(w), uint32_t(h))) return false;

    m_gui.init(m_window.handle(), m_ctx);
    m_palette.init(m_ctx.device);
    setPalette(opts.palette);
    m_mandel.init(m_ctx, m_palette);
    m_mandel.setBlocking(opts.maxFrames >= 0); // automated runs need finished reference orbits
    m_mandel.setForcePerturbation(opts.perturb);
    m_terrain.init(m_ctx, m_palette);
    m_raymarcher.init(m_ctx, m_palette);
    m_bifurcation.init(m_ctx, m_palette);
    m_orbitTracer.init(m_ctx);

    applyFormulaDefaults(m_march, Formula3D(std::clamp(opts.formula, 0, int(Formula3D::Count) - 1)));
    if (opts.iter3d) m_march.iterations = opts.iter3d;
    if (opts.detail > 0.0f) m_march.detail = opts.detail;
    if (opts.power > 0.0f) m_march.power = opts.power;
    resetCameras();
    if (opts.camera) m_camera = *opts.camera;
    m_mode = opts.mode;
    m_showOrbit = opts.traceC.has_value();
    if (opts.orbit) {
        OrbitCamera& cam = activeOrbit();
        cam.yaw = opts.orbit->yaw;
        cam.pitch = opts.orbit->pitch;
        cam.distance = opts.orbit->distance;
    }
    m_params.maxIter = opts.maxIter;
    m_lastTime = glfwGetTime();
    return true;
}

void App::resetCameras() {
    m_camera.reset();
    m_orbit.reset();
    m_marchOrbit = marchHome();
    m_marchOrbit.target = m_march.boundCenter;
    m_logisticOrbit = logisticHome();
}

OrbitCamera& App::activeOrbit() {
    switch (m_mode) {
    case ViewMode::Fractal3D: return m_marchOrbit;
    case ViewMode::LogisticMap: return m_logisticOrbit;
    default: return m_orbit;
    }
}

void App::animate(double time) {
    if (!m_animate) return;
    const float t = float(time);
    switch (m_march.formula) {
    case Formula3D::QuatMandelbrot:
        m_march.sliceW = 0.5f * std::sin(t * 0.25f);
        break;
    case Formula3D::QuatJulia:
        m_march.juliaC = glm::vec4(-0.291f + 0.15f * std::cos(t * 0.21f), -0.399f + 0.2f * std::sin(t * 0.17f),
                                   0.339f + 0.15f * std::cos(t * 0.13f), 0.437f + 0.15f * std::sin(t * 0.11f));
        break;
    case Formula3D::Mandelbulb:
        m_march.power = 8.0f + 4.0f * std::sin(t * 0.2f);
        break;
    case Formula3D::Bicomplex:
        m_march.sliceW = 0.3f * std::sin(t * 0.25f);
        break;
    case Formula3D::Count:
        break;
    }
}

bool App::keyPressed(int key) {
    const bool down = m_window.keyDown(key);
    const bool pressed = down && !m_keyWasDown[key];
    m_keyWasDown[key] = down;
    return pressed;
}

void App::handleInput(float dt) {
    double mx, my;
    m_window.cursorPos(mx, my);
    const double scroll = m_window.consumeScroll();
    const bool mouseFree = !m_gui.wantsMouse();
    const bool keysFree = !m_gui.wantsKeyboard();

    if (keysFree) {
        if (keyPressed(GLFW_KEY_1)) m_mode = ViewMode::Plane2D;
        if (keyPressed(GLFW_KEY_2)) m_mode = ViewMode::Heightfield;
        if (keyPressed(GLFW_KEY_3)) m_mode = ViewMode::Fractal3D;
        if (keyPressed(GLFW_KEY_4)) m_mode = ViewMode::LogisticMap;
        if (keyPressed(GLFW_KEY_O)) m_showOrbit = !m_showOrbit;
        if (keyPressed(GLFW_KEY_TAB)) m_mode = ViewMode((int(m_mode) + 1) % int(ViewMode::Count));
        if (keyPressed(GLFW_KEY_P)) m_screenshotRequested = true;
        if (m_window.keyDown(GLFW_KEY_R)) resetCameras();
    }

    switch (m_mode) {
    case ViewMode::Plane2D: handleInput2D(dt, mx, my, scroll, mouseFree, keysFree); break;
    case ViewMode::Heightfield: handleInput3D(dt, mx, my, scroll, mouseFree, keysFree); break;
    default: handleInputOrbit(activeOrbit(), dt, mx, my, scroll, mouseFree, keysFree); break;
    }

    m_lastCursor[0] = mx;
    m_lastCursor[1] = my;
}

void App::handleInput2D(float dt, double mx, double my, double scroll, bool mouseFree, bool keysFree) {
    const double w = m_ctx.width, h = m_ctx.height;

    if (mouseFree) {
        if (scroll != 0.0) m_camera.zoomAt(mx, my, w, h, std::pow(0.9, scroll));
        const bool down = m_window.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        if (down && m_dragging) m_camera.pan(mx - m_lastCursor[0], my - m_lastCursor[1], h);
        m_dragging = down;
    } else {
        m_dragging = false;
    }

    if (keysFree) {
        const double panPx = 600.0 * dt;
        if (m_window.keyDown(GLFW_KEY_LEFT) || m_window.keyDown(GLFW_KEY_A)) m_camera.pan(panPx, 0, h);
        if (m_window.keyDown(GLFW_KEY_RIGHT) || m_window.keyDown(GLFW_KEY_D)) m_camera.pan(-panPx, 0, h);
        if (m_window.keyDown(GLFW_KEY_UP) || m_window.keyDown(GLFW_KEY_W)) m_camera.pan(0, panPx, h);
        if (m_window.keyDown(GLFW_KEY_DOWN) || m_window.keyDown(GLFW_KEY_S)) m_camera.pan(0, -panPx, h);
        if (m_window.keyDown(GLFW_KEY_E) || m_window.keyDown(GLFW_KEY_EQUAL))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(0.25, dt));
        if (m_window.keyDown(GLFW_KEY_Q) || m_window.keyDown(GLFW_KEY_MINUS))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(4.0, dt));
    }
}

void App::handleInput3D(float dt, double mx, double my, double scroll, bool mouseFree, bool keysFree) {
    const double h = m_ctx.height;

    // Moves the sampled region by a world-space delta on the ground plane.
    auto moveRegion = [this](glm::vec3 d) {
        m_camera.cx += d.x * m_camera.scale;
        m_camera.cy -= d.z * m_camera.scale;
    };

    const bool left = m_window.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
    const bool right = m_window.mouseDown(GLFW_MOUSE_BUTTON_RIGHT);
    const bool shift = m_window.keyDown(GLFW_KEY_LEFT_SHIFT) || m_window.keyDown(GLFW_KEY_RIGHT_SHIFT);
    if (mouseFree) {
        if (scroll != 0.0) m_orbit.dolly(float(std::pow(0.9, scroll)));
        const float dx = float(mx - m_lastCursor[0]);
        const float dy = float(my - m_lastCursor[1]);
        if (m_dragging && (right || (left && shift))) {
            // Drag the terrain under the cursor.
            const float k = 2.0f * m_orbit.distance * std::tan(0.5f * m_orbit.fovY) / float(h);
            moveRegion(-(m_orbit.groundRight() * dx - m_orbit.groundForward() * dy) * k);
        } else if (m_dragging && left) {
            m_orbit.rotate(-dx * 0.005f, dy * 0.005f);
        }
        m_dragging = left || right;
    } else {
        m_dragging = false;
    }

    if (keysFree) {
        const float step = 0.8f * dt;
        glm::vec3 move{0.0f};
        if (m_window.keyDown(GLFW_KEY_W) || m_window.keyDown(GLFW_KEY_UP)) move += m_orbit.groundForward();
        if (m_window.keyDown(GLFW_KEY_S) || m_window.keyDown(GLFW_KEY_DOWN)) move -= m_orbit.groundForward();
        if (m_window.keyDown(GLFW_KEY_D) || m_window.keyDown(GLFW_KEY_RIGHT)) move += m_orbit.groundRight();
        if (m_window.keyDown(GLFW_KEY_A) || m_window.keyDown(GLFW_KEY_LEFT)) move -= m_orbit.groundRight();
        moveRegion(move * step);

        const double w = m_ctx.width;
        if (m_window.keyDown(GLFW_KEY_E) || m_window.keyDown(GLFW_KEY_EQUAL))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(0.4, dt));
        if (m_window.keyDown(GLFW_KEY_Q) || m_window.keyDown(GLFW_KEY_MINUS))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(2.5, dt));
    }
}

void App::handleInputOrbit(OrbitCamera& cam, float dt, double mx, double my, double scroll, bool mouseFree,
                           bool keysFree) {
    const bool left = m_window.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
    if (mouseFree) {
        if (scroll != 0.0) cam.dolly(float(std::pow(0.9, scroll)));
        if (m_dragging && left)
            cam.rotate(-float(mx - m_lastCursor[0]) * 0.005f, float(my - m_lastCursor[1]) * 0.005f);
        m_dragging = left;
    } else {
        m_dragging = false;
    }

    if (keysFree) {
        // Fly the orbit target; speed scales with distance so close-ups stay controllable.
        const float step = 0.6f * dt * cam.distance;
        glm::vec3 move{0.0f};
        if (m_window.keyDown(GLFW_KEY_W) || m_window.keyDown(GLFW_KEY_UP)) move += cam.groundForward();
        if (m_window.keyDown(GLFW_KEY_S) || m_window.keyDown(GLFW_KEY_DOWN)) move -= cam.groundForward();
        if (m_window.keyDown(GLFW_KEY_D) || m_window.keyDown(GLFW_KEY_RIGHT)) move += cam.groundRight();
        if (m_window.keyDown(GLFW_KEY_A) || m_window.keyDown(GLFW_KEY_LEFT)) move -= cam.groundRight();
        if (m_window.keyDown(GLFW_KEY_E)) move.y += 1.0f;
        if (m_window.keyDown(GLFW_KEY_Q)) move.y -= 1.0f;
        cam.target += move * step;
    }
}

void App::drawUi() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Fracbrot", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);

    int mode = int(m_mode);
    if (ImGui::Combo("mode (Tab)", &mode, kModeNames, int(ViewMode::Count))) m_mode = ViewMode(mode);
    ImGui::Separator();

    int iter = int(m_params.maxIter);
    if (ImGui::SliderInt("iterations", &iter, 16, 20000, "%d", ImGuiSliderFlags_Logarithmic))
        m_params.maxIter = uint32_t(std::max(iter, 1));

    ImGui::Text("center  %s", m_camera.cx.toString(18).c_str());
    ImGui::Text("        %s", m_camera.cy.toString(18).c_str());
    ImGui::Text("zoom    %.3e", 1.2 / m_camera.scale);
    if (m_mode == ViewMode::Plane2D) {
        const PerturbStatus& ps = m_mandel.status();
        if (ps.active) {
            ImGui::Text("deep zoom: ref %u iters%s, %d bits%s", ps.refLen, ps.refEscaped ? " (escaped)" : "",
                        ps.precisionBits, ps.computing ? ", updating..." : "");
        }
        bool force = m_mandel.forcePerturbation();
        if (ImGui::Checkbox("force perturbation", &force)) m_mandel.setForcePerturbation(force);
        ImGui::SameLine();
        if (ImGui::Button("copy location")) ImGui::SetClipboardText(m_camera.toString().c_str());
    } else if (m_camera.scale < 1e-5) {
        ImGui::TextColored({1, 0.6f, 0.2f, 1}, "float precision limit (deep zoom is 2D only)");
    }
    if (ImGui::Button("reset (R)")) resetCameras();

    if (m_mode == ViewMode::Heightfield) {
        ImGui::SeparatorText("terrain");
        ImGui::SliderFloat("height", &m_scene.heightScale, 0.0f, 1.5f);
        ImGui::SliderFloat("height curve", &m_scene.heightCurve, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("interior height", &m_scene.interiorHeight, 0.0f, 1.0f);
        static const uint32_t kGrids[] = {256, 512, 1024, 2048};
        static const char* const kGridNames[] = {"256", "512", "1024", "2048"};
        int grid = int(std::find(std::begin(kGrids), std::end(kGrids), m_scene.gridSize) - std::begin(kGrids));
        if (ImGui::Combo("grid", &grid, kGridNames, int(std::size(kGrids)))) m_scene.gridSize = kGrids[grid];
        ImGui::SliderAngle("light azimuth", &m_lightAzimuth, -180.0f, 180.0f);
        ImGui::SliderAngle("light elevation", &m_lightElevation, 5.0f, 90.0f);
    }

    if (m_mode == ViewMode::Fractal3D) {
        ImGui::SeparatorText("3D fractal");
        int formula = int(m_march.formula);
        if (ImGui::Combo("formula", &formula, kFormulaNames, int(Formula3D::Count))) {
            applyFormulaDefaults(m_march, Formula3D(formula));
            m_marchOrbit.target = m_march.boundCenter;
        }
        int iters = int(m_march.iterations);
        if (ImGui::SliderInt("3D iterations", &iters, 2, 512, "%d", ImGuiSliderFlags_Logarithmic)) m_march.iterations = uint32_t(iters);
        if (m_march.formula == Formula3D::Mandelbulb) {
            ImGui::SliderFloat("power", &m_march.power, 1.0f, 16.0f);
        } else {
            ImGui::SliderFloat("slice w", &m_march.sliceW, -1.5f, 1.5f);
            ImGui::SliderAngle("4D tilt", &m_march.tilt, -90.0f, 90.0f);
        }
        if (m_march.formula == Formula3D::QuatJulia) ImGui::SliderFloat4("julia c", &m_march.juliaC.x, -1.5f, 1.5f);
        ImGui::Checkbox("animate", &m_animate);

        ImGui::SliderFloat("detail", &m_march.detail, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        int steps = int(m_march.maxSteps);
        if (ImGui::SliderInt("max steps", &steps, 32, 1000)) m_march.maxSteps = uint32_t(steps);
        ImGui::SliderFloat("render scale", &m_renderScale, 0.25f, 2.0f);
        ImGui::SliderFloat("trap color", &m_march.trapMix, 0.0f, 1.0f);
        ImGui::CheckboxFlags("shadows", &m_march.flags, kMarchShadows);
        ImGui::SameLine();
        ImGui::CheckboxFlags("AO", &m_march.flags, kMarchAO);
        ImGui::SameLine();
        ImGui::CheckboxFlags("reflections", &m_march.flags, kMarchReflect);
        ImGui::SliderAngle("light azimuth", &m_lightAzimuth, -180.0f, 180.0f);
        ImGui::SliderAngle("light elevation", &m_lightElevation, -90.0f, 90.0f);
    }

    if (m_mode == ViewMode::Plane2D) {
        ImGui::SeparatorText("orbit");
        ImGui::Checkbox("trace orbit under cursor (O)", &m_showOrbit);
        if (m_showOrbit) {
            ImGui::Text("c = %.6f %+.6fi", m_orbitC[0], m_orbitC[1]);
            if (m_orbitEscape >= 0)
                ImGui::Text("escapes after %d iterations", m_orbitEscape);
            else
                ImGui::Text("bounded (in the set)");
        }
    }

    if (m_mode == ViewMode::LogisticMap) {
        ImGui::SeparatorText("logistic map");
        ImGui::TextDisabled("x' = r x (1 - x)  <->  z' = z^2 + c\nc = r(2 - r)/4, z = r(1/2 - x)");
        ImGui::Checkbox("highlight r", &m_highlightR);
        if (m_highlightR) {
            ImGui::SliderFloat("r", &m_logisticR, 1.0f, 4.0f, "%.4f");
            ImGui::Text("c = %.6f", logisticToC(m_logisticR));
        }
        static const uint32_t kColumns[] = {512, 1024, 2048, 4096};
        static const char* const kColumnNames[] = {"512", "1024", "2048", "4096"};
        int col = int(std::find(std::begin(kColumns), std::end(kColumns), m_bif.columns) - std::begin(kColumns));
        if (ImGui::Combo("columns", &col, kColumnNames, int(std::size(kColumns)))) m_bif.columns = kColumns[col];
        int samples = int(m_bif.samples);
        if (ImGui::SliderInt("samples", &samples, 16, 1024)) m_bif.samples = uint32_t(samples);
        int warmup = int(m_bif.warmup);
        if (ImGui::SliderInt("warmup", &warmup, 0, 5000)) m_bif.warmup = uint32_t(warmup);
        ImGui::SliderFloat("point size", &m_bif.pointSize, 0.5f, 6.0f);
        ImGui::SliderFloat("brightness", &m_bif.brightness, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("vertical scale", &m_bif.yScale, 0.1f, 2.0f);
        ImGui::SliderFloat("plane brightness", &m_bif.planeDim, 0.0f, 1.0f);
        ImGui::Checkbox("auto-rotate", &m_autoRotate);
    }

    ImGui::SeparatorText("color");
    const auto presets = palettePresets();
    if (ImGui::BeginCombo("palette", presets[size_t(m_paletteIndex)].name)) {
        for (int i = 0; i < int(presets.size()); ++i) {
            if (ImGui::Selectable(presets[size_t(i)].name, i == m_paletteIndex)) setPalette(i);
        }
        ImGui::EndCombo();
    }
    bool smooth = m_params.flags & kFlagSmooth;
    if (ImGui::Checkbox("smooth", &smooth)) m_params.flags ^= kFlagSmooth;
    ImGui::SliderFloat("density", &m_params.colorDensity, 0.05f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("offset", &m_params.colorOffset, 0.0f, 1.0f);
    ImGui::SliderFloat("cycle speed", &m_colorCycleSpeed, -1.0f, 1.0f);

    ImGui::SeparatorText("export");
    if (ImGui::Button("screenshot (P)")) m_screenshotRequested = true;
    ImGui::SameLine();
    ImGui::Checkbox("include UI", &m_screenshotWithUi);
    if (!m_lastScreenshot.empty()) ImGui::TextDisabled("saved %s", m_lastScreenshot.c_str());

    ImGui::Separator();
    if (m_mode == ViewMode::Plane2D)
        ImGui::TextDisabled("drag: pan  scroll: zoom\nWASD/arrows: pan  Q/E: zoom");
    else if (m_mode == ViewMode::Heightfield)
        ImGui::TextDisabled("drag: orbit  scroll: distance\nright/shift-drag, WASD: move region\nQ/E: zoom region");
    else
        ImGui::TextDisabled("drag: orbit  scroll: distance\nWASD: move target  Q/E: down/up");
    ImGui::TextDisabled("1-4 / Tab: mode  P: screenshot");
    ImGui::End();
}

void App::setPalette(int index) {
    const auto presets = palettePresets();
    m_paletteIndex = std::clamp(index, 0, int(presets.size()) - 1);
    m_palette.upload(m_ctx.queue, presets[size_t(m_paletteIndex)]);
}

void App::updateScene(const OrbitCamera& orbit) {
    const float aspect = float(m_ctx.width) / float(std::max(m_ctx.height, 1u));
    m_scene.viewProj = orbit.projection(aspect) * orbit.view();
    m_scene.invViewProj = glm::inverse(m_scene.viewProj);
    m_scene.camPos = glm::vec4(orbit.position(), 1.0f);
    const float ce = std::cos(m_lightElevation);
    m_scene.lightDir = glm::vec4(ce * std::sin(m_lightAzimuth), std::sin(m_lightElevation),
                                 ce * std::cos(m_lightAzimuth), 0.0f);
}

std::string App::nextScreenshotPath() const {
    char buf[64];
    const std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "fracbrot_%Y%m%d_%H%M%S.png", std::localtime(&t));
    return buf;
}

bool App::frame() {
    m_window.pollEvents();
    if (m_window.shouldClose()) return false;
    const bool lastFrame = m_opts.maxFrames >= 0 && m_frame + 1 >= m_opts.maxFrames;

    const double now = glfwGetTime();
    const float dt = float(std::min(now - m_lastTime, 0.1));
    m_lastTime = now;

    int w, h;
    m_window.framebufferSize(w, h);
    m_ctx.resize(uint32_t(w), uint32_t(h));

    handleInput(dt);

    m_gui.beginFrame();
    if (!m_opts.hideUi) drawUi();

    Context::Frame target = m_ctx.acquire();
    if (!target) {
        ImGui::EndFrame();
        return true;
    }

    m_params.center[0] = float(m_camera.cx.toDouble());
    m_params.center[1] = float(m_camera.cy.toDouble());
    m_params.scale = float(m_camera.scale);
    m_params.resolution[0] = float(m_ctx.width);
    m_params.resolution[1] = float(m_ctx.height);
    m_params.time = float(now);
    m_params.colorOffset = std::fmod(m_params.colorOffset + m_colorCycleSpeed * dt + 1.0f, 1.0f);
    animate(now);

    wgpu::CommandEncoder encoder = m_ctx.device.CreateCommandEncoder();

    // Scene pass.
    wgpu::RenderPassColorAttachment color;
    color.view = target.view;
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;
    color.clearValue = {0.04, 0.045, 0.07, 1.0};
    wgpu::RenderPassDescriptor sceneDesc;
    sceneDesc.colorAttachmentCount = 1;
    sceneDesc.colorAttachments = &color;

    wgpu::RenderPassDepthStencilAttachment depth;
    depth.view = m_ctx.depthView;
    depth.depthLoadOp = wgpu::LoadOp::Clear;
    depth.depthStoreOp = wgpu::StoreOp::Discard;
    depth.depthClearValue = 1.0f;

    if (m_mode == ViewMode::Plane2D) {
        m_mandel.update(m_ctx, m_params, m_camera);
        if (m_showOrbit) {
            if (m_opts.traceC) {
                m_orbitC[0] = m_opts.traceC->first;
                m_orbitC[1] = m_opts.traceC->second;
            } else {
                double mx, my;
                m_window.cursorPos(mx, my);
                const double k = 2.0 * m_camera.scale / double(m_ctx.height);
                m_orbitC[0] = m_camera.cx.toDouble() + (mx - 0.5 * m_ctx.width) * k;
                m_orbitC[1] = m_camera.cy.toDouble() - (my - 0.5 * m_ctx.height) * k;
            }
            m_orbitEscape = m_orbitTracer.trace(m_ctx, m_camera, m_orbitC[0], m_orbitC[1], m_params.maxIter);
        }
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&sceneDesc);
        m_mandel.draw(pass);
        if (m_showOrbit) m_orbitTracer.draw(pass);
        pass.End();
    } else if (m_mode == ViewMode::LogisticMap) {
        if (m_autoRotate) m_logisticOrbit.yaw += 0.15f * dt;
        m_bif.highlightC = m_highlightR ? float(logisticToC(m_logisticR)) : 1e9f;
        updateScene(m_logisticOrbit);
        m_bifurcation.update(m_ctx, m_params, m_scene, m_bif);
        m_bifurcation.compute(encoder);
        sceneDesc.depthStencilAttachment = &depth;
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&sceneDesc);
        m_bifurcation.draw(pass);
        pass.End();
    } else if (m_mode == ViewMode::Fractal3D) {
        updateScene(m_marchOrbit);
        m_raymarcher.update(m_ctx, m_params, m_scene, m_march, m_marchOrbit.fovY, m_renderScale);
        m_raymarcher.render(encoder, target.view);
    } else {
        updateScene(m_orbit);
        m_terrain.update(m_ctx, m_params, m_scene);
        m_terrain.compute(encoder);
        sceneDesc.depthStencilAttachment = &depth;
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&sceneDesc);
        m_terrain.draw(pass);
        pass.End();
    }

    // UI pass on top.
    const bool capture = m_screenshotRequested || (lastFrame && !m_opts.screenshot.empty());
    color.loadOp = wgpu::LoadOp::Load;
    wgpu::RenderPassDescriptor uiDesc;
    uiDesc.colorAttachmentCount = 1;
    uiDesc.colorAttachments = &color;
    wgpu::RenderPassEncoder uiPass = encoder.BeginRenderPass(&uiDesc);
    m_gui.render(uiPass, !m_screenshotRequested || m_screenshotWithUi);
    uiPass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    m_ctx.queue.Submit(1, &commands);

    if (capture) {
        const std::string path = m_screenshotRequested ? nextScreenshotPath() : m_opts.screenshot;
        if (saveTexturePng(m_ctx, target.texture, path)) m_lastScreenshot = path;
        m_screenshotRequested = false;
    }

    m_ctx.present();
    ++m_frame;
    return !lastFrame;
}

void App::shutdown() {
    m_gui.shutdown();
    m_orbitTracer = {};
    m_bifurcation = {};
    m_raymarcher = {};
    m_terrain = {};
    m_mandel = {};
    m_ctx = {};
    m_window.destroy();
}

} // namespace fb
