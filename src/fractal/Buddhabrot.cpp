#include "fractal/Buddhabrot.h"

#include "camera/Camera2D.h"
#include "fractal/Palette.h"
#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/buddhabrot_accum.wgsl.h"
#include "shaders/buddhabrot_common.wgsl.h"
#include "shaders/buddhabrot_display.wgsl.h"
#include "shaders/buddhabrot_reduce.wgsl.h"
#include "shaders/common.wgsl.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fb {

namespace {

constexpr uint32_t kAccumWorkgroup = 64;
constexpr uint32_t kReduceGroups = 256;

wgpu::ComputePipeline createCompute(const Context& ctx, const char* code, const char* label) {
    wgpu::ComputePipelineDescriptor desc;
    desc.label = label;
    desc.compute.module = createShader(ctx.device, {kCommonWgsl, kBuddhabrotCommonWgsl, code}, label);
    desc.compute.entryPoint = "main";
    return ctx.device.CreateComputePipeline(&desc);
}

// The parts of the parameters that determine what the histogram holds.
BuddhaParams histogramKey(BuddhaParams p) {
    p.frame = 0;
    p.samplesPerThread = 0;
    p.exposure = 0.0f;
    p.gamma = 0.0f;
    p.screenSize[0] = p.screenSize[1] = 0.0f;
    return p;
}

} // namespace

void Buddhabrot::init(const Context& ctx, const PaletteTexture& palette) {
    m_palette = &palette;
    m_uniforms = createUniformBuffer(ctx.device, sizeof(BuddhaParams), "buddhabrot params");

    wgpu::BufferDescriptor maxDesc;
    maxDesc.label = "buddhabrot max";
    maxDesc.size = 16;
    maxDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    m_max = ctx.device.CreateBuffer(&maxDesc);

    m_accumPipeline = createCompute(ctx, kBuddhabrotAccumWgsl, "buddhabrot accumulate");
    m_reducePipeline = createCompute(ctx, kBuddhabrotReduceWgsl, "buddhabrot reduce");

    wgpu::ShaderModule display =
        createShader(ctx.device, {kCommonWgsl, kBuddhabrotCommonWgsl, kBuddhabrotDisplayWgsl}, "buddhabrot display");
    wgpu::ColorTargetState target;
    target.format = ctx.format;
    wgpu::FragmentState fragment;
    fragment.module = display;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;
    wgpu::RenderPipelineDescriptor rDesc;
    rDesc.label = "buddhabrot display";
    rDesc.vertex.module = display;
    rDesc.vertex.entryPoint = "vs";
    rDesc.fragment = &fragment;
    m_displayPipeline = ctx.device.CreateRenderPipeline(&rDesc);
}

void Buddhabrot::resizeHistogram(const Context& ctx, uint32_t width, uint32_t height) {
    m_histBytes = uint64_t(width) * height * 3 * sizeof(uint32_t);
    wgpu::BufferDescriptor desc;
    desc.label = "buddhabrot histogram";
    desc.size = m_histBytes;
    desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    m_hist = ctx.device.CreateBuffer(&desc);

    wgpu::BindGroupEntry a[2];
    a[0].binding = 0;
    a[0].buffer = m_uniforms;
    a[1].binding = 1;
    a[1].buffer = m_hist;
    wgpu::BindGroupDescriptor aDesc;
    aDesc.layout = m_accumPipeline.GetBindGroupLayout(0);
    aDesc.entryCount = 2;
    aDesc.entries = a;
    m_accumBindGroup = ctx.device.CreateBindGroup(&aDesc);

    wgpu::BindGroupEntry r[3];
    r[0].binding = 0;
    r[0].buffer = m_uniforms;
    r[1].binding = 1;
    r[1].buffer = m_hist;
    r[2].binding = 2;
    r[2].buffer = m_max;
    wgpu::BindGroupDescriptor rDesc;
    rDesc.layout = m_reducePipeline.GetBindGroupLayout(0);
    rDesc.entryCount = 3;
    rDesc.entries = r;
    m_reduceBindGroup = ctx.device.CreateBindGroup(&rDesc);

    wgpu::BindGroupEntry d[5];
    d[0].binding = 0;
    d[0].buffer = m_uniforms;
    d[1].binding = 1;
    d[1].buffer = m_hist;
    d[2].binding = 2;
    d[2].buffer = m_max;
    d[3].binding = 3;
    d[3].textureView = m_palette->view;
    d[4].binding = 4;
    d[4].sampler = m_palette->sampler;
    wgpu::BindGroupDescriptor dDesc;
    dDesc.layout = m_displayPipeline.GetBindGroupLayout(0);
    dDesc.entryCount = 5;
    dDesc.entries = d;
    m_displayBindGroup = ctx.device.CreateBindGroup(&dDesc);
}

