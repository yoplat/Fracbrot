#include "app/App.h"

#include "gfx/Readback.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace fb {

namespace {

const char* const kModeNames[] = {"2D plane", "Heightfield"};
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
    m_terrain.init(m_ctx, m_palette);

    if (opts.camera) m_camera = *opts.camera;
    if (opts.orbit) m_orbit = *opts.orbit;
    m_mode = opts.mode;
    m_params.maxIter = opts.maxIter;
    m_lastTime = glfwGetTime();
    return true;
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
        if (keyPressed(GLFW_KEY_TAB)) m_mode = ViewMode((int(m_mode) + 1) % int(ViewMode::Count));
        if (keyPressed(GLFW_KEY_P)) m_screenshotRequested = true;
        if (m_window.keyDown(GLFW_KEY_R)) {
            m_camera.reset();
            m_orbit.reset();
        }
    }

    if (m_mode == ViewMode::Plane2D)
        handleInput2D(dt, mx, my, scroll, mouseFree, keysFree);
    else
        handleInput3D(dt, mx, my, scroll, mouseFree, keysFree);

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

    ImGui::Text("center  %.15f", m_camera.cx);
    ImGui::Text("        %.15f", m_camera.cy);
    ImGui::Text("zoom    %.3e", 1.2 / m_camera.scale);
    if (m_camera.scale < 1e-5) ImGui::TextColored({1, 0.6f, 0.2f, 1}, "float precision limit reached");
    if (ImGui::Button("reset (R)")) {
        m_camera.reset();
        m_orbit.reset();
    }

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
    else
        ImGui::TextDisabled("drag: orbit  scroll: distance\nright/shift-drag, WASD: move region\nQ/E: zoom region");
    ImGui::End();
}

void App::setPalette(int index) {
    const auto presets = palettePresets();
    m_paletteIndex = std::clamp(index, 0, int(presets.size()) - 1);
    m_palette.upload(m_ctx.queue, presets[size_t(m_paletteIndex)]);
}

void App::updateScene() {
    const float aspect = float(m_ctx.width) / float(std::max(m_ctx.height, 1u));
    m_scene.viewProj = m_orbit.projection(aspect) * m_orbit.view();
    m_scene.invViewProj = glm::inverse(m_scene.viewProj);
    m_scene.camPos = glm::vec4(m_orbit.position(), 1.0f);
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

    m_params.center[0] = float(m_camera.cx);
    m_params.center[1] = float(m_camera.cy);
    m_params.scale = float(m_camera.scale);
    m_params.resolution[0] = float(m_ctx.width);
    m_params.resolution[1] = float(m_ctx.height);
    m_params.time = float(now);
    m_params.colorOffset = std::fmod(m_params.colorOffset + m_colorCycleSpeed * dt + 1.0f, 1.0f);

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

    if (m_mode == ViewMode::Plane2D) {
        m_mandel.update(m_ctx.queue, m_params);
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&sceneDesc);
        m_mandel.draw(pass);
        pass.End();
    } else {
        updateScene();
        m_terrain.update(m_ctx, m_params, m_scene);
        m_terrain.compute(encoder);

        wgpu::RenderPassDepthStencilAttachment depth;
        depth.view = m_ctx.depthView;
        depth.depthLoadOp = wgpu::LoadOp::Clear;
        depth.depthStoreOp = wgpu::StoreOp::Discard;
        depth.depthClearValue = 1.0f;
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
    m_terrain = {};
    m_mandel = {};
    m_ctx = {};
    m_window.destroy();
}

} // namespace fb
