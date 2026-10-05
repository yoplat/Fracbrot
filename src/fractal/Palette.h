#pragma once

#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <span>
#include <vector>

namespace fb {

struct PaletteStop {
    float t;
    float r, g, b;
};

struct PalettePreset {
    const char* name;
    std::vector<PaletteStop> stops; // t in [0,1), wraps around
};

std::span<const PalettePreset> palettePresets();

// Bakes a cyclic gradient into `width` RGBA8 texels.
std::vector<uint8_t> bakePalette(const PalettePreset& preset, uint32_t width);

// GPU gradient lookup (width x 1 texture + repeating linear sampler), shared by all renderers.
class PaletteTexture {
public:
    static constexpr uint32_t kWidth = 1024;

    void init(const wgpu::Device& device);
    void upload(const wgpu::Queue& queue, const PalettePreset& preset);

    wgpu::TextureView view;
    wgpu::Sampler sampler;
    uint32_t version = 0; // bumped on every upload, so cached renders know to refresh

private:
    wgpu::Texture m_texture;
};

} // namespace fb
