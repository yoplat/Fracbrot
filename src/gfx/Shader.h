#pragma once

#include <webgpu/webgpu_cpp.h>

#include <initializer_list>
#include <string>

namespace fb {

// Builds a shader module from WGSL chunks concatenated in order (e.g. common + main).
inline wgpu::ShaderModule createShader(const wgpu::Device& device, std::initializer_list<const char*> chunks,
                                       const char* label) {
    std::string code;
    for (const char* c : chunks) {
        code += c;
        code += '\n';
    }
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = code.c_str();
    wgpu::ShaderModuleDescriptor desc;
    desc.nextInChain = &wgsl;
    desc.label = label;
    return device.CreateShaderModule(&desc);
}

} // namespace fb
