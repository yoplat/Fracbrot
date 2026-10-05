#include "fractal/OrbitTracer.h"

#include "gfx/Buffers.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/orbit.wgsl.h"

#include <algorithm>
#include <vector>

namespace fb {

namespace {

struct OrbitUniforms {
    float scale;
    float aspect;
    float _pad[2];
    float color[4];
};
static_assert(sizeof(OrbitUniforms) == 32);

} // namespace

void OrbitTracer::init(const Context& ctx) {
    wgpu::ShaderModule shader = createShader(ctx.device, {kOrbitWgsl}, "orbit");

    wgpu::VertexAttribute attr;
    attr.format = wgpu::VertexFormat::Float32x2;
    attr.offset = 0;
    attr.shaderLocation = 0;
    wgpu::VertexBufferLayout layout;
    layout.arrayStride = 2 * sizeof(float);
    layout.attributeCount = 1;
    layout.attributes = &attr;

    wgpu::BlendState blend;
    blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::SrcAlpha, wgpu::BlendFactor::OneMinusSrcAlpha};
    blend.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
    wgpu::ColorTargetState target;
    target.format = ctx.format;
    target.blend = &blend;
    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;

    wgpu::RenderPipelineDescriptor desc;
    desc.label = "orbit";
    desc.vertex.module = shader;
    desc.vertex.entryPoint = "vs";
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &layout;
    desc.fragment = &fragment;
    desc.primitive.topology = wgpu::PrimitiveTopology::LineStrip;
    m_pipeline = ctx.device.CreateRenderPipeline(&desc);

    wgpu::BufferDescriptor vDesc;
    vDesc.label = "orbit vertices";
    vDesc.size = kMaxPoints * 2 * sizeof(float);
    vDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    m_vertices = ctx.device.CreateBuffer(&vDesc);

    m_uniforms = createUniformBuffer(ctx.device, sizeof(OrbitUniforms), "orbit uniforms");
    wgpu::BindGroupEntry entry;
    entry.binding = 0;
    entry.buffer = m_uniforms;
    wgpu::BindGroupDescriptor bg;
    bg.layout = m_pipeline.GetBindGroupLayout(0);
    bg.entryCount = 1;
    bg.entries = &entry;
    m_bindGroup = ctx.device.CreateBindGroup(&bg);
}

int OrbitTracer::trace(const Context& ctx, const Camera2D& camera, double cx, double cy, uint32_t maxIter) {
    std::vector<float> path;
    path.reserve(size_t(kMaxPoints) * 2);
    const uint32_t limit = std::min(maxIter, kMaxPoints - 1);

    // Offsets from the view center keep vertices precise when zoomed in.
    const double ox = camera.cx.toDouble(), oy = camera.cy.toDouble();
    double zx = 0.0, zy = 0.0;
    int escapedAt = -1;
    path.insert(path.end(), {float(zx - ox), float(zy - oy)});
    for (uint32_t i = 0; i < limit; ++i) {
        const double nx = zx * zx - zy * zy + cx;
        zy = 2.0 * zx * zy + cy;
        zx = nx;
        path.insert(path.end(), {float(zx - ox), float(zy - oy)});
        if (zx * zx + zy * zy > 4.0) {
            escapedAt = int(i + 1);
            break;
        }
    }

    m_count = uint32_t(path.size() / 2);
    ctx.queue.WriteBuffer(m_vertices, 0, path.data(), path.size() * sizeof(float));

    const OrbitUniforms u{float(camera.scale), float(ctx.width) / float(std::max(ctx.height, 1u)), {},
                          {1.0f, 0.95f, 0.7f, 0.85f}};
    ctx.queue.WriteBuffer(m_uniforms, 0, &u, sizeof(u));
    return escapedAt;
}

void OrbitTracer::draw(const wgpu::RenderPassEncoder& pass) const {
    if (m_count < 2) return;
    pass.SetPipeline(m_pipeline);
    pass.SetBindGroup(0, m_bindGroup);
    pass.SetVertexBuffer(0, m_vertices, 0, uint64_t(m_count) * 2 * sizeof(float));
    pass.Draw(m_count);
}

} // namespace fb
