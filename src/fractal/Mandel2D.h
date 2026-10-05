#pragma once

#include "fractal/Params.h"
#include "fractal/ReferenceOrbit.h"

#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;
struct Camera2D;

// Mirrors `struct Perturb` in shaders/perturb.wgsl.
struct PerturbParams {
    float offsetPx[2] = {0.0f, 0.0f};
    float pixelMant = 0.5f;
    int32_t pixelExp = 0;
    uint32_t refLen = 0;
    uint32_t _pad[3] = {};
};
static_assert(sizeof(PerturbParams) == 32);

struct PerturbStatus {
    bool active = false;    // perturbation path in use this frame
    bool computing = false; // a new reference orbit is being computed
    uint32_t refLen = 0;
    bool refEscaped = false;
    int precisionBits = 0;
};

// Fullscreen-pass escape-time renderer for the 2D Mandelbrot set. Uses plain
// f32 iteration for shallow views and perturbation around a high-precision
// reference orbit for deep zoom.
class Mandel2D {
public:
    static constexpr double kPerturbBelowScale = 1e-3;

    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const Context& ctx, const FractalParams& params, const Camera2D& camera);
    void draw(const wgpu::RenderPassEncoder& pass) const;

    // Compute reference orbits to completion within the frame (automated runs).
    void setBlocking(bool blocking) { m_blocking = blocking; }
    void setForcePerturbation(bool force) { m_forcePerturb = force; }
    bool forcePerturbation() const { return m_forcePerturb; }
    const PerturbStatus& status() const { return m_status; }

private:
    void updateReference(const Context& ctx, const Camera2D& camera, uint32_t maxIter, double pixelSize);
    bool suitable(const ReferenceOrbit& ref, const Camera2D& camera, uint32_t maxIter, double pixelSize,
                  double maxOffsetPx) const;
    void uploadReference(const Context& ctx);

    const PaletteTexture* m_palette = nullptr;
    wgpu::RenderPipeline m_pipeline;
    wgpu::RenderPipeline m_perturbPipeline;
    wgpu::Buffer m_uniforms;
    wgpu::Buffer m_perturbUniforms;
    wgpu::Buffer m_refBuffer;
    uint64_t m_refCapacity = 0;
    wgpu::BindGroup m_bindGroup;
    wgpu::BindGroup m_perturbBindGroup;

    ReferenceOrbit m_active;  // uploaded to the GPU
    ReferenceOrbit m_pending; // being computed
    bool m_blocking = false;
    bool m_forcePerturb = false;
    PerturbStatus m_status;
};

} // namespace fb
