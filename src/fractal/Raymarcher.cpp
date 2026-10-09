#include "fractal/Raymarcher.h"

#include "fractal/DistanceEstimator.h"
#include "fractal/Palette.h"
#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/blit.wgsl.h"
#include "shaders/common.wgsl.h"
#include "shaders/raymarch.wgsl.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fb {

namespace {

// Accumulation target for progressive anti-aliasing: float so averaging many samples
// doesn't band.
constexpr wgpu::TextureFormat kAccumFormat = wgpu::TextureFormat::RGBA16Float;

wgpu::RenderPipeline createFullscreenPipeline(const Context& ctx, const wgpu::ShaderModule& shader,
                                              const char* label, wgpu::TextureFormat format,
                                              const wgpu::BlendState* blend = nullptr,
                                              const wgpu::ConstantEntry* constant = nullptr) {
    wgpu::ColorTargetState target;
    target.format = format;
    target.blend = blend;
    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.constantCount = constant ? 1 : 0;
    fragment.constants = constant;
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

bool supportsPerturbation(Formula3D formula) {
    switch (formula) {
    case Formula3D::QuatMandelbrot:
    case Formula3D::QuatJulia:
    case Formula3D::Hypercomplex:
    case Formula3D::QuatFlip: return true;
    default: return false;
    }
}

void applyFormulaDefaults(MarchParams& params, Formula3D formula) {
    params.formula = formula;
    params.detail = 1.0f;
    params.rotation = glm::vec4(0.0f);
    switch (formula) {
    case Formula3D::QuatMandelbrot:
        params.iterations = 64;
        params.detail = 0.4f;
        params.trapMix = 0.8f;
        // Ice Fractal's default: 27 degrees in the YZ plane each iteration.
        params.rotation = {0.0f, glm::radians(27.0f), 0.0f, 0.0f};
        // Rotation lets the set grow beyond the plain spun Mandelbrot's bounds.
        params.boundCenter = {-0.5f, 0.0f, 0.0f};
        params.boundRadius = 2.4f;
        break;
    case Formula3D::QuatJulia:
        params.iterations = 12;
        params.trapMix = 0.8f;
        params.boundCenter = {0.0f, 0.0f, 0.0f};
        params.boundRadius = 1.8f;
        break;
    case Formula3D::Mandelbulb:
        params.iterations = 10;
        params.trapMix = 0.35f;
        params.boundCenter = {0.0f, 0.0f, 0.0f};
        params.boundRadius = 1.3f;
        break;
    case Formula3D::Bicomplex:
        params.iterations = 64;
        params.trapMix = 0.8f;
        params.boundCenter = {-0.75f, 0.0f, 0.0f};
        params.boundRadius = 2.2f;
        break;
    case Formula3D::Hypercomplex:
    case Formula3D::QuatFlip:
        params.iterations = 64;
        params.detail = 0.4f;
        params.trapMix = 0.8f;
        params.boundCenter = {-0.5f, 0.0f, 0.0f};
        params.boundRadius = 2.4f;
        break;
    case Formula3D::Count:
        break;
    }
}

void Raymarcher::init(const Context& ctx, const PaletteTexture& palette) {
    m_palette = &palette;
    m_params = createUniformBuffer(ctx.device, sizeof(FractalParams), "march params");
    m_scene = createUniformBuffer(ctx.device, sizeof(SceneParams), "march scene");
    m_march = createUniformBuffer(ctx.device, sizeof(MarchParams), "march");

    // Each new sample is blended in with weight 1/(n+1) via the blend constant, so the
    // target holds the running average.
    wgpu::BlendState average;
    average.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Constant, wgpu::BlendFactor::OneMinusConstant};
    average.alpha = average.color;
    const wgpu::ShaderModule march = createShader(ctx.device, {kCommonWgsl, kRaymarchWgsl}, "raymarch");
    m_marchPipeline = createFullscreenPipeline(ctx, march, "raymarch", kAccumFormat, &average);
    wgpu::ConstantEntry perturbOn;
    perturbOn.key = "PERTURB_PATH";
    perturbOn.value = 1.0;
    m_marchPipelinePerturb =
        createFullscreenPipeline(ctx, march, "raymarch deep zoom", kAccumFormat, &average, &perturbOn);
    m_blitPipeline =
        createFullscreenPipeline(ctx, createShader(ctx.device, {kBlitWgsl}, "blit"), "blit", ctx.format);

    wgpu::SamplerDescriptor sd;
    sd.magFilter = wgpu::FilterMode::Linear;
    sd.minFilter = wgpu::FilterMode::Linear;
    m_blitSampler = ctx.device.CreateSampler(&sd);

    m_paletteView = palette.view;
    m_paletteSampler = palette.sampler;
    createMarchBindGroup(ctx);
}

void Raymarcher::createMarchBindGroup(const Context& ctx) {
    if (m_refCapacity == 0) {
        m_refCapacity = 2 * sizeof(glm::vec4);
        wgpu::BufferDescriptor desc;
        desc.label = "3d reference orbit";
        desc.size = m_refCapacity;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        m_refOrbit = ctx.device.CreateBuffer(&desc);
    }
    wgpu::BindGroupEntry entries[6];
    entries[0].binding = 0;
    entries[0].buffer = m_params;
    entries[1].binding = 1;
    entries[1].buffer = m_scene;
    entries[2].binding = 2;
    entries[2].buffer = m_march;
    entries[3].binding = 3;
    entries[3].textureView = m_paletteView;
    entries[4].binding = 4;
    entries[4].sampler = m_paletteSampler;
    entries[5].binding = 5;
    entries[5].buffer = m_refOrbit;
    // Auto layouts differ per pipeline, so each gets its own bind group.
    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_marchPipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 6;
    bgDesc.entries = entries;
    m_marchBindGroup = ctx.device.CreateBindGroup(&bgDesc);
    bgDesc.layout = m_marchPipelinePerturb.GetBindGroupLayout(0);
    m_marchBindGroupPerturb = ctx.device.CreateBindGroup(&bgDesc);
}

MarchParams Raymarcher::prepare(MarchParams march, glm::dvec3 origin, bool perturb) {
    march.boundCenter = glm::vec3(glm::dvec3(march.boundCenter) - origin);
    march.refOrigin = glm::vec4(glm::vec3(origin), 0.0f);
    march.perturb = perturb && supportsPerturbation(march.formula) ? 1u : 0u;
    march.refLen = 0;
    march.pixelAngle = 0.0f;
    march.jitter = glm::vec2(0.0f);
    return march;
}

namespace {

// Halton radical inverse: a low-discrepancy sequence for sub-pixel sample positions.
float halton(uint32_t i, uint32_t base) {
    float f = 1.0f, r = 0.0f;
    for (; i > 0; i /= base) {
        f /= float(base);
        r += f * float(i % base);
    }
    return r;
}

} // namespace

void Raymarcher::resizeTarget(const Context& ctx, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    wgpu::TextureDescriptor tDesc;
    tDesc.label = "raymarch accumulation";
    tDesc.size = {width, height, 1};
    tDesc.format = kAccumFormat;
    tDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    m_offscreen = ctx.device.CreateTexture(&tDesc).CreateView();

    wgpu::BindGroupEntry entries[2];
    entries[0].binding = 0;
    entries[0].textureView = m_offscreen;
    entries[1].binding = 1;
    entries[1].sampler = m_blitSampler;
    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_blitPipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 2;
    bgDesc.entries = entries;
    m_blitBindGroup = ctx.device.CreateBindGroup(&bgDesc);
}

bool Raymarcher::changed(FractalParams params, const SceneParams& scene, const MarchParams& march, glm::dvec3 origin,
                         bool perturb) const {
    params.time = 0.0f; // not used by the ray-marcher; changes every frame
    params.resolution[0] = m_lastParams.resolution[0];
    params.resolution[1] = m_lastParams.resolution[1];
    MarchParams m = prepare(march, origin, perturb);
    m.pixelAngle = m_lastMarch.pixelAngle; // derived from resolution
    m.refLen = m_lastMarch.refLen;         // derived from the other inputs
    m.jitter = m_lastMarch.jitter;         // per-sample
    return std::memcmp(&params, &m_lastParams, sizeof(params)) != 0 ||
           std::memcmp(&scene, &m_lastScene, sizeof(scene)) != 0 ||
           std::memcmp(&m, &m_lastMarch, sizeof(m)) != 0 || m_palette->version != m_lastPaletteVersion;
}

void Raymarcher::update(const Context& ctx, FractalParams params, const SceneParams& scene, MarchParams march,
                        float fovY, float renderScale, glm::dvec3 origin, bool perturb) {
    const uint32_t w = std::max(1u, uint32_t(float(ctx.width) * renderScale));
    const uint32_t h = std::max(1u, uint32_t(float(ctx.height) * renderScale));
    if (w != m_width || h != m_height) {
        resizeTarget(ctx, w, h);
        m_dirty = true;
    }
    if (changed(params, scene, march, origin, perturb)) m_dirty = true;
    if (m_dirty) m_samples = 0;
    m_marchThisFrame = m_samples < kMaxSamples;
    if (!m_marchThisFrame) return;
    if (!m_dirty) {
        // Same image, next anti-aliasing sample: only the jitter changes.
        m_lastMarch.jitter = glm::vec2(halton(m_samples + 1, 2), halton(m_samples + 1, 3)) - 0.5f;
        ctx.queue.WriteBuffer(m_march, 0, &m_lastMarch, sizeof(m_lastMarch));
        return;
    }
    params.time = 0.0f;

    const MarchParams raw = march;
    march = prepare(march, origin, perturb);
    if (march.perturb) {
        const std::vector<glm::vec4> orbit = referenceOrbit(raw, origin, raw.iterations + 2);
        const uint64_t bytes = orbit.size() * sizeof(glm::vec4);
        if (bytes > m_refCapacity) {
            m_refCapacity = std::max(bytes, 2 * m_refCapacity);
            wgpu::BufferDescriptor desc;
            desc.label = "3d reference orbit";
            desc.size = m_refCapacity;
            desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
            m_refOrbit = ctx.device.CreateBuffer(&desc);
            createMarchBindGroup(ctx);
        }
        ctx.queue.WriteBuffer(m_refOrbit, 0, orbit.data(), bytes);
        march.refLen = uint32_t(orbit.size());
    }

    params.resolution[0] = float(w);
    params.resolution[1] = float(h);
    march.pixelAngle = 2.0f * std::tan(0.5f * fovY) / float(h);

    ctx.queue.WriteBuffer(m_params, 0, &params, sizeof(params));
    ctx.queue.WriteBuffer(m_scene, 0, &scene, sizeof(scene));
    ctx.queue.WriteBuffer(m_march, 0, &march, sizeof(march));
    m_lastParams = params;
    m_lastScene = scene;
    m_lastMarch = march;
    m_lastPaletteVersion = m_palette->version;
}

void Raymarcher::render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& target) {
    wgpu::RenderPassColorAttachment color;
    color.view = m_offscreen;
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor desc;
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &color;

    if (m_marchThisFrame) {
        // Sample n is blended in with weight 1/(n + 1); the first one replaces the image.
        color.loadOp = m_samples == 0 ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        const double weight = 1.0 / double(m_samples + 1);
        const wgpu::Color blend{weight, weight, weight, weight};
        wgpu::RenderPassEncoder march = encoder.BeginRenderPass(&desc);
        const bool deep = m_lastMarch.perturb != 0;
        march.SetPipeline(deep ? m_marchPipelinePerturb : m_marchPipeline);
        march.SetBindGroup(0, deep ? m_marchBindGroupPerturb : m_marchBindGroup);
        march.SetBlendConstant(&blend);
        march.Draw(3);
        march.End();
        ++m_samples;
        m_dirty = false;
        m_marchThisFrame = false;
    }
    color.loadOp = wgpu::LoadOp::Clear;

    color.view = target;
    wgpu::RenderPassEncoder blit = encoder.BeginRenderPass(&desc);
    blit.SetPipeline(m_blitPipeline);
    blit.SetBindGroup(0, m_blitBindGroup);
    blit.Draw(3);
    blit.End();
}

} // namespace fb
