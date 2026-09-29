#include "app/App.h"

namespace fb {

bool App::init(const AppOptions& opts) {
    m_opts = opts;
    if (!m_window.create(opts.width, opts.height, "Fracbrot")) return false;

    int w, h;
    m_window.framebufferSize(w, h);
    return m_ctx.init(m_window.handle(), uint32_t(w), uint32_t(h));
}

bool App::frame() {
    m_window.pollEvents();
    if (m_window.shouldClose()) return false;
    if (m_opts.maxFrames >= 0 && m_frame >= m_opts.maxFrames) return false;

    int w, h;
    m_window.framebufferSize(w, h);
    m_ctx.resize(uint32_t(w), uint32_t(h));

    wgpu::TextureView target = m_ctx.acquire();
    if (!target) return true;

    wgpu::RenderPassColorAttachment color;
    color.view = target;
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;
    color.clearValue = {0.05, 0.05, 0.10, 1.0};

    wgpu::RenderPassDescriptor pass;
    pass.colorAttachmentCount = 1;
    pass.colorAttachments = &color;

    wgpu::CommandEncoder encoder = m_ctx.device.CreateCommandEncoder();
    encoder.BeginRenderPass(&pass).End();
    wgpu::CommandBuffer commands = encoder.Finish();
    m_ctx.queue.Submit(1, &commands);

    m_ctx.present();
    ++m_frame;
    return true;
}

void App::shutdown() {
    m_ctx = {};
    m_window.destroy();
}

} // namespace fb
