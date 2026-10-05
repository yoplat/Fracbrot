#include "fractal/Mandel2D.h"

#include "camera/Camera2D.h"
#include "fractal/Palette.h"
#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/common.wgsl.h"
#include "shaders/mandel2d.wgsl.h"
#include "shaders/perturb.wgsl.h"

#include <algorithm>
#include <cmath>

namespace fb {

namespace {

constexpr double kFrameBudgetMs = 8.0;

wgpu::RenderPipeline createPipeline(const Context& ctx, const char* code, const char* label) {
    wgpu::ShaderModule shader = createShader(ctx.device, {kCommonWgsl, code}, label);

    wgpu::ColorTargetState target;
    target.format = ctx.format;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;

    wgpu::RenderPipelineDescriptor desc;
    desc.label = label;
    desc.vertex.module = shader;
    desc.vertex.entryPoint = "vs";
    desc.fragment = &fragment;
    return ctx.device.CreateRenderPipeline(&desc);
}

} // namespace

void Mandel2D::init(const Context& ctx, const PaletteTexture& palette) {
    m_palette = &palette;
    m_pipeline = createPipeline(ctx, kMandel2dWgsl, "mandel2d");
    m_perturbPipeline = createPipeline(ctx, kPerturbWgsl, "mandel2d perturb");

    m_uniforms = createUniformBuffer(ctx.device, sizeof(FractalParams), "mandel2d params");
    m_perturbUniforms = createUniformBuffer(ctx.device, sizeof(PerturbParams), "perturb params");

    wgpu::BindGroupEntry entries[3];
    entries[0].binding = 0;
    entries[0].buffer = m_uniforms;
    entries[0].size = sizeof(FractalParams);
    entries[1].binding = 1;
    entries[1].textureView = palette.view;
    entries[2].binding = 2;
    entries[2].sampler = palette.sampler;

    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_pipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 3;
    bgDesc.entries = entries;
    m_bindGroup = ctx.device.CreateBindGroup(&bgDesc);
}

bool Mandel2D::suitable(const ReferenceOrbit& ref, const Camera2D& camera, uint32_t maxIter, double pixelSize,
                        double maxOffsetPx) const {
    if (!ref.started()) return false;
    if (ref.fracLimbs() < BigFixed::limbsFor(pixelSize)) return false;
    if (!ref.escaped() && ref.maxIter() < maxIter) return false;
    const double ox = (camera.cx - ref.cx()).toDouble() / pixelSize;
    const double oy = (camera.cy - ref.cy()).toDouble() / pixelSize;
    return std::fabs(ox) < maxOffsetPx && std::fabs(oy) < maxOffsetPx;
}

void Mandel2D::updateReference(const Context& ctx, const Camera2D& camera, uint32_t maxIter, double pixelSize) {
    // Any point works as reference (rebasing handles the rest), as long as the
    // per-pixel offsets stay small enough for f32.
    const double maxOffsetPx = 4.0 * double(std::max(ctx.width, ctx.height));
    const bool needNew = !suitable(m_active, camera, maxIter, pixelSize, maxOffsetPx);
    m_status.computing = false;
    if (!needNew) return;

    if (!suitable(m_pending, camera, maxIter, pixelSize, maxOffsetPx) || m_pending.complete()) {
        BigFixed cx = camera.cx, cy = camera.cy;
        const int limbs = std::max(BigFixed::limbsFor(pixelSize), cx.fracLimbs());
        cx.setFracLimbs(limbs);
        cy.setFracLimbs(limbs);
        m_pending.start(cx, cy, maxIter);
    }

    // Block when there is nothing usable to show yet.
    const bool block = m_blocking || !m_active.started();
    if (m_pending.step(block ? -1.0 : kFrameBudgetMs)) {
        m_active = std::move(m_pending);
        m_pending = {};
        uploadReference(ctx);
    } else {
        m_status.computing = true;
    }
}

void Mandel2D::uploadReference(const Context& ctx) {
    const auto& points = m_active.points();
    const uint64_t bytes = std::max<uint64_t>(points.size() * sizeof(RefPoint), sizeof(RefPoint));
    if (bytes > m_refCapacity) {
        m_refCapacity = std::max<uint64_t>(bytes, m_refCapacity * 2);
        wgpu::BufferDescriptor desc;
        desc.label = "reference orbit";
        desc.size = m_refCapacity;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        m_refBuffer = ctx.device.CreateBuffer(&desc);

        wgpu::BindGroupEntry entries[5];
        entries[0].binding = 0;
        entries[0].buffer = m_uniforms;
        entries[1].binding = 1;
        entries[1].textureView = m_palette->view;
        entries[2].binding = 2;
        entries[2].sampler = m_palette->sampler;
        entries[3].binding = 3;
        entries[3].buffer = m_perturbUniforms;
        entries[4].binding = 4;
        entries[4].buffer = m_refBuffer;
        wgpu::BindGroupDescriptor bgDesc;
        bgDesc.layout = m_perturbPipeline.GetBindGroupLayout(0);
        bgDesc.entryCount = 5;
        bgDesc.entries = entries;
        m_perturbBindGroup = ctx.device.CreateBindGroup(&bgDesc);
    }
    ctx.queue.WriteBuffer(m_refBuffer, 0, points.data(), points.size() * sizeof(RefPoint));
}

void Mandel2D::update(const Context& ctx, const FractalParams& params, const Camera2D& camera) {
    ctx.queue.WriteBuffer(m_uniforms, 0, &params, sizeof(params));

    m_status.active = m_forcePerturb || camera.scale < kPerturbBelowScale;
    if (!m_status.active) {
        m_status.computing = false;
        return;
    }

    const double pixelSize = 2.0 * camera.scale / double(std::max(ctx.height, 1u));
    updateReference(ctx, camera, params.maxIter, pixelSize);

    PerturbParams pp;
    int exp = 0;
    pp.pixelMant = float(std::frexp(pixelSize, &exp));
    pp.pixelExp = exp;
    pp.offsetPx[0] = float((camera.cx - m_active.cx()).toDouble() / pixelSize);
    pp.offsetPx[1] = float((camera.cy - m_active.cy()).toDouble() / pixelSize);
    pp.refLen = uint32_t(m_active.points().size());
    ctx.queue.WriteBuffer(m_perturbUniforms, 0, &pp, sizeof(pp));

    m_status.refLen = pp.refLen;
    m_status.refEscaped = m_active.escaped();
    m_status.precisionBits = m_active.fracLimbs() * 32;
}

void Mandel2D::draw(const wgpu::RenderPassEncoder& pass) const {
    if (m_status.active && m_perturbBindGroup) {
        pass.SetPipeline(m_perturbPipeline);
        pass.SetBindGroup(0, m_perturbBindGroup);
    } else {
        pass.SetPipeline(m_pipeline);
        pass.SetBindGroup(0, m_bindGroup);
    }
    pass.Draw(3);
}

} // namespace fb
