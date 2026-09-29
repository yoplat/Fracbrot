#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace fb {

enum ParamFlags : uint32_t {
    kFlagSmooth = 1u << 0,
};

// Mirrors `struct Params` in shaders/common.wgsl; keep layouts in sync (16-byte aligned).
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

// Mirrors `struct Scene` in the 3D shaders.
struct SceneParams {
    glm::mat4 viewProj{1.0f};
    glm::mat4 invViewProj{1.0f};
    glm::vec4 camPos{0.0f};
    glm::vec4 lightDir{0.4f, 0.8f, 0.3f, 0.0f}; // towards the light
    float heightScale = 0.35f;
    uint32_t gridSize = 512;
    float interiorHeight = 0.0f; // height of the set itself, in [0, 1]
    float heightCurve = 1.0f;    // exponent applied to the normalized height
};
static_assert(sizeof(SceneParams) == 176);

} // namespace fb
