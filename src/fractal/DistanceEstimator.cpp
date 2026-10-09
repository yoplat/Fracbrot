#include "fractal/DistanceEstimator.h"

#include <algorithm>
#include <cmath>

namespace fb {

namespace {

// Quaternions as (real, i, j, k) in (x, y, z, w), matching the shader.
glm::vec4 qsq(glm::vec4 q) {
    return {q.x * q.x - (q.y * q.y + q.z * q.z + q.w * q.w), 2.0f * q.x * q.y, 2.0f * q.x * q.z, 2.0f * q.x * q.w};
}

glm::vec4 toSlice(const MarchParams& m, glm::vec3 pos) {
    const float ct = std::cos(m.tilt), st = std::sin(m.tilt);
    return {pos.x * ct - m.sliceW * st, pos.y, pos.z, pos.x * st + m.sliceW * ct};
}

glm::vec4 rotateIter(const MarchParams& m, glm::vec4 z) {
    const glm::vec3 cs = glm::cos(glm::vec3(m.rotation));
    const glm::vec3 sn = glm::sin(glm::vec3(m.rotation));
    z = {z.x, z.y * cs.x - z.w * sn.x, z.z, z.y * sn.x + z.w * cs.x};
    z = {z.x, z.y, z.z * cs.y - z.w * sn.y, z.z * sn.y + z.w * cs.y};
    z = {z.y * sn.z + z.x * cs.z, z.y * cs.z - z.x * sn.z, z.z, z.w};
    return z;
}

float deQuat(const MarchParams& m, glm::vec3 pos, bool julia) {
    const glm::vec4 p = toSlice(m, pos);
    glm::vec4 z{0.0f}, c = p;
    float dr = 0.0f;
    if (julia) {
        z = p;
        c = m.juliaC;
        dr = 1.0f;
    }
    const float dc = julia ? 0.0f : 1.0f;
    float r2 = glm::dot(z, z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r2 > 256.0f) break;
        dr = 2.0f * std::sqrt(r2) * dr + dc;
        z = rotateIter(m, qsq(z) + c);
        r2 = glm::dot(z, z);
    }
    const float r = std::sqrt(r2);
    return 0.5f * r * std::log(std::max(r, 1e-8f)) / std::max(dr, 1e-8f);
}

float deQuatFlip(const MarchParams& m, glm::vec3 pos) {
    const glm::vec4 c = toSlice(m, pos);
    glm::vec4 z{c.x, -c.z, -c.y, c.w};
    float dr = 1.0f;
    float r2 = glm::dot(z, z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r2 > 256.0f) break;
        dr = 2.0f * std::sqrt(r2) * dr + 1.0f;
        z = rotateIter(m, qsq(z) + c);
        r2 = glm::dot(z, z);
    }
    const float r = std::sqrt(r2);
    return 0.5f * r * std::log(std::max(r, 1e-8f)) / std::max(dr, 1e-8f);
}

glm::vec4 hmul(glm::vec4 a, glm::vec4 b) {
    return {a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w, a.x * b.y + a.y * b.x + a.z * b.w + a.w * b.z,
            a.x * b.z + a.z * b.x - a.y * b.w - a.w * b.y, a.x * b.w + a.w * b.x + a.y * b.z + a.z * b.y};
}

float deHyper(const MarchParams& m, glm::vec3 pos) {
    constexpr float kBailout2 = 1e4f;
    const glm::vec4 c = toSlice(m, pos);
    const glm::vec4 e0{std::cos(m.tilt), 0.0f, 0.0f, std::sin(m.tilt)};
    glm::vec4 z{0.0f}, d0{0.0f}, d1{0.0f}, d2{0.0f};
    float r2 = 0.0f;
    for (uint32_t i = 0; i < m.iterations; ++i) {
        d0 = rotateIter(m, 2.0f * hmul(z, d0) + e0);
        d1 = rotateIter(m, 2.0f * hmul(z, d1) + glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
        d2 = rotateIter(m, 2.0f * hmul(z, d2) + glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
        z = rotateIter(m, hmul(z, z) + c);
        r2 = glm::dot(z, z);
        if (r2 > kBailout2) break;
    }
    if (r2 <= kBailout2) return 0.0f;
    const glm::vec3 g{glm::dot(d0, z), glm::dot(d1, z), glm::dot(d2, z)};
    return 0.25f * r2 * std::log(r2) / std::max(glm::length(g), 1e-20f);
}

float deBulb(const MarchParams& m, glm::vec3 worldPos) {
    const glm::vec3 c{worldPos.x, worldPos.z, worldPos.y};
    glm::vec3 z = c;
    float dr = 1.0f;
    float r = glm::length(z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r > 2.0f) break;
        const float theta = std::acos(std::clamp(z.z / std::max(r, 1e-8f), -1.0f, 1.0f)) * m.power;
        const float phi = std::atan2(z.y, z.x) * m.power;
        const float rp = std::pow(r, m.power - 1.0f);
        dr = rp * m.power * dr + 1.0f;
        z = rp * r * glm::vec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)) + c;
        r = glm::length(z);
    }
    return 0.5f * std::log(std::max(r, 1e-8f)) * r / dr;
}

