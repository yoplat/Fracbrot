#include "fractal/DistanceEstimator.h"

#include <algorithm>
#include <cmath>

namespace fb {

namespace {

// Everything here runs in double precision: navigation must keep working at zoom depths
// where float positions have run out of resolution.
using dvec2 = glm::dvec2;
using dvec3 = glm::dvec3;
using dvec4 = glm::dvec4;

// Quaternions as (real, i, j, k) in (x, y, z, w), matching the shader.
dvec4 qsq(dvec4 q) {
    return {q.x * q.x - (q.y * q.y + q.z * q.z + q.w * q.w), 2.0 * q.x * q.y, 2.0 * q.x * q.z, 2.0 * q.x * q.w};
}

dvec4 rotateIter(const MarchParams& m, dvec4 z) {
    const dvec3 cs = glm::cos(dvec3(m.rotation));
    const dvec3 sn = glm::sin(dvec3(m.rotation));
    z = {z.x, z.y * cs.x - z.w * sn.x, z.z, z.y * sn.x + z.w * cs.x};
    z = {z.x, z.y, z.z * cs.y - z.w * sn.y, z.z * sn.y + z.w * cs.y};
    z = {z.y * sn.z + z.x * cs.z, z.y * cs.z - z.x * sn.z, z.z, z.w};
    return z;
}

dvec4 hmul(dvec4 a, dvec4 b) {
    return {a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w, a.x * b.y + a.y * b.x + a.z * b.w + a.w * b.z,
            a.x * b.z + a.z * b.x - a.y * b.w - a.w * b.y, a.x * b.w + a.w * b.x + a.y * b.z + a.z * b.y};
}

double deQuat(const MarchParams& m, dvec3 pos, bool julia) {
    const dvec4 p = toSlice(m, pos);
    dvec4 z{0.0}, c = p;
    double dr = 0.0;
    if (julia) {
        z = p;
        c = dvec4(m.juliaC);
        dr = 1.0;
    }
    const double dc = julia ? 0.0 : 1.0;
    double r2 = glm::dot(z, z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r2 > 256.0) break;
        dr = 2.0 * std::sqrt(r2) * dr + dc;
        z = rotateIter(m, qsq(z) + c);
        r2 = glm::dot(z, z);
    }
    const double r = std::sqrt(r2);
    return 0.5 * r * std::log(std::max(r, 1e-300)) / std::max(dr, 1e-300);
}

double deQuatFlip(const MarchParams& m, dvec3 pos) {
    const dvec4 c = toSlice(m, pos);
    dvec4 z{c.x, -c.z, -c.y, c.w};
    double dr = 1.0;
    double r2 = glm::dot(z, z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r2 > 256.0) break;
        dr = 2.0 * std::sqrt(r2) * dr + 1.0;
        z = rotateIter(m, qsq(z) + c);
        r2 = glm::dot(z, z);
    }
    const double r = std::sqrt(r2);
    return 0.5 * r * std::log(std::max(r, 1e-300)) / std::max(dr, 1e-300);
}

double deHyper(const MarchParams& m, dvec3 pos) {
    constexpr double kBailout2 = 1e4;
    const dvec4 c = toSlice(m, pos);
    const dvec4 e0{std::cos(double(m.tilt)), 0.0, 0.0, std::sin(double(m.tilt))};
    dvec4 z{0.0}, d0{0.0}, d1{0.0}, d2{0.0};
    double r2 = 0.0;
    for (uint32_t i = 0; i < m.iterations; ++i) {
        d0 = rotateIter(m, 2.0 * hmul(z, d0) + e0);
        d1 = rotateIter(m, 2.0 * hmul(z, d1) + dvec4(0.0, 1.0, 0.0, 0.0));
        d2 = rotateIter(m, 2.0 * hmul(z, d2) + dvec4(0.0, 0.0, 1.0, 0.0));
        z = rotateIter(m, hmul(z, z) + c);
        r2 = glm::dot(z, z);
        if (r2 > kBailout2) break;
    }
    if (r2 <= kBailout2) return 0.0;
    const dvec3 g{glm::dot(d0, z), glm::dot(d1, z), glm::dot(d2, z)};
    return 0.25 * r2 * std::log(r2) / std::max(glm::length(g), 1e-300);
}

