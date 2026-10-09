#pragma once

#include "fractal/Params.h"

#include <glm/glm.hpp>
#include <webgpu/webgpu_cpp.h>

namespace fb {

class Context;
class PaletteTexture;

enum class Formula3D : uint32_t {
    QuatMandelbrot = 0,
    QuatJulia = 1,
    Mandelbulb = 2,
    Bicomplex = 3,
    Hypercomplex = 4, // commutative i,j,k squaring from Yann Le Bihan's video (see raymarch.wgsl)
    QuatFlip = 5,     // quaternion with Syntopia's first-iteration y/z flip
    Count
};

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
    glm::vec4 rotation{0.0f}; // per-iteration quaternion rotation (XZ, YZ, XW, unused), radians
    // Filled in by Raymarcher::update: the shader works relative to refOrigin (the camera).
    glm::vec4 refOrigin{0.0f};
    uint32_t refLen = 0;  // reference orbit entries
    uint32_t perturb = 0; // iterate offsets from the reference orbit (deep zoom)
    glm::vec2 jitter{0.0f}; // sub-pixel sample offset for progressive anti-aliasing
};
static_assert(sizeof(MarchParams) == 128);

// Formulas the shader can iterate by perturbation for deep zoom.
bool supportsPerturbation(Formula3D formula);

// Sensible defaults (bounds, iterations) for each formula.
void applyFormulaDefaults(MarchParams& params, Formula3D formula);

// Fullscreen ray-marcher. Renders at `renderScale` of the target size into an
// offscreen texture, then upscales, so the expensive pass stays interactive.
class Raymarcher {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    // True if these inputs would render a different image than the last update's
    // (resolution aside). Doesn't modify state.
    // `scene` must be relative to `origin` (the camera position in world space).
    bool changed(FractalParams params, const SceneParams& scene, const MarchParams& march, glm::dvec3 origin,
                 bool perturb) const;
    void update(const Context& ctx, FractalParams params, const SceneParams& scene, MarchParams march,
                float fovY, float renderScale, glm::dvec3 origin, bool perturb);
    // Records the ray-march pass and the upscale into `target`. A changed image restarts at
    // one sample; an unchanged one gains another jittered sample (progressive anti-aliasing)
    // until kMaxSamples, after which frames only upscale the finished image.
    void render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& target);
    uint32_t samples() const { return m_samples; }

    static constexpr uint32_t kMaxSamples = 16;
    // Forces the next render to ray-march even if nothing changed (benchmarks).
    void invalidate() { m_dirty = true; }

private:
    void resizeTarget(const Context& ctx, uint32_t width, uint32_t height);
    void createMarchBindGroup(const Context& ctx);
    // The parameters as uploaded, minus the resolution-dependent and reference-length fields.
    static MarchParams prepare(MarchParams march, glm::dvec3 origin, bool perturb);

    wgpu::RenderPipeline m_marchPipeline;        // regular
    wgpu::RenderPipeline m_marchPipelinePerturb; // deep zoom (PERTURB_PATH = true)
    wgpu::RenderPipeline m_blitPipeline;
    wgpu::Buffer m_params;
    wgpu::Buffer m_scene;
    wgpu::Buffer m_march;
    wgpu::Buffer m_refOrbit;
    uint64_t m_refCapacity = 0;
    wgpu::Sampler m_blitSampler;
    wgpu::TextureView m_offscreen;
    wgpu::BindGroup m_marchBindGroup;
    wgpu::BindGroup m_marchBindGroupPerturb;
    wgpu::BindGroup m_blitBindGroup;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Inputs of the image currently in m_offscreen, for skipping unchanged frames.
    const PaletteTexture* m_palette = nullptr;
    wgpu::TextureView m_paletteView;
    wgpu::Sampler m_paletteSampler;
    FractalParams m_lastParams{};
    SceneParams m_lastScene{};
    MarchParams m_lastMarch{};
    uint32_t m_lastPaletteVersion = ~0u;
    bool m_dirty = true;
    uint32_t m_samples = 0;      // samples accumulated in m_offscreen
    bool m_marchThisFrame = false;
};

} // namespace fb
