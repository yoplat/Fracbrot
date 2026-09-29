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

    GLFWwindow* handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
};

} // namespace fb
