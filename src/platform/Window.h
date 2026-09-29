#pragma once

#include <string>

struct GLFWwindow;

namespace fb {

class Window {
public:
    bool create(int width, int height, const std::string& title);
    void destroy();

    bool shouldClose() const;
    void pollEvents();

    // Framebuffer size in pixels (differs from window size on HiDPI).
    void framebufferSize(int& w, int& h) const;
    // Cursor position in framebuffer pixels.
    void cursorPos(double& x, double& y) const;
    bool mouseDown(int button) const;
    bool keyDown(int key) const;
    // Scroll accumulated since the last call.
    double consumeScroll();

    GLFWwindow* handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    double m_scroll = 0.0;
};

} // namespace fb
