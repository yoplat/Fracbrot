#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace fb {

// Creates a WebGPU surface for the window using the platform's native handle
// (CAMetalLayer on macOS, X11/Wayland on Linux, HWND on Windows, canvas on web).
wgpu::Surface createSurface(const wgpu::Instance& instance, GLFWwindow* window);

} // namespace fb
