#pragma once

#include "fractal/Params.h"

#include <glm/glm.hpp>
#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;

enum class Formula3D : uint32_t { QuatMandelbrot = 0, QuatJulia = 1, Mandelbulb = 2, Bicomplex = 3, Count };

enum MarchFlags : uint32_t {
    kMarchShadows = 1u << 0,
    kMarchAO = 1u << 1,
    kMarchReflect = 1u << 2,
};

// Mirrors `struct March` in shaders/raymarch.wgsl.
struct MarchParams {
    Formula3D formula = Formula3D::QuatMandelbrot;
    uint32_t iterations = 16;
    float detail = 1.0f;
    uint32_t maxSteps = 200;
    float shadowSoftness = 12.0f;
    float aoStrength = 1.0f;
    uint32_t flags = kMarchShadows | kMarchAO | kMarchReflect;
    float trapMix = 0.8f;
    float pixelAngle = 0.001f; // set by Raymarcher::update
    float power = 8.0f;
    float sliceW = 0.0f;
    float tilt = 0.0f;
    glm::vec4 juliaC{-0.291f, -0.399f, 0.339f, 0.437f};
    glm::vec3 boundCenter{-0.75f, 0.0f, 0.0f};
    float boundRadius = 1.4f;
};
static_assert(sizeof(MarchParams) == 80);

// Sensible defaults (bounds, iterations) for each formula.
void applyFormulaDefaults(MarchParams& params, Formula3D formula);

// Fullscreen ray-marcher. Renders at `renderScale` of the target size into an
// offscreen texture, then upscales, so the expensive pass stays interactive.
class Raymarcher {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const Context& ctx, FractalParams params, const SceneParams& scene, MarchParams march,
                float fovY, float renderScale);
    // Records the ray-march pass and the upscale into `target`.
    void render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& target) const;

private:
    void resizeTarget(const Context& ctx, uint32_t width, uint32_t height);

    wgpu::RenderPipeline m_marchPipeline;
    wgpu::RenderPipeline m_blitPipeline;
    wgpu::Buffer m_params;
    wgpu::Buffer m_scene;
    wgpu::Buffer m_march;
    wgpu::Sampler m_blitSampler;
    wgpu::TextureView m_offscreen;
    wgpu::BindGroup m_marchBindGroup;
    wgpu::BindGroup m_blitBindGroup;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace fb
