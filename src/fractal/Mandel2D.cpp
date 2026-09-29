#include "fractal/Mandel2D.h"

#include "fractal/Palette.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/mandel2d.wgsl.h"

namespace fb {

void Mandel2D::init(const Context& ctx, const PaletteTexture& palette) {
    wgpu::ShaderModule shader = createShader(ctx.device, kMandel2dWgsl, "mandel2d");

    wgpu::ColorTargetState target;
    target.format = ctx.format;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;

    wgpu::RenderPipelineDescriptor desc;
    desc.label = "mandel2d";
    desc.vertex.module = shader;
    desc.vertex.entryPoint = "vs";
    desc.fragment = &fragment;
    m_pipeline = ctx.device.CreateRenderPipeline(&desc);

    wgpu::BufferDescriptor bufDesc;
    bufDesc.label = "mandel2d params";
    bufDesc.size = sizeof(FractalParams);
    bufDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    m_uniforms = ctx.device.CreateBuffer(&bufDesc);

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

void Mandel2D::update(const wgpu::Queue& queue, const FractalParams& params) {
    queue.WriteBuffer(m_uniforms, 0, &params, sizeof(params));
}

void Mandel2D::draw(const wgpu::RenderPassEncoder& pass) const {
    pass.SetPipeline(m_pipeline);
    pass.SetBindGroup(0, m_bindGroup);
    pass.Draw(3);
}

} // namespace fb
