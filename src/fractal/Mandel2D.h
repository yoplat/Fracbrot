#pragma once

#include "fractal/Params.h"

#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;

// Fullscreen-pass escape-time renderer for the 2D Mandelbrot set.
class Mandel2D {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const wgpu::Queue& queue, const FractalParams& params);
    void draw(const wgpu::RenderPassEncoder& pass) const;

private:
    wgpu::RenderPipeline m_pipeline;
    wgpu::Buffer m_uniforms;
    wgpu::BindGroup m_bindGroup;
};

} // namespace fb
