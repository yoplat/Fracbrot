#pragma once

#include "fractal/Params.h"

#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;

// Heightfield view of the escape-time field: a compute pass fills a height
// texture, then a gridSize² mesh is displaced by it in the vertex shader.
class Terrain {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const Context& ctx, const FractalParams& params, const SceneParams& scene);
    void compute(const wgpu::CommandEncoder& encoder) const;
    void draw(const wgpu::RenderPassEncoder& pass) const;

private:
    void resizeGrid(const Context& ctx, uint32_t gridSize);

    const PaletteTexture* m_palette = nullptr;
    wgpu::ComputePipeline m_heightPipeline;
    wgpu::RenderPipeline m_renderPipeline;
    wgpu::Buffer m_params;
    wgpu::Buffer m_scene;
    wgpu::Texture m_height;
    wgpu::Buffer m_indices;
    wgpu::BindGroup m_heightBindGroup;
    wgpu::BindGroup m_renderBindGroup;
    uint32_t m_gridSize = 0;
    uint32_t m_indexCount = 0;
};

} // namespace fb
