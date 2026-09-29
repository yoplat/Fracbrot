#include "fractal/Palette.h"

#include <algorithm>
#include <cmath>

namespace fb {

std::span<const PalettePreset> palettePresets() {
    static const std::vector<PalettePreset> presets = {
        {"Classic", {{0.0f, 0.00f, 0.03f, 0.39f},
                     {0.16f, 0.13f, 0.42f, 0.80f},
                     {0.42f, 0.93f, 1.00f, 1.00f},
                     {0.6425f, 1.00f, 0.67f, 0.00f},
                     {0.8575f, 0.00f, 0.01f, 0.00f}}},
        {"Fire", {{0.0f, 0.02f, 0.00f, 0.00f},
                  {0.25f, 0.55f, 0.05f, 0.00f},
                  {0.5f, 1.00f, 0.50f, 0.00f},
                  {0.75f, 1.00f, 0.95f, 0.55f},
                  {0.9f, 0.45f, 0.10f, 0.02f}}},
        {"Ocean", {{0.0f, 0.00f, 0.02f, 0.08f},
                   {0.3f, 0.00f, 0.30f, 0.50f},
                   {0.55f, 0.30f, 0.85f, 0.85f},
                   {0.7f, 0.95f, 1.00f, 0.95f},
                   {0.85f, 0.05f, 0.25f, 0.40f}}},
        {"Neon", {{0.0f, 0.05f, 0.00f, 0.15f},
                  {0.2f, 0.90f, 0.10f, 0.70f},
                  {0.4f, 0.10f, 0.00f, 0.30f},
                  {0.6f, 0.10f, 0.90f, 1.00f},
                  {0.8f, 0.00f, 0.10f, 0.20f}}},
        {"Grayscale", {{0.0f, 0.0f, 0.0f, 0.0f}, {0.5f, 1.0f, 1.0f, 1.0f}}},
    };
    return presets;
}

std::vector<uint8_t> bakePalette(const PalettePreset& preset, uint32_t width) {
    const auto& stops = preset.stops;
    std::vector<uint8_t> out(size_t(width) * 4);
    for (uint32_t i = 0; i < width; ++i) {
        const float t = (float(i) + 0.5f) / float(width);

        // Find the segment [a, b] containing t, wrapping from the last stop to the first.
        size_t b = 0;
        while (b < stops.size() && stops[b].t <= t) ++b;
        const PaletteStop& sa = stops[(b + stops.size() - 1) % stops.size()];
        const PaletteStop& sb = stops[b % stops.size()];
        float span = sb.t - sa.t;
        float local = t - sa.t;
        if (span <= 0.0f) span += 1.0f;
        if (local < 0.0f) local += 1.0f;
        const float f = std::clamp(local / span, 0.0f, 1.0f);
        const float s = f * f * (3.0f - 2.0f * f); // smoothstep between stops

        const float rgb[3] = {sa.r + (sb.r - sa.r) * s, sa.g + (sb.g - sa.g) * s, sa.b + (sb.b - sa.b) * s};
        for (int c = 0; c < 3; ++c) out[i * 4 + c] = uint8_t(std::lround(std::clamp(rgb[c], 0.0f, 1.0f) * 255.0f));
        out[i * 4 + 3] = 255;
    }
    return out;
}

void PaletteTexture::init(const wgpu::Device& device) {
    wgpu::TextureDescriptor desc;
    desc.label = "palette";
    desc.size = {kWidth, 1, 1};
    desc.format = wgpu::TextureFormat::RGBA8Unorm;
    desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    m_texture = device.CreateTexture(&desc);
    view = m_texture.CreateView();

    wgpu::SamplerDescriptor sd;
    sd.addressModeU = wgpu::AddressMode::Repeat;
    sd.addressModeV = wgpu::AddressMode::ClampToEdge;
    sd.magFilter = wgpu::FilterMode::Linear;
    sd.minFilter = wgpu::FilterMode::Linear;
    sampler = device.CreateSampler(&sd);
}

void PaletteTexture::upload(const wgpu::Queue& queue, const PalettePreset& preset) {
    const std::vector<uint8_t> texels = bakePalette(preset, kWidth);
    wgpu::TexelCopyTextureInfo dst;
    dst.texture = m_texture;
    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = kWidth * 4;
    wgpu::Extent3D size{kWidth, 1, 1};
    queue.WriteTexture(&dst, texels.data(), texels.size(), &layout, &size);
}

} // namespace fb
