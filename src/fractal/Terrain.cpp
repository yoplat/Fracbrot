#include "fractal/Terrain.h"

#include "fractal/Palette.h"
#include "gfx/Context.h"
#include "gfx/Shader.h"
#include "shaders/common.wgsl.h"
#include "shaders/heightmap.wgsl.h"
#include "shaders/terrain.wgsl.h"

#include <vector>

namespace fb {

namespace {

wgpu::Buffer createUniform(const wgpu::Device& device, uint64_t size, const char* label) {
    wgpu::BufferDescriptor desc;
    desc.label = label;
    desc.size = size;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    return device.CreateBuffer(&desc);
}

} // namespace

void Terrain::init(const Context& ctx, const PaletteTexture& palette) {
    m_palette = &palette;
    m_params = createUniform(ctx.device, sizeof(FractalParams), "terrain params");
    m_scene = createUniform(ctx.device, sizeof(SceneParams), "terrain scene");

    wgpu::ShaderModule heightShader = createShader(ctx.device, {kCommonWgsl, kHeightmapWgsl}, "heightmap");
    wgpu::ComputePipelineDescriptor cDesc;
    cDesc.label = "heightmap";
    cDesc.compute.module = heightShader;
    cDesc.compute.entryPoint = "main";
    m_heightPipeline = ctx.device.CreateComputePipeline(&cDesc);

    wgpu::ShaderModule shader = createShader(ctx.device, {kCommonWgsl, kTerrainWgsl}, "terrain");
    wgpu::ColorTargetState target;
    target.format = ctx.format;
    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;

    wgpu::DepthStencilState depth;
    depth.format = Context::kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor rDesc;
    rDesc.label = "terrain";
    rDesc.vertex.module = shader;
    rDesc.vertex.entryPoint = "vs";
    rDesc.fragment = &fragment;
    rDesc.depthStencil = &depth;
    rDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    m_renderPipeline = ctx.device.CreateRenderPipeline(&rDesc);
}

void Terrain::resizeGrid(const Context& ctx, uint32_t gridSize) {
    m_gridSize = gridSize;

    wgpu::TextureDescriptor tDesc;
    tDesc.label = "terrain height";
    tDesc.size = {gridSize, gridSize, 1};
    tDesc.format = wgpu::TextureFormat::RG32Float;
    tDesc.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    m_height = ctx.device.CreateTexture(&tDesc);
    wgpu::TextureView heightView = m_height.CreateView();

    // Two triangles per grid cell, vertices addressed as y * gridSize + x.
    std::vector<uint32_t> indices;
    indices.reserve(size_t(gridSize - 1) * (gridSize - 1) * 6);
    for (uint32_t y = 0; y + 1 < gridSize; ++y) {
        for (uint32_t x = 0; x + 1 < gridSize; ++x) {
            const uint32_t i = y * gridSize + x;
            indices.insert(indices.end(), {i, i + gridSize, i + 1, i + 1, i + gridSize, i + gridSize + 1});
        }
    }
    m_indexCount = uint32_t(indices.size());
    wgpu::BufferDescriptor bDesc;
    bDesc.label = "terrain indices";
    bDesc.size = indices.size() * sizeof(uint32_t);
    bDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    m_indices = ctx.device.CreateBuffer(&bDesc);
    ctx.queue.WriteBuffer(m_indices, 0, indices.data(), bDesc.size);

    wgpu::BindGroupEntry hEntries[3];
    hEntries[0].binding = 0;
    hEntries[0].buffer = m_params;
    hEntries[1].binding = 1;
    hEntries[1].buffer = m_scene;
    hEntries[2].binding = 2;
    hEntries[2].textureView = heightView;
    wgpu::BindGroupDescriptor hDesc;
    hDesc.layout = m_heightPipeline.GetBindGroupLayout(0);
    hDesc.entryCount = 3;
    hDesc.entries = hEntries;
    m_heightBindGroup = ctx.device.CreateBindGroup(&hDesc);

    wgpu::BindGroupEntry rEntries[5];
    rEntries[0].binding = 0;
    rEntries[0].buffer = m_params;
    rEntries[1].binding = 1;
    rEntries[1].buffer = m_scene;
    rEntries[2].binding = 2;
    rEntries[2].textureView = heightView;
    rEntries[3].binding = 3;
    rEntries[3].textureView = m_palette->view;
    rEntries[4].binding = 4;
    rEntries[4].sampler = m_palette->sampler;
    wgpu::BindGroupDescriptor rDesc;
    rDesc.layout = m_renderPipeline.GetBindGroupLayout(0);
    rDesc.entryCount = 5;
    rDesc.entries = rEntries;
    m_renderBindGroup = ctx.device.CreateBindGroup(&rDesc);
}

void Terrain::update(const Context& ctx, const FractalParams& params, const SceneParams& scene) {
    if (scene.gridSize != m_gridSize) resizeGrid(ctx, scene.gridSize);
    ctx.queue.WriteBuffer(m_params, 0, &params, sizeof(params));
    ctx.queue.WriteBuffer(m_scene, 0, &scene, sizeof(scene));
}

void Terrain::compute(const wgpu::CommandEncoder& encoder) const {
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(m_heightPipeline);
    pass.SetBindGroup(0, m_heightBindGroup);
    const uint32_t groups = (m_gridSize + 7) / 8;
    pass.DispatchWorkgroups(groups, groups);
    pass.End();
}

void Terrain::draw(const wgpu::RenderPassEncoder& pass) const {
    pass.SetPipeline(m_renderPipeline);
    pass.SetBindGroup(0, m_renderBindGroup);
    pass.SetIndexBuffer(m_indices, wgpu::IndexFormat::Uint32);
    pass.DrawIndexed(m_indexCount);
}

} // namespace fb
