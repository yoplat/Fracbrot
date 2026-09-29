#include "gfx/Readback.h"

#include "gfx/Context.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace fb {

bool saveTexturePng(const Context& ctx, const wgpu::Texture& texture, const std::string& path) {
    const uint32_t w = texture.GetWidth();
    const uint32_t h = texture.GetHeight();
    const wgpu::TextureFormat fmt = texture.GetFormat();
    const bool bgra = fmt == wgpu::TextureFormat::BGRA8Unorm || fmt == wgpu::TextureFormat::BGRA8UnormSrgb;
    if (!bgra && fmt != wgpu::TextureFormat::RGBA8Unorm && fmt != wgpu::TextureFormat::RGBA8UnormSrgb) {
        std::fprintf(stderr, "screenshot: unsupported format %d\n", int(fmt));
        return false;
    }

    const uint32_t rowBytes = w * 4;
    const uint32_t paddedRow = (rowBytes + 255u) & ~255u;

    wgpu::BufferDescriptor bufDesc;
    bufDesc.size = uint64_t(paddedRow) * h;
    bufDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer buffer = ctx.device.CreateBuffer(&bufDesc);

    wgpu::TexelCopyTextureInfo src;
    src.texture = texture;
    wgpu::TexelCopyBufferInfo dst;
    dst.buffer = buffer;
    dst.layout.bytesPerRow = paddedRow;
    dst.layout.rowsPerImage = h;
    wgpu::Extent3D size{w, h, 1};

    wgpu::CommandEncoder encoder = ctx.device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&src, &dst, &size);
    wgpu::CommandBuffer commands = encoder.Finish();
    ctx.queue.Submit(1, &commands);

    bool ok = false;
    ctx.instance.WaitAny(
        buffer.MapAsync(wgpu::MapMode::Read, 0, bufDesc.size, wgpu::CallbackMode::WaitAnyOnly,
            [&ok](wgpu::MapAsyncStatus status, wgpu::StringView) { ok = status == wgpu::MapAsyncStatus::Success; }),
        UINT64_MAX);
    if (!ok) return false;

    const auto* mapped = static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, bufDesc.size));
    std::vector<uint8_t> pixels(size_t(rowBytes) * h);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* s = mapped + size_t(y) * paddedRow;
        uint8_t* d = pixels.data() + size_t(y) * rowBytes;
        std::memcpy(d, s, rowBytes);
        if (bgra) {
            for (uint32_t x = 0; x < w; ++x) std::swap(d[x * 4 + 0], d[x * 4 + 2]);
        }
    }
    buffer.Unmap();

    if (!stbi_write_png(path.c_str(), int(w), int(h), 4, pixels.data(), int(rowBytes))) return false;
    std::printf("saved %s (%ux%u)\n", path.c_str(), w, h);
    return true;
}

} // namespace fb
