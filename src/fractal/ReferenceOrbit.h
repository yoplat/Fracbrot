#pragma once

#include "math/BigFixed.h"

#include <cstdint>
#include <vector>

namespace fb {

// Extended-range orbit point for the GPU: value = (x, y) * 2^e, max(|x|,|y|) in [0.5, 1).
// Mirrors `struct RefPoint` in shaders/perturb.wgsl.
struct RefPoint {
    float x = 0.0f;
    float y = 0.0f;
    int32_t e = 0;
    int32_t _pad = 0;
};
static_assert(sizeof(RefPoint) == 16);

// High-precision orbit Z_{n+1} = Z_n² + c of a reference point, used by the
// perturbation renderer. Computed incrementally so it never stalls a frame.
class ReferenceOrbit {
public:
    static constexpr double kBailout2 = 65536.0; // matches BAILOUT2 in the shaders

    // Restarts the computation for c = (cx, cy) at the precision of cx/cy.
    void start(const BigFixed& cx, const BigFixed& cy, uint32_t maxIter);
    // Iterates for at most `budgetMs` (negative = until done). Returns true when complete.
    bool step(double budgetMs);

    bool started() const { return !m_points.empty(); }
    bool complete() const { return m_escaped || m_points.size() > m_maxIter; }
    bool escaped() const { return m_escaped; }
    uint32_t maxIter() const { return m_maxIter; }
    int fracLimbs() const { return m_cx.fracLimbs(); }
    const BigFixed& cx() const { return m_cx; }
    const BigFixed& cy() const { return m_cy; }
    // Z_0 .. Z_{len-1}; the last point is the escaping one if escaped().
    const std::vector<RefPoint>& points() const { return m_points; }

private:
    void push(double x, double y);

    BigFixed m_cx, m_cy; // c
    BigFixed m_zx, m_zy; // current Z_n
    uint32_t m_maxIter = 0;
    bool m_escaped = false;
    std::vector<RefPoint> m_points;
};

} // namespace fb
