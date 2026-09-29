#pragma once

#include <webgpu/webgpu_cpp.h>

#include <string>

namespace fb {

class Context;

// Copies a BGRA8/RGBA8 texture to the CPU and writes it as PNG. Blocks until done.
bool saveTexturePng(const Context& ctx, const wgpu::Texture& texture, const std::string& path);

} // namespace fb
