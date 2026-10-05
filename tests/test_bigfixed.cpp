// Minimal self-checking tests for the deep-zoom math (no framework needed).
#include "camera/Camera2D.h"
#include "fractal/ReferenceOrbit.h"
#include "math/BigFixed.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

} // namespace

int main() {
    using fb::BigFixed;

    // double round trip and basic arithmetic
    check(near(BigFixed(-1.25).toDouble(), -1.25, 0.0), "double round trip");
    check(near((BigFixed(1.5) + BigFixed(-2.75)).toDouble(), -1.25, 0.0), "add mixed signs");
    check(near((BigFixed(0.1) - BigFixed(0.3)).toDouble(), 0.1 - 0.3, 1e-16), "sub");
    check(near((BigFixed(-1.5) * BigFixed(2.5)).toDouble(), -3.75, 0.0), "mul");
    check(near((BigFixed(0.5) * BigFixed(0.5)).toDouble(), 0.25, 0.0), "mul frac");
    BigFixed m(3.0);
    check(near(m.mul2().toDouble(), 6.0, 0.0), "mul2");

    // decimal parsing / printing beyond double precision
    const std::string digits = "-1.7686104930146770745031756532702265202396779075886654948";
    auto p = BigFixed::parse(digits, 8);
    check(p.has_value(), "parse");
    check(p && p->toString(int(digits.size()) - 3) == digits, "toString round trip");
    check(BigFixed::parse("-0.7453", 3)->toString(17) == "-0.74530000000000000", "toString rounds");
    check(BigFixed(0.5).toString(0) == "1" && BigFixed(-0.25).toString(1) == "-0.3", "toString rounding carry");
    check(p && near(p->toDouble(), -1.7686104930146770745, 1e-15), "parse toDouble");
    check(!BigFixed::parse("1.2x", 4).has_value(), "parse rejects junk");

    // tiny differences survive: (a + 1e-60) - a == 1e-60
    BigFixed a = *BigFixed::parse("0.123456789", 10);
    BigFixed b = a;
    b += 1e-60;
    check(near((b - a).toDouble(), 1e-60, 1e-75), "tiny difference");

    // camera zoom keeps precision growing at deep scales and round-trips through strings
    fb::Camera2D cam;
    for (int k = 0; k < 400; ++k) cam.zoomAt(900, 300, 1600, 1000, 0.5);
    check(cam.scale < 1e-100, "deep scale reached");
    check(cam.cx.fracLimbs() >= BigFixed::limbsFor(cam.scale), "camera precision grows");
    auto parsed = fb::Camera2D::parse(cam.toString());
    check(parsed && near((parsed->cx - cam.cx).toDouble(), 0.0, cam.scale * 1e-6), "camera string round trip");

    // reference orbit: c = i is pre-periodic (0, i, -1+i, -i, -1+i, ...) and never escapes
    fb::ReferenceOrbit orbit;
    orbit.start(BigFixed(0.0, 4), BigFixed(1.0, 4), 1000);
    check(orbit.step(-1.0) && !orbit.escaped() && orbit.points().size() == 1001, "orbit of i completes");
    const fb::RefPoint& z3 = orbit.points()[3]; // -i
    check(near(std::ldexp(double(z3.x), z3.e), 0.0, 1e-12) && near(std::ldexp(double(z3.y), z3.e), -1.0, 1e-12),
          "orbit value Z_3 = -i");
    orbit.start(BigFixed(1.0, 2), BigFixed(0.0, 2), 1000);
    check(orbit.step(-1.0) && orbit.escaped() && orbit.points().size() < 10, "orbit of 1 escapes");

    if (g_failures == 0) std::printf("all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
