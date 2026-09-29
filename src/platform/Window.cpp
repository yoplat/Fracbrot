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
    if (!m_window) return false;

    // Installed before ImGui so its GLFW backend chains to it.
    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double, double dy) {
        static_cast<Window*>(glfwGetWindowUserPointer(w))->m_scroll += dy;
    });
    return true;
}

void Window::destroy() {
    if (m_window) glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::framebufferSize(int& w, int& h) const { glfwGetFramebufferSize(m_window, &w, &h); }

void Window::cursorPos(double& x, double& y) const {
    int ww, wh, fw, fh;
    glfwGetWindowSize(m_window, &ww, &wh);
    glfwGetFramebufferSize(m_window, &fw, &fh);
    glfwGetCursorPos(m_window, &x, &y);
    if (ww > 0 && wh > 0) {
        x *= double(fw) / ww;
        y *= double(fh) / wh;
    }
}

bool Window::mouseDown(int button) const { return glfwGetMouseButton(m_window, button) == GLFW_PRESS; }

bool Window::keyDown(int key) const { return glfwGetKey(m_window, key) == GLFW_PRESS; }

double Window::consumeScroll() {
    double s = m_scroll;
    m_scroll = 0.0;
    return s;
}

} // namespace fb
