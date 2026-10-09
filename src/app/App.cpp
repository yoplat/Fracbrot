#include "app/App.h"

#include "fractal/DistanceEstimator.h"
#include "gfx/Readback.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace fb {

namespace {

// How long the 3D view must be unchanged before rendering at full quality again.
constexpr double kSettleSeconds = 0.15;

const char* const kModeNames[] = {"2D plane", "3D fractal", "Logistic map"};
const char* const kFormulaNames[] = {"Quaternion Mandelbrot", "Quaternion Julia", "Mandelbulb",
                                      "Bicomplex Mandelbrot (Tetrabrot)", "Hypercomplex Mandelbrot (video)",
                                      "Quaternion + Syntopia flip"};
static_assert(std::size(kFormulaNames) == size_t(Formula3D::Count));

OrbitCamera marchHome(const MarchParams& march) {
    OrbitCamera cam;
    cam.target = march.boundCenter;
    cam.yaw = 0.45f;
    cam.pitch = 0.3f;
    cam.distance = 3.2f;
    if (march.formula == Formula3D::Hypercomplex || march.formula == Formula3D::QuatFlip) {
        // Side-on along the needle, like the video.
        cam.target = {-0.9f, 0.0f, 0.0f};
        cam.yaw = 0.15f;
        cam.pitch = 0.06f;
        cam.distance = 2.6f;
    } else if (march.formula == Formula3D::QuatMandelbrot) {
        // Side-on to the real axis, like Ice Fractal's default view.
        cam.target = {-0.7f, 0.0f, 0.0f};
        cam.yaw = 0.12f;
        cam.pitch = 0.08f;
        cam.distance = 3.6f;
    }
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
    const bool headless = opts.headless || opts.bench;
    if (!m_window.create(opts.width, opts.height, "Fracbrot", !headless)) return false;

    int w, h;
    m_window.framebufferSize(w, h);
    if (!m_ctx.init(m_window.handle(), uint32_t(w), uint32_t(h), headless)) return false;

    m_gui.init(m_window.handle(), m_ctx);
    m_palette.init(m_ctx.device);
    setPalette(opts.palette);
    m_mandel.init(m_ctx, m_palette);
    m_mandel.setBlocking(opts.maxFrames >= 0); // automated runs need finished reference orbits
    m_mandel.setForcePerturbation(opts.perturb);
    m_raymarcher.init(m_ctx, m_palette);
    m_bifurcation.init(m_ctx, m_palette);
    m_orbitTracer.init(m_ctx);

    applyFormulaDefaults(m_march, Formula3D(std::clamp(opts.formula, 0, int(Formula3D::Count) - 1)));
    if (opts.iter3d) m_march.iterations = opts.iter3d;
    if (opts.detail > 0.0f) m_march.detail = opts.detail;
    if (opts.power > 0.0f) m_march.power = opts.power;
    if (opts.rotation) m_march.rotation = glm::vec4(glm::radians(*opts.rotation), 0.0f);
    if (opts.renderScale > 0.0f) m_renderScale = opts.renderScale;
    resetCameras();
    if (opts.camera) m_camera = *opts.camera;
    m_mode = opts.mode;
    m_showOrbit = opts.traceC.has_value();
    if (opts.orbit && m_mode != ViewMode::Plane2D) {
        OrbitCamera& cam = activeOrbit();
        cam.yaw = opts.orbit->yaw;
        cam.pitch = opts.orbit->pitch;
        cam.distance = opts.orbit->distance;
    }
    if (opts.target && m_mode != ViewMode::Plane2D) activeOrbit().target = *opts.target;
    m_params.maxIter = opts.maxIter;
    m_lastTime = glfwGetTime();

    if (opts.bench) {
        m_opts.hideUi = true;
        m_mandel.setBlocking(true);
        m_bench.emplace(opts.benchOut);
        applyBenchScene(benchScenes().front());
    }
    return true;
}

void App::applyBenchScene(const BenchScene& scene) {
    m_mode = ViewMode(scene.mode);
    m_params = {};
    m_params.maxIter = scene.iter;
    m_animate = false;
    m_autoRotate = false;
    m_showOrbit = false;
    m_renderScale = 0.75f;
    applyFormulaDefaults(m_march, Formula3D(scene.formula));
    resetCameras();
    if (scene.view) m_camera = Camera2D::parse(scene.view).value_or(Camera2D{});
    OrbitCamera& cam = activeOrbit();
    if (scene.orbit) {
        cam.yaw = scene.orbit->x;
        cam.pitch = scene.orbit->y;
        cam.distance = scene.orbit->z;
    }
    if (scene.target) cam.target = *scene.target;
}

void App::resetCameras() {
    m_camera.reset();
    m_marchOrbit = marchHome(m_march);
    m_logisticOrbit = logisticHome();
}

OrbitCamera& App::activeOrbit() { return m_mode == ViewMode::LogisticMap ? m_logisticOrbit : m_marchOrbit; }

void App::animate(double time) {
    if (!m_animate) return;
    const float t = float(time);
    switch (m_march.formula) {
    case Formula3D::QuatMandelbrot:
        m_march.rotation.y = glm::radians(27.0f + 25.0f * std::sin(t * 0.2f));
        break;
    case Formula3D::QuatJulia:
        m_march.juliaC = glm::vec4(-0.291f + 0.15f * std::cos(t * 0.21f), -0.399f + 0.2f * std::sin(t * 0.17f),
                                   0.339f + 0.15f * std::cos(t * 0.13f), 0.437f + 0.15f * std::sin(t * 0.11f));
        break;
    case Formula3D::Mandelbulb:
        m_march.power = 8.0f + 4.0f * std::sin(t * 0.2f);
        break;
    case Formula3D::Bicomplex:
    case Formula3D::Hypercomplex:
    case Formula3D::QuatFlip:
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
        if (keyPressed(GLFW_KEY_2)) m_mode = ViewMode::Fractal3D;
        if (keyPressed(GLFW_KEY_3)) m_mode = ViewMode::LogisticMap;
        if (keyPressed(GLFW_KEY_O)) m_showOrbit = !m_showOrbit;
        if (keyPressed(GLFW_KEY_TAB)) m_mode = ViewMode((int(m_mode) + 1) % int(ViewMode::Count));
        if (keyPressed(GLFW_KEY_P)) m_screenshotRequested = true;
        if (m_window.keyDown(GLFW_KEY_R)) resetCameras();
    }

    if (m_mode == ViewMode::Plane2D)
        handleInput2D(dt, mx, my, scroll, mouseFree, keysFree);
    else
        handleInputOrbit(activeOrbit(), dt, mx, my, scroll, mouseFree, keysFree);

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
        if (m_window.keyDown(GLFW_KEY_Z) || m_window.keyDown(GLFW_KEY_E) || m_window.keyDown(GLFW_KEY_EQUAL))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(0.25, dt));
        if (m_window.keyDown(GLFW_KEY_X) || m_window.keyDown(GLFW_KEY_Q) || m_window.keyDown(GLFW_KEY_MINUS))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(4.0, dt));
    }
}

