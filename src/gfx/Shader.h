#pragma once

#include <webgpu/webgpu_cpp.h>

namespace fb {

inline wgpu::ShaderModule createShader(const wgpu::Device& device, const char* code, const char* label) {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = code;
    wgpu::ShaderModuleDescriptor desc;
    desc.nextInChain = &wgsl;
    desc.label = label;
    return device.CreateShaderModule(&desc);
}

} // namespace fb
