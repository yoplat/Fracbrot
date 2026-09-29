#include "platform/Surface.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace fb {

wgpu::Surface createSurface(const wgpu::Instance& instance, GLFWwindow* window) {
    wgpu::SurfaceDescriptor desc;

#if defined(_WIN32)
    wgpu::SurfaceSourceWindowsHWND source;
    source.hwnd = glfwGetWin32Window(window);
    source.hinstance = GetModuleHandle(nullptr);
    desc.nextInChain = &source;
    return instance.CreateSurface(&desc);
#else
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        wgpu::SurfaceSourceWaylandSurface source;
        source.display = glfwGetWaylandDisplay();
        source.surface = glfwGetWaylandWindow(window);
        desc.nextInChain = &source;
        return instance.CreateSurface(&desc);
    }
    wgpu::SurfaceSourceXlibWindow source;
    source.display = glfwGetX11Display();
    source.window = glfwGetX11Window(window);
    desc.nextInChain = &source;
    return instance.CreateSurface(&desc);
#endif
}

} // namespace fb