void App::handleInputOrbit(OrbitCamera& cam, float dt, double mx, double my, double scroll, bool mouseFree,
                           bool keysFree) {
    // Re-pivot between drags; keeping the pivot fixed during a drag keeps rotation stable.
    const bool fractal = m_mode == ViewMode::Fractal3D;
    const float pixelAngle = 2.0f * std::tan(0.5f * cam.fovY) / float(std::max(m_ctx.height, 1u));
    if (fractal && !m_dragging) pivotOnSurface(cam, m_march, pixelAngle);
    auto zoom = [&](float factor) {
        if (fractal)
            zoomTowardSurface(cam, m_march, pixelAngle, factor);
        else
            cam.dolly(factor);
    };

    const bool left = m_window.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
    if (mouseFree) {
        if (scroll != 0.0) zoom(float(std::pow(0.9, scroll)));
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

        if (m_window.keyDown(GLFW_KEY_Z)) zoom(float(std::pow(0.3, dt)));
        if (m_window.keyDown(GLFW_KEY_X)) zoom(float(std::pow(1.0 / 0.3, dt)));
    }
}

void App::drawUi() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Fracbrot", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);

    int mode = int(m_mode);
    if (ImGui::Combo("mode (Tab)", &mode, kModeNames, int(ViewMode::Count))) m_mode = ViewMode(mode);
    ImGui::Separator();

    if (m_mode == ViewMode::Plane2D) {
        int iter = int(m_params.maxIter);
        if (ImGui::SliderInt("iterations", &iter, 16, 20000, "%d", ImGuiSliderFlags_Logarithmic))
            m_params.maxIter = uint32_t(std::max(iter, 1));
        ImGui::Text("center  %s", m_camera.cx.toString(18).c_str());
        ImGui::Text("        %s", m_camera.cy.toString(18).c_str());
        ImGui::Text("zoom    %.3e", 1.2 / m_camera.scale);
        const PerturbStatus& ps = m_mandel.status();
        if (ps.active) {
            ImGui::Text("deep zoom: ref %u iters%s, %d bits%s", ps.refLen, ps.refEscaped ? " (escaped)" : "",
                        ps.precisionBits, ps.computing ? ", updating..." : "");
        }
        bool force = m_mandel.forcePerturbation();
        if (ImGui::Checkbox("force perturbation", &force)) m_mandel.setForcePerturbation(force);
        ImGui::SameLine();
        if (ImGui::Button("copy location")) ImGui::SetClipboardText(m_camera.toString().c_str());
    }
    if (ImGui::Button("reset view (R)")) resetCameras();

    if (m_mode == ViewMode::Fractal3D) {
        ImGui::SeparatorText("3D fractal");
        int formula = int(m_march.formula);
        if (ImGui::Combo("formula", &formula, kFormulaNames, int(Formula3D::Count))) {
            applyFormulaDefaults(m_march, Formula3D(formula));
            m_marchOrbit = marchHome(m_march);
        }
        int iters = int(m_march.iterations);
        if (ImGui::SliderInt("iterations", &iters, 2, 512, "%d", ImGuiSliderFlags_Logarithmic))
            m_march.iterations = uint32_t(iters);
        if (m_march.formula == Formula3D::Mandelbulb) {
            ImGui::SliderFloat("power", &m_march.power, 1.0f, 16.0f);
        } else {
            ImGui::SliderFloat("slice w", &m_march.sliceW, -1.5f, 1.5f);
            ImGui::SliderAngle("4D tilt", &m_march.tilt, -90.0f, 90.0f);
        }
        if (m_march.formula == Formula3D::QuatJulia) ImGui::SliderFloat4("julia c", &m_march.juliaC.x, -1.5f, 1.5f);
        if (m_march.formula == Formula3D::QuatMandelbrot || m_march.formula == Formula3D::QuatJulia) {
            ImGui::TextDisabled("rotation applied to z every iteration");
            ImGui::SliderAngle("rotate XZ", &m_march.rotation.x, 0.0f, 360.0f);
            ImGui::SliderAngle("rotate YZ", &m_march.rotation.y, 0.0f, 360.0f);
            ImGui::SliderAngle("rotate XW", &m_march.rotation.z, 0.0f, 360.0f);
            ImGui::TextDisabled("rotation method: icefractal.com/mandelbrot/quat3d.php");
        }
        ImGui::Checkbox("animate", &m_animate);

        ImGui::SliderFloat("detail", &m_march.detail, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        int steps = int(m_march.maxSteps);
        if (ImGui::SliderInt("max steps", &steps, 32, 1000)) m_march.maxSteps = uint32_t(steps);
        ImGui::SliderFloat("render scale", &m_renderScale, 0.25f, 2.0f);
        ImGui::Checkbox("fast controls (half res while moving)", &m_fastControls);
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
        int iter = int(m_params.maxIter);
        if (ImGui::SliderInt("plane iterations", &iter, 16, 5000, "%d", ImGuiSliderFlags_Logarithmic))
            m_params.maxIter = uint32_t(std::max(iter, 1));
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
        ImGui::TextDisabled("drag: pan  scroll / Z X: zoom in/out\nWASD/arrows: pan");
    else
        ImGui::TextDisabled("drag: orbit  scroll / Z X: zoom in/out\nWASD: move target  Q/E: down/up");
    ImGui::TextDisabled("1-3 / Tab: mode  P: screenshot");
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
    // Latency is timed from here: acquiring can block on vsync, which isn't render cost.
    const auto frameStart = std::chrono::steady_clock::now();

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
    } else {
        updateScene(m_marchOrbit);
        // Fast controls: render at half resolution while the view is changing, then once at
        // full quality when it settles. Unchanged frames reuse the last image.
        if (m_raymarcher.changed(m_params, m_scene, m_march)) m_lastMarchChange = now;
        const bool automated = m_bench || m_opts.maxFrames >= 0; // scripted runs: always full quality
        const bool moving = m_fastControls && !automated && now - m_lastMarchChange < kSettleSeconds;
        const float scale = moving ? 0.5f * m_renderScale : m_renderScale;
        if (m_bench) m_raymarcher.invalidate();
        m_raymarcher.update(m_ctx, m_params, m_scene, m_march, m_marchOrbit.fovY, scale);
        m_raymarcher.render(encoder, target.view);
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

    if (m_bench) {
        using ms = std::chrono::duration<double, std::milli>;
        const double cpuMs = ms(std::chrono::steady_clock::now() - frameStart).count();
        m_ctx.waitIdle();
        const double totalMs = ms(std::chrono::steady_clock::now() - frameStart).count();
        if (m_bench->record(cpuMs, totalMs)) {
            if (m_bench->finished()) {
                m_ctx.present();
                m_bench->report(m_ctx.adapterName, m_ctx.width, m_ctx.height);
                return false;
            }
            applyBenchScene(benchScenes()[m_bench->sceneIndex()]);
        }
    }

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
    m_mandel = {};
    m_ctx = {};
    m_window.destroy();
}

} // namespace fb
