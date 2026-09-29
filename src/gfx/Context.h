#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace fb {

// Owns the WebGPU instance/device/queue and the window surface.
class Context {
public:
    bool init(GLFWwindow* window, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);

    // Returns a view of the next swapchain texture, or null if the frame should be skipped.
    wgpu::TextureView acquire();
    void present();

    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    uint32_t width = 0;
    uint32_t height = 0;

private:
    void configure();
};

} // namespace fb