void Buddhabrot::update(const Context& ctx, const Camera2D& view, const BuddhaSettings& s) {
    BuddhaParams& p = m_params;
    p.center[0] = float(view.cx.toDouble());
    p.center[1] = float(view.cy.toDouble());
    p.scale = float(view.scale);
    p.rotate = s.rotate ? 1u : 0u;
    p.histSize[0] = std::max(1u, uint32_t(float(ctx.width) * s.histScale));
    p.histSize[1] = std::max(1u, uint32_t(float(ctx.height) * s.histScale));
    p.screenSize[0] = float(ctx.width);
    p.screenSize[1] = float(ctx.height);
    p.channels = s.nebula ? 3u : 1u;
    p.limits[0] = std::max(1u, s.limits[0]);
    p.limits[1] = s.nebula ? std::max(1u, s.limits[1]) : 0u;
    p.limits[2] = s.nebula ? std::max(1u, s.limits[2]) : 0u;
    p.limits[3] = std::max({p.limits[0], p.limits[1], p.limits[2]});
    p.minIter = s.minIter;
    p.exposure = s.exposure;
    p.gamma = s.gamma;

    const uint64_t bytes = uint64_t(p.histSize[0]) * p.histSize[1] * 3 * sizeof(uint32_t);
    if (bytes != m_histBytes) {
        resizeHistogram(ctx, p.histSize[0], p.histSize[1]);
        m_resetPending = true;
    }
    const BuddhaParams key = histogramKey(p);
    if (std::memcmp(&key, &m_lastKey, sizeof(key)) != 0) {
        m_lastKey = key;
        m_resetPending = true;
    }

    m_accumulate = !s.paused;
    p.samplesPerThread = 4;
    m_workgroups = std::max(1u, s.samplesPerFrame / (kAccumWorkgroup * p.samplesPerThread));
    if (m_accumulate) ++p.frame;
    ctx.queue.WriteBuffer(m_uniforms, 0, &p, sizeof(p));
}

void Buddhabrot::compute(const wgpu::CommandEncoder& encoder) {
    if (m_resetPending) {
        encoder.ClearBuffer(m_hist, 0, m_histBytes);
        m_samples = 0;
        m_resetPending = false;
    }
    encoder.ClearBuffer(m_max, 0, 16);

    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    if (m_accumulate) {
        pass.SetPipeline(m_accumPipeline);
        pass.SetBindGroup(0, m_accumBindGroup);
        pass.DispatchWorkgroups(m_workgroups);
        m_samples += uint64_t(m_workgroups) * kAccumWorkgroup * m_params.samplesPerThread;
    }
    pass.SetPipeline(m_reducePipeline);
    pass.SetBindGroup(0, m_reduceBindGroup);
    pass.DispatchWorkgroups(kReduceGroups);
    pass.End();
}

void Buddhabrot::draw(const wgpu::RenderPassEncoder& pass) const {
    pass.SetPipeline(m_displayPipeline);
    pass.SetBindGroup(0, m_displayBindGroup);
    pass.Draw(3);
}

} // namespace fb