float deMandel2(const MarchParams& m, glm::vec2 c) {
    glm::vec2 z{0.0f}, dz{0.0f};
    float r2 = 0.0f;
    for (uint32_t i = 0; i < m.iterations; ++i) {
        dz = 2.0f * glm::vec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) + glm::vec2(1.0f, 0.0f);
        z = glm::vec2(z.x * z.x - z.y * z.y, 2.0f * z.x * z.y) + c;
        r2 = glm::dot(z, z);
        if (r2 > 1e4f) break;
    }
    if (r2 <= 1e4f) return 0.0f;
    const float r = std::sqrt(r2);
    return 0.5f * r * std::log(r) / glm::length(dz);
}

float deBicomplex(const MarchParams& m, glm::vec3 pos) {
    const glm::vec4 q = toSlice(m, pos);
    const float d1 = deMandel2(m, {q.x - q.w, q.y + q.z});
    const float d2 = deMandel2(m, {q.x + q.w, q.y - q.z});
    return std::max(d1, d2) * 0.70710678f;
}

} // namespace

float distanceEstimate(const MarchParams& m, glm::vec3 pos) {
    switch (m.formula) {
    case Formula3D::QuatJulia: return deQuat(m, pos, true);
    case Formula3D::Mandelbulb: return deBulb(m, pos);
    case Formula3D::Bicomplex: return deBicomplex(m, pos);
    case Formula3D::Hypercomplex: return deHyper(m, pos);
    case Formula3D::QuatFlip: return deQuatFlip(m, pos);
    default: return deQuat(m, pos, false);
    }
}

std::optional<float> raycast(const MarchParams& m, glm::vec3 origin, glm::vec3 dir, float pixelAngle) {
    // Clip to the bounding sphere, as the shader does.
    const glm::vec3 oc = origin - m.boundCenter;
    const float b = glm::dot(oc, dir);
    const float h = b * b - (glm::dot(oc, oc) - m.boundRadius * m.boundRadius);
    if (h < 0.0f) return std::nullopt;
    const float s = std::sqrt(h);
    const float tFar = -b + s;
    if (tFar < 0.0f) return std::nullopt;

    float t = std::max(-b - s, 0.0f);
    for (uint32_t i = 0; i < 4 * m.maxSteps; ++i) {
        const float d = distanceEstimate(m, origin + dir * t);
        if (d < std::max(t * pixelAngle * m.detail, 1e-6f)) return t;
        t += d;
        if (t > tFar) break;
    }
    return std::nullopt;
}

bool pivotOnSurface(OrbitCamera& cam, const MarchParams& m, float pixelAngle) {
    const glm::vec3 eye = cam.position();
    const glm::vec3 forward = glm::normalize(cam.target - eye);
    // Thin fins can pass right beside the camera while the surface ahead is still far, so
    // also cap the pivot at the distance to the nearest geometry in any direction.
    const float nearest = std::max(distanceEstimate(m, eye), 1e-6f);
    const auto hit = raycast(m, eye, forward, pixelAngle);
    const float pivot = std::min(hit ? *hit : cam.distance, nearest);
    if (!hit && pivot >= cam.distance) return false;
    cam.target = eye + forward * pivot;
    cam.distance = pivot;
    return true;
}

void zoomTowardSurface(OrbitCamera& cam, const MarchParams& m, float pixelAngle, float factor) {
    const glm::vec3 eye = cam.position();
    const glm::vec3 forward = glm::normalize(cam.target - eye);
    const float ahead = raycast(m, eye, forward, pixelAngle).value_or(cam.distance);
    cam.target += forward * ((1.0f - factor) * ahead);
}

} // namespace fb
