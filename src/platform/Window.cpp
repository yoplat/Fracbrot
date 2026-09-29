#include "platform/Window.h"

#include <GLFW/glfw3.h>

#include <cstdio>

namespace fb {

bool Window::create(int width, int height, const std::string& title) {
    glfwSetErrorCallback([](int code, const char* msg) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, msg);
    });
    if (!glfwInit()) return false;

    // WebGPU owns the swapchain, GLFW must not create a GL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    return m_window != nullptr;
}

void Window::destroy() {
    if (m_window) glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::framebufferSize(int& w, int& h) const { glfwGetFramebufferSize(m_window, &w, &h); }

} // namespace fb
