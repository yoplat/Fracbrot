#include "fractal/ReferenceOrbit.h"

#include <chrono>
#include <cmath>

namespace fb {

void ReferenceOrbit::start(const BigFixed& cx, const BigFixed& cy, uint32_t maxIter) {
    m_cx = cx;
    m_cy = cy;
    m_zx = BigFixed(0.0, cx.fracLimbs());
    m_zy = BigFixed(0.0, cx.fracLimbs());
    m_maxIter = maxIter;
    m_escaped = false;
    m_points.clear();
    m_points.reserve(size_t(maxIter) + 1);
    push(0.0, 0.0); // Z_0
}

void ReferenceOrbit::push(double x, double y) {
    RefPoint p;
    const double m = std::max(std::fabs(x), std::fabs(y));
    if (m > 0.0) {
        std::frexp(m, &p.e);
        p.x = float(std::ldexp(x, -p.e));
        p.y = float(std::ldexp(y, -p.e));
    }
    m_points.push_back(p);
}

bool ReferenceOrbit::step(double budgetMs) {
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::duration<double, std::milli>(budgetMs);

    uint32_t sinceCheck = 0;
    while (!complete()) {
        const BigFixed x2 = m_zx * m_zx;
        const BigFixed y2 = m_zy * m_zy;
        BigFixed xy = m_zx * m_zy;
        m_zx = x2 - y2 + m_cx;
        m_zy = xy.mul2() + m_cy;

        const double x = m_zx.toDouble();
        const double y = m_zy.toDouble();
        push(x, y);
        if (x * x + y * y > kBailout2) m_escaped = true;

        if (budgetMs >= 0.0 && ++sinceCheck >= 64) {
            sinceCheck = 0;
            if (Clock::now() >= deadline) break;
        }
    }
    return complete();
}

} // namespace fb
