#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace fb {

// Owns the WebGPU instance/device/queue and the window surface.
class Context {
public:
    static constexpr wgpu::TextureFormat kDepthFormat = wgpu::TextureFormat::Depth24Plus;

    bool init(GLFWwindow* window, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);

    struct Frame {
        wgpu::Texture texture;
        wgpu::TextureView view;
        explicit operator bool() const { return bool(view); }
    };
    // Returns the next swapchain texture, or an empty frame if it should be skipped.
    Frame acquire();
    void present();

    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    wgpu::TextureView depthView; // matches the surface size
    uint32_t width = 0;
    // Number of WebGPU validation/device errors reported so far.
    static inline int errorCount = 0;

    uint32_t height = 0;

private:
    void configure();
};

} // namespace fb
