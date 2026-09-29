#include "app/Gui.h"

#include "gfx/Context.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

namespace fb {

void Gui::init(GLFWwindow* window, const Context& ctx) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_InitInfo info;
    info.Device = ctx.device.Get();
    info.RenderTargetFormat = static_cast<WGPUTextureFormat>(ctx.format);
    ImGui_ImplWGPU_Init(&info);
    m_initialized = true;
}

void Gui::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void Gui::beginFrame() {
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::render(const wgpu::RenderPassEncoder& pass, bool draw) {
    ImGui::Render();
    if (draw) ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());
}

bool Gui::wantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool Gui::wantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

} // namespace fb
