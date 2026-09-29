#include "fractal/Bifurcation.h"

#include "fractal/Palette.h"
#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/bifurcation.wgsl.h"
#include "shaders/bifurcation_compute.wgsl.h"
#include "shaders/common.wgsl.h"

namespace fb {

void Bifurcation::init(const Context& ctx, const PaletteTexture& palette) {
    m_palette = &palette;
    m_params = createUniformBuffer(ctx.device, sizeof(FractalParams), "bif params");
    m_scene = createUniformBuffer(ctx.device, sizeof(SceneParams), "bif scene");
    m_bif = createUniformBuffer(ctx.device, sizeof(BifurcationParams), "bif");

    wgpu::ComputePipelineDescriptor cDesc;
    cDesc.label = "bifurcation compute";
    cDesc.compute.module = createShader(ctx.device, {kBifurcationComputeWgsl}, "bifurcation compute");
    cDesc.compute.entryPoint = "main";
    m_computePipeline = ctx.device.CreateComputePipeline(&cDesc);

    wgpu::ShaderModule shader = createShader(ctx.device, {kCommonWgsl, kBifurcationWgsl}, "bifurcation");

    // Opaque plane, writes depth.
    wgpu::ColorTargetState planeTarget;
    planeTarget.format = ctx.format;
    wgpu::FragmentState planeFragment;
    planeFragment.module = shader;
    planeFragment.entryPoint = "fsPlane";
    planeFragment.targetCount = 1;
    planeFragment.targets = &planeTarget;
    wgpu::DepthStencilState planeDepth;
    planeDepth.format = Context::kDepthFormat;
    planeDepth.depthWriteEnabled = true;
    planeDepth.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor planeDesc;
    planeDesc.label = "bifurcation plane";
    planeDesc.vertex.module = shader;
    planeDesc.vertex.entryPoint = "vsPlane";
    planeDesc.fragment = &planeFragment;
    planeDesc.depthStencil = &planeDepth;
    m_planePipeline = ctx.device.CreateRenderPipeline(&planeDesc);

    // Additive points, depth-tested against the plane but not writing.
    wgpu::BlendState additive;
    additive.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::SrcAlpha, wgpu::BlendFactor::One};
    additive.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
    wgpu::ColorTargetState pointTarget;
    pointTarget.format = ctx.format;
    pointTarget.blend = &additive;
    wgpu::FragmentState pointFragment;
    pointFragment.module = shader;
    pointFragment.entryPoint = "fsPoint";
    pointFragment.targetCount = 1;
    pointFragment.targets = &pointTarget;
    wgpu::DepthStencilState pointDepth;
    pointDepth.format = Context::kDepthFormat;
    pointDepth.depthWriteEnabled = false;
    pointDepth.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor pointDesc;
    pointDesc.label = "bifurcation points";
    pointDesc.vertex.module = shader;
    pointDesc.vertex.entryPoint = "vsPoint";
    pointDesc.fragment = &pointFragment;
    pointDesc.depthStencil = &pointDepth;
    m_pointPipeline = ctx.device.CreateRenderPipeline(&pointDesc);

    wgpu::BindGroupEntry planeEntries[5];
    planeEntries[0].binding = 0;
    planeEntries[0].buffer = m_params;
    planeEntries[1].binding = 1;
    planeEntries[1].buffer = m_scene;
    planeEntries[2].binding = 2;
    planeEntries[2].buffer = m_bif;
    planeEntries[3].binding = 3;
    planeEntries[3].textureView = palette.view;
    planeEntries[4].binding = 4;
    planeEntries[4].sampler = palette.sampler;
    wgpu::BindGroupDescriptor planeBg;
    planeBg.layout = m_planePipeline.GetBindGroupLayout(0);
    planeBg.entryCount = 5;
    planeBg.entries = planeEntries;
    m_planeBindGroup = ctx.device.CreateBindGroup(&planeBg);
}

void Bifurcation::resizePoints(const Context& ctx, uint32_t columns, uint32_t samples) {
    m_columns = columns;
    m_samples = samples;

    wgpu::BufferDescriptor desc;
    desc.label = "bifurcation points";
    desc.size = uint64_t(columns) * samples * 2 * sizeof(float);
    desc.usage = wgpu::BufferUsage::Storage;
    m_points = ctx.device.CreateBuffer(&desc);

    wgpu::BindGroupEntry cEntries[2];
    cEntries[0].binding = 0;
    cEntries[0].buffer = m_bif;
    cEntries[1].binding = 1;
    cEntries[1].buffer = m_points;
    wgpu::BindGroupDescriptor cBg;
    cBg.layout = m_computePipeline.GetBindGroupLayout(0);
    cBg.entryCount = 2;
    cBg.entries = cEntries;
    m_computeBindGroup = ctx.device.CreateBindGroup(&cBg);

    wgpu::BindGroupEntry pEntries[3];
    pEntries[0].binding = 1;
    pEntries[0].buffer = m_scene;
    pEntries[1].binding = 2;
    pEntries[1].buffer = m_bif;
    pEntries[2].binding = 5;
    pEntries[2].buffer = m_points;
    wgpu::BindGroupDescriptor pBg;
    pBg.layout = m_pointPipeline.GetBindGroupLayout(0);
    pBg.entryCount = 3;
    pBg.entries = pEntries;
    m_pointBindGroup = ctx.device.CreateBindGroup(&pBg);
}

void Bifurcation::update(const Context& ctx, const FractalParams& params, const SceneParams& scene,
                         BifurcationParams bif) {
    if (bif.columns != m_columns || bif.samples != m_samples) resizePoints(ctx, bif.columns, bif.samples);
    bif.resolution[0] = float(ctx.width);
    bif.resolution[1] = float(ctx.height);
    ctx.queue.WriteBuffer(m_params, 0, &params, sizeof(params));
    ctx.queue.WriteBuffer(m_scene, 0, &scene, sizeof(scene));
    ctx.queue.WriteBuffer(m_bif, 0, &bif, sizeof(bif));
}

void Bifurcation::compute(const wgpu::CommandEncoder& encoder) const {
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(m_computePipeline);
    pass.SetBindGroup(0, m_computeBindGroup);
    pass.DispatchWorkgroups((m_columns + 63) / 64);
    pass.End();
}

void Bifurcation::draw(const wgpu::RenderPassEncoder& pass) const {
    pass.SetPipeline(m_planePipeline);
    pass.SetBindGroup(0, m_planeBindGroup);
    pass.Draw(6);

    pass.SetPipeline(m_pointPipeline);
    pass.SetBindGroup(0, m_pointBindGroup);
    pass.Draw(6, m_columns * m_samples);
}

} // namespace fb
