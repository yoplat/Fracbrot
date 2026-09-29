#pragma once

#include <cstdint>

namespace fb {

// Mirrors `struct Params` in the WGSL shaders; keep layouts in sync (16-byte aligned).
struct FractalParams {
    float center[2] = {-0.5f, 0.0f};
    float scale = 1.2f;
    uint32_t maxIter = 256;
    float resolution[2] = {1.0f, 1.0f};
    float time = 0.0f;
    uint32_t _pad = 0;
};
static_assert(sizeof(FractalParams) == 32);

} // namespace fb
