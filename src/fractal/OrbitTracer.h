#pragma once

#include "camera/Camera2D.h"

#include <webgpu/webgpu_cpp.h>

#include <cstdint>

namespace fb {

class Context;

// Draws the orbit z0 = 0, z1, z2, ... of one point c over the 2D view.
class OrbitTracer {
public:
    static constexpr uint32_t kMaxPoints = 2048;

    void init(const Context& ctx);
    // Iterates c (double precision) and uploads the path. Returns the escape
    // iteration, or -1 if the orbit stayed bounded for `maxIter` steps.
    int trace(const Context& ctx, const Camera2D& camera, double cx, double cy, uint32_t maxIter);
    void draw(const wgpu::RenderPassEncoder& pass) const;

private:
    wgpu::RenderPipeline m_pipeline;
    wgpu::Buffer m_vertices;
    wgpu::Buffer m_uniforms;
    wgpu::BindGroup m_bindGroup;
    uint32_t m_count = 0;
};

} // namespace fb
