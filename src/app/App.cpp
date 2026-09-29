#include "app/App.h"

#include "gfx/Readback.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace fb {

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
    if (opts.camera) m_camera = *opts.camera;
    m_params.maxIter = opts.maxIter;
    m_lastTime = glfwGetTime();
    return true;
}

void App::handleInput(float dt) {
    const double w = m_ctx.width, h = m_ctx.height;

    double mx, my;
    m_window.cursorPos(mx, my);
    const double scroll = m_window.consumeScroll();

    if (!m_gui.wantsMouse()) {
        if (scroll != 0.0) m_camera.zoomAt(mx, my, w, h, std::pow(0.9, scroll));

        const bool down = m_window.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        if (down && m_dragging) m_camera.pan(mx - m_lastCursor[0], my - m_lastCursor[1], h);
        m_dragging = down;
    } else {
        m_dragging = false;
    }
    m_lastCursor[0] = mx;
    m_lastCursor[1] = my;

    if (!m_gui.wantsKeyboard()) {
        const double panPx = 600.0 * dt;
        if (m_window.keyDown(GLFW_KEY_LEFT) || m_window.keyDown(GLFW_KEY_A)) m_camera.pan(panPx, 0, h);
        if (m_window.keyDown(GLFW_KEY_RIGHT) || m_window.keyDown(GLFW_KEY_D)) m_camera.pan(-panPx, 0, h);
        if (m_window.keyDown(GLFW_KEY_UP) || m_window.keyDown(GLFW_KEY_W)) m_camera.pan(0, panPx, h);
        if (m_window.keyDown(GLFW_KEY_DOWN) || m_window.keyDown(GLFW_KEY_S)) m_camera.pan(0, -panPx, h);
        if (m_window.keyDown(GLFW_KEY_E) || m_window.keyDown(GLFW_KEY_EQUAL))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(0.25, dt));
        if (m_window.keyDown(GLFW_KEY_Q) || m_window.keyDown(GLFW_KEY_MINUS))
            m_camera.zoomAt(w / 2, h / 2, w, h, std::pow(4.0, dt));
        if (m_window.keyDown(GLFW_KEY_R)) m_camera.reset();

        const bool p = m_window.keyDown(GLFW_KEY_P);
        if (p && !m_screenshotKeyWasDown) m_screenshotRequested = true;
        m_screenshotKeyWasDown = p;
    }
}

void App::drawUi() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Fracbrot", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
    ImGui::Separator();

    int iter = int(m_params.maxIter);
    if (ImGui::SliderInt("iterations", &iter, 16, 20000, "%d", ImGuiSliderFlags_Logarithmic))
        m_params.maxIter = uint32_t(std::max(iter, 1));

    ImGui::Text("center  %.15f", m_camera.cx);
    ImGui::Text("        %.15f", m_camera.cy);
    ImGui::Text("zoom    %.3e", 1.2 / m_camera.scale);
    if (m_camera.scale < 1e-5) ImGui::TextColored({1, 0.6f, 0.2f, 1}, "float precision limit reached");
    if (ImGui::Button("reset (R)")) m_camera.reset();

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
    ImGui::TextDisabled("drag: pan  scroll: zoom\nWASD/arrows: pan  Q/E: zoom");
    ImGui::End();
}

void App::setPalette(int index) {
    const auto presets = palettePresets();
    m_paletteIndex = std::clamp(index, 0, int(presets.size()) - 1);
    m_palette.upload(m_ctx.queue, presets[size_t(m_paletteIndex)]);
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
    m_mandel.update(m_ctx.queue, m_params);

    wgpu::RenderPassColorAttachment color;
    color.view = target.view;
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;
    color.clearValue = {0.0, 0.0, 0.0, 1.0};

    wgpu::RenderPassDescriptor passDesc;
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &color;

    wgpu::CommandEncoder encoder = m_ctx.device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);
    m_mandel.draw(pass);
    const bool capture = m_screenshotRequested || (lastFrame && !m_opts.screenshot.empty());
    m_gui.render(pass, !m_screenshotRequested || m_screenshotWithUi);
    pass.End();
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
    m_mandel = {};
    m_ctx = {};
    m_window.destroy();
}

} // namespace fb
