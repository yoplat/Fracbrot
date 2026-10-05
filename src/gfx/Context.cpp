#include "gfx/Context.h"

#include "platform/Surface.h"

#include <cstdio>
#include <string_view>

namespace fb {

namespace {

std::string_view view(wgpu::StringView s) { return {s.data, s.length}; }

} // namespace

bool Context::init(GLFWwindow* window, uint32_t w, uint32_t h, bool headless) {
    m_headless = headless;
    static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor instanceDesc;
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = &kTimedWaitAny;
    instance = wgpu::CreateInstance(&instanceDesc);
    if (!instance) {
        std::fprintf(stderr, "failed to create WebGPU instance\n");
        return false;
    }

    if (!headless) surface = createSurface(instance, window);

    wgpu::RequestAdapterOptions adapterOpts;
    adapterOpts.compatibleSurface = surface;
    adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;
    instance.WaitAny(
        instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::WaitAnyOnly,
            [this](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView msg) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    std::fprintf(stderr, "adapter request failed: %.*s\n", int(msg.length), msg.data);
                    return;
                }
                adapter = std::move(a);
            }),
        UINT64_MAX);
    if (!adapter) return false;

    wgpu::AdapterInfo info;
    adapter.GetInfo(&info);
    adapterName = std::string(view(info.device));
    std::printf("adapter: %.*s (%s)\n", int(view(info.device).size()), view(info.device).data(),
                info.backendType == wgpu::BackendType::Metal    ? "Metal"
                : info.backendType == wgpu::BackendType::Vulkan ? "Vulkan"
                : info.backendType == wgpu::BackendType::D3D12  ? "D3D12"
                : info.backendType == wgpu::BackendType::WebGPU ? "WebGPU"
                                                                : "other");

    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg) {
            ++errorCount;
            std::fprintf(stderr, "webgpu error (%d): %.*s\n", int(type), int(msg.length), msg.data);
        });
    deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
        [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView msg) {
            if (reason == wgpu::DeviceLostReason::Destroyed) return;
            ++errorCount;
            std::fprintf(stderr, "device lost (%d): %.*s\n", int(reason), int(msg.length), msg.data);
        });
    instance.WaitAny(
        adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::WaitAnyOnly,
            [this](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView msg) {
                if (status != wgpu::RequestDeviceStatus::Success) {
                    std::fprintf(stderr, "device request failed: %.*s\n", int(msg.length), msg.data);
                    return;
                }
                device = std::move(d);
            }),
        UINT64_MAX);
    if (!device) return false;
    queue = device.GetQueue();

    if (headless) {
        format = wgpu::TextureFormat::BGRA8Unorm; // what the Metal surface uses
    } else {
        wgpu::SurfaceCapabilities caps;
        surface.GetCapabilities(adapter, &caps);
        format = caps.formats[0];
    }

    width = w;
    height = h;
    configure();
    return true;
}

void Context::configure() {
    if (m_headless) {
        wgpu::TextureDescriptor desc;
        desc.label = "offscreen target";
        desc.size = {width, height, 1};
        desc.format = format;
        desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        m_offscreen = device.CreateTexture(&desc);
    } else {
        wgpu::SurfaceConfiguration config;
        config.device = device;
        config.format = format;
        config.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        config.width = width;
        config.height = height;
        config.presentMode = wgpu::PresentMode::Fifo;
        surface.Configure(&config);
    }

    wgpu::TextureDescriptor depthDesc;
    depthDesc.label = "depth";
    depthDesc.size = {width, height, 1};
    depthDesc.format = kDepthFormat;
    depthDesc.usage = wgpu::TextureUsage::RenderAttachment;
    depthView = device.CreateTexture(&depthDesc).CreateView();
}

void Context::resize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0 || (w == width && h == height)) return;
    width = w;
    height = h;
    configure();
}

Context::Frame Context::acquire() {
    if (m_headless) return {m_offscreen, m_offscreen.CreateView()};
    wgpu::SurfaceTexture st;
    surface.GetCurrentTexture(&st);
    switch (st.status) {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
        return {st.texture, st.texture.CreateView()};
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
        configure();
        return {};
    default:
        return {};
    }
}

void Context::present() {
#ifndef __EMSCRIPTEN__
    if (!m_headless) surface.Present();
    instance.ProcessEvents();
#endif
}

void Context::waitIdle() {
    instance.WaitAny(queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,
                                               [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {}),
                     UINT64_MAX);
}

} // namespace fb
