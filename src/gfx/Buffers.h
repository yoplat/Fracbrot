#pragma once

#include <webgpu/webgpu_cpp.h>

namespace fb {

inline wgpu::Buffer createUniformBuffer(const wgpu::Device& device, uint64_t size, const char* label) {
    wgpu::BufferDescriptor desc;
    desc.label = label;
    desc.size = size;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    return device.CreateBuffer(&desc);
}

} // namespace fb
