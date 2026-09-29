#include "fractal/Mandel2D.h"

#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/mandel2d.wgsl.h"

namespace fb {

void Mandel2D::init(const Context& ctx) {
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

    wgpu::BindGroupEntry entry;
    entry.binding = 0;
    entry.buffer = m_uniforms;
    entry.size = sizeof(FractalParams);

    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_pipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 1;
    bgDesc.entries = &entry;
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
