#pragma once

#include <cstdint>

namespace fb {

enum ParamFlags : uint32_t {
    kFlagSmooth = 1u << 0,
};

// Mirrors `struct Params` in the WGSL shaders; keep layouts in sync (16-byte aligned).
struct FractalParams {
    float center[2] = {-0.5f, 0.0f};
    float scale = 1.2f;
    uint32_t maxIter = 256;
    float resolution[2] = {1.0f, 1.0f};
    float time = 0.0f;
    float colorDensity = 1.0f; // palette repeat rate
    float colorOffset = 0.0f;  // palette phase in [0,1)
    uint32_t flags = kFlagSmooth;
    uint32_t _pad[2] = {};
};
static_assert(sizeof(FractalParams) == 48);

} // namespace fb
