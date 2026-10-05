#include "fractal/Raymarcher.h"

#include "fractal/Palette.h"
#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/blit.wgsl.h"
#include "shaders/common.wgsl.h"
#include "shaders/raymarch.wgsl.h"

#include <algorithm>
#include <cmath>

namespace fb {

namespace {

wgpu::RenderPipeline createFullscreenPipeline(const Context& ctx, const wgpu::ShaderModule& shader,
                                              const char* label) {
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
    case Formula3D::Count:
        break;
    }
}

void Raymarcher::init(const Context& ctx, const PaletteTexture& palette) {
    m_params = createUniformBuffer(ctx.device, sizeof(FractalParams), "march params");
    m_scene = createUniformBuffer(ctx.device, sizeof(SceneParams), "march scene");
    m_march = createUniformBuffer(ctx.device, sizeof(MarchParams), "march");

    m_marchPipeline = createFullscreenPipeline(
        ctx, createShader(ctx.device, {kCommonWgsl, kRaymarchWgsl}, "raymarch"), "raymarch");
    m_blitPipeline = createFullscreenPipeline(ctx, createShader(ctx.device, {kBlitWgsl}, "blit"), "blit");

    wgpu::SamplerDescriptor sd;
    sd.magFilter = wgpu::FilterMode::Linear;
    sd.minFilter = wgpu::FilterMode::Linear;
    m_blitSampler = ctx.device.CreateSampler(&sd);

    wgpu::BindGroupEntry entries[5];
    entries[0].binding = 0;
    entries[0].buffer = m_params;
    entries[1].binding = 1;
    entries[1].buffer = m_scene;
    entries[2].binding = 2;
    entries[2].buffer = m_march;
    entries[3].binding = 3;
    entries[3].textureView = palette.view;
    entries[4].binding = 4;
    entries[4].sampler = palette.sampler;
    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_marchPipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 5;
    bgDesc.entries = entries;
    m_marchBindGroup = ctx.device.CreateBindGroup(&bgDesc);
}

void Raymarcher::resizeTarget(const Context& ctx, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    wgpu::TextureDescriptor tDesc;
    tDesc.label = "raymarch offscreen";
    tDesc.size = {width, height, 1};
    tDesc.format = ctx.format;
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

void Raymarcher::update(const Context& ctx, FractalParams params, const SceneParams& scene, MarchParams march,
                        float fovY, float renderScale) {
    const uint32_t w = std::max(1u, uint32_t(float(ctx.width) * renderScale));
    const uint32_t h = std::max(1u, uint32_t(float(ctx.height) * renderScale));
    if (w != m_width || h != m_height) resizeTarget(ctx, w, h);

    params.resolution[0] = float(w);
    params.resolution[1] = float(h);
    march.pixelAngle = 2.0f * std::tan(0.5f * fovY) / float(h);

    ctx.queue.WriteBuffer(m_params, 0, &params, sizeof(params));
    ctx.queue.WriteBuffer(m_scene, 0, &scene, sizeof(scene));
    ctx.queue.WriteBuffer(m_march, 0, &march, sizeof(march));
}

void Raymarcher::render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& target) const {
    wgpu::RenderPassColorAttachment color;
    color.view = m_offscreen;
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor desc;
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &color;

    wgpu::RenderPassEncoder march = encoder.BeginRenderPass(&desc);
    march.SetPipeline(m_marchPipeline);
    march.SetBindGroup(0, m_marchBindGroup);
    march.Draw(3);
    march.End();

    color.view = target;
    wgpu::RenderPassEncoder blit = encoder.BeginRenderPass(&desc);
    blit.SetPipeline(m_blitPipeline);
    blit.SetBindGroup(0, m_blitBindGroup);
    blit.Draw(3);
    blit.End();
}

} // namespace fb