double deBulb(const MarchParams& m, dvec3 worldPos) {
    const double power = m.power;
    const dvec3 c{worldPos.x, worldPos.z, worldPos.y};
    dvec3 z = c;
    double dr = 1.0;
    double r = glm::length(z);
    for (uint32_t i = 0; i < m.iterations; ++i) {
        if (r > 2.0) break;
        const double theta = std::acos(std::clamp(z.z / std::max(r, 1e-300), -1.0, 1.0)) * power;
        const double phi = std::atan2(z.y, z.x) * power;
        const double rp = std::pow(r, power - 1.0);
        dr = rp * power * dr + 1.0;
        z = rp * r * dvec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)) + c;
        r = glm::length(z);
    }
    return 0.5 * std::log(std::max(r, 1e-300)) * r / dr;
}

double deMandel2(const MarchParams& m, dvec2 c) {
    dvec2 z{0.0}, dz{0.0};
    double r2 = 0.0;
    for (uint32_t i = 0; i < m.iterations; ++i) {
        dz = 2.0 * dvec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) + dvec2(1.0, 0.0);
        z = dvec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        r2 = glm::dot(z, z);
        if (r2 > 1e4) break;
    }
    if (r2 <= 1e4) return 0.0;
    const double r = std::sqrt(r2);
    return 0.5 * r * std::log(r) / glm::length(dz);
}

double deBicomplex(const MarchParams& m, dvec3 pos) {
    const dvec4 q = toSlice(m, pos);
    const double d1 = deMandel2(m, {q.x - q.w, q.y + q.z});
    const double d2 = deMandel2(m, {q.x + q.w, q.y - q.z});
    return std::max(d1, d2) * 0.70710678118654752;
}

} // namespace

glm::dvec4 toSlice(const MarchParams& m, glm::dvec3 pos) {
    const double ct = std::cos(double(m.tilt)), st = std::sin(double(m.tilt));
    return {pos.x * ct - m.sliceW * st, pos.y, pos.z, pos.x * st + m.sliceW * ct};
}

double distanceEstimate(const MarchParams& m, glm::dvec3 pos) {
    switch (m.formula) {
    case Formula3D::QuatJulia: return deQuat(m, pos, true);
    case Formula3D::Mandelbulb: return deBulb(m, pos);
    case Formula3D::Bicomplex: return deBicomplex(m, pos);
    case Formula3D::Hypercomplex: return deHyper(m, pos);
    case Formula3D::QuatFlip: return deQuatFlip(m, pos);
    default: return deQuat(m, pos, false);
    }
}

std::optional<double> raycast(const MarchParams& m, glm::dvec3 origin, glm::dvec3 dir, double pixelAngle) {
    // Clip to the bounding sphere, as the shader does.
    const dvec3 oc = origin - dvec3(m.boundCenter);
    const double b = glm::dot(oc, dir);
    const double h = b * b - (glm::dot(oc, oc) - double(m.boundRadius) * m.boundRadius);
    if (h < 0.0) return std::nullopt;
    const double s = std::sqrt(h);
    const double tFar = -b + s;
    if (tFar < 0.0) return std::nullopt;

    double t = std::max(-b - s, 0.0);
    for (uint32_t i = 0; i < 4 * m.maxSteps; ++i) {
        const double d = distanceEstimate(m, origin + dir * t);
        if (d < std::max(t * pixelAngle * m.detail, 1e-15)) return t;
        t += d;
        if (t > tFar) break;
    }
    return std::nullopt;
}

void zoomAlongRay(OrbitCamera& cam, const MarchParams& m, glm::dvec3 dir, double pixelAngle, double factor) {
    const double ahead = raycast(m, cam.positionD(), dir, pixelAngle).value_or(cam.distance);
    cam.translate(dir * ((1.0 - factor) * ahead));
}

double clearance(const OrbitCamera& cam, const MarchParams& m) {
    return std::max(distanceEstimate(m, cam.positionD()), 1e-15);
}

} // namespace fb
