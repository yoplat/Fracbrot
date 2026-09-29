#pragma once

#include "fractal/Params.h"

#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;

// Mirrors `struct Bif` in shaders/bifurcation*.wgsl.
struct BifurcationParams {
    float cMin = -2.0f;
    float cMax = 0.25f;
    uint32_t columns = 2048;
    uint32_t samples = 256;
    uint32_t warmup = 1000;
    float pointSize = 1.5f;
    float brightness = 0.35f;
    float highlightC = 1e9f; // real c of the highlighted column (off by default)
    float yScale = 1.0f;
    float planeDim = 0.6f;
    float resolution[2] = {1.0f, 1.0f};
};
static_assert(sizeof(BifurcationParams) == 48);

// Logistic-map r <-> Mandelbrot c on the real axis: c = r(2 - r)/4.
inline double logisticToC(double r) { return r * (2.0 - r) / 4.0; }

// Mandelbrot plane plus the real-axis bifurcation diagram standing on it.
class Bifurcation {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const Context& ctx, const FractalParams& params, const SceneParams& scene, BifurcationParams bif);
    void compute(const wgpu::CommandEncoder& encoder) const;
    void draw(const wgpu::RenderPassEncoder& pass) const;

private:
    void resizePoints(const Context& ctx, uint32_t columns, uint32_t samples);

    const PaletteTexture* m_palette = nullptr;
    wgpu::ComputePipeline m_computePipeline;
    wgpu::RenderPipeline m_planePipeline;
    wgpu::RenderPipeline m_pointPipeline;
    wgpu::Buffer m_params;
    wgpu::Buffer m_scene;
    wgpu::Buffer m_bif;
    wgpu::Buffer m_points;
    wgpu::BindGroup m_computeBindGroup;
    wgpu::BindGroup m_planeBindGroup;
    wgpu::BindGroup m_pointBindGroup;
    uint32_t m_columns = 0;
    uint32_t m_samples = 0;
};

} // namespace fb
