#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace fb {

class Context;

// Thin wrapper over Dear ImGui's GLFW + WebGPU backends.
class Gui {
public:
    void init(GLFWwindow* window, const Context& ctx);
    void shutdown();
    void beginFrame();
    void render(const wgpu::RenderPassEncoder& pass);

    bool wantsMouse() const;
    bool wantsKeyboard() const;

private:
    bool m_initialized = false;
};

} // namespace fb
