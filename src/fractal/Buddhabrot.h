#pragma once

#include <webgpu/webgpu_cpp.h>

#include <cstdint>

namespace fb {

class Context;
class PaletteTexture;
struct Camera2D;

// Mirrors `struct Buddha` in shaders/buddhabrot_common.wgsl.
struct BuddhaParams {
    float center[2] = {0.0f, 0.0f};
    float scale = 1.0f;
    uint32_t rotate = 1;
    uint32_t histSize[2] = {1, 1};
    float screenSize[2] = {1.0f, 1.0f};
    uint32_t limits[4] = {5000, 500, 50, 5000};
    float regionMin[2] = {-2.05f, 0.0f};
    float regionMax[2] = {0.55f, 1.25f};
    uint32_t frame = 0;
    uint32_t samplesPerThread = 4;
    uint32_t minIter = 20;
    uint32_t channels = 3;
    float exposure = 1.0f;
    float gamma = 1.0f;
    uint32_t _pad[2] = {};
};
static_assert(sizeof(BuddhaParams) == 96);

struct BuddhaSettings {
    uint32_t limits[3] = {5000, 500, 50}; // R, G, B iteration limits (nebulabrot)
    uint32_t minIter = 20;                // skip orbits escaping in fewer steps
    bool nebula = true;                   // false: one channel (R limit) through the palette
    bool rotate = true;                   // real axis vertical: the classic upright Buddha
    float histScale = 0.5f;               // histogram resolution relative to the screen
    uint32_t samplesPerFrame = 1u << 20;  // random c values drawn per frame
    float exposure = 1.0f;
    float gamma = 1.6f;
    bool paused = false;
};

// Progressive Buddhabrot / Nebulabrot: a compute pass adds random escaping orbits to a
// per-pixel histogram every frame; a fullscreen pass tone-maps it. Changing the view or any
// setting that affects the histogram restarts the accumulation.
class Buddhabrot {
public:
    void init(const Context& ctx, const PaletteTexture& palette);
    void update(const Context& ctx, const Camera2D& view, const BuddhaSettings& settings);
    // Records the clear (if restarting), accumulation (unless paused) and max reduction.
    void compute(const wgpu::CommandEncoder& encoder);
    void draw(const wgpu::RenderPassEncoder& pass) const;

    void reset() { m_resetPending = true; }
    uint64_t samples() const { return m_samples; }

private:
    void resizeHistogram(const Context& ctx, uint32_t width, uint32_t height);

    const PaletteTexture* m_palette = nullptr;
    wgpu::ComputePipeline m_accumPipeline;
    wgpu::ComputePipeline m_reducePipeline;
    wgpu::RenderPipeline m_displayPipeline;
    wgpu::Buffer m_uniforms;
    wgpu::Buffer m_hist;
    wgpu::Buffer m_max;
    wgpu::BindGroup m_accumBindGroup;
    wgpu::BindGroup m_reduceBindGroup;
    wgpu::BindGroup m_displayBindGroup;

    BuddhaParams m_params;
    BuddhaParams m_lastKey; // fields that define the histogram contents
    uint64_t m_histBytes = 0;
    uint64_t m_samples = 0;
    uint32_t m_workgroups = 0;
    bool m_accumulate = true;
    bool m_resetPending = true;
};

} // namespace fb
