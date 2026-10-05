#pragma once

#include "math/BigFixed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace fb {

// 2D view over the complex plane. The center is arbitrary precision (for deep
// zoom); scale is a double, which bounds zoom depth at roughly 1e300.
// Screen coordinates are framebuffer pixels, origin top-left.
struct Camera2D {
    static constexpr double kMinScale = 1e-290;

    BigFixed cx{-0.5};
    BigFixed cy{0.0};
    double scale = 1.2; // half-height of the view in complex units

    void reset() { *this = {}; }

    void pan(double dxPx, double dyPx, double height) {
        const double k = 2.0 * scale / height;
        cx -= dxPx * k;
        cy += dyPx * k;
    }

    // Zooms by `factor` (<1 zooms in) keeping the point under (px, py) fixed.
    void zoomAt(double px, double py, double width, double height, double factor) {
        const double ox = (px - 0.5 * width) / height * 2.0;
        const double oy = (py - 0.5 * height) / height * 2.0;
        const double newScale = std::max(scale * factor, kMinScale);
        growPrecision(newScale);
        cx += ox * (scale - newScale);
        cy -= oy * (scale - newScale);
        scale = newScale;
    }

    // Parses "X,Y,SCALE" where X and Y may have arbitrarily many digits.
    static std::optional<Camera2D> parse(std::string_view text) {
        const size_t a = text.find(',');
        const size_t b = a == std::string_view::npos ? a : text.find(',', a + 1);
        if (b == std::string_view::npos) return std::nullopt;
        Camera2D cam;
        cam.scale = std::max(std::strtod(std::string(text.substr(b + 1)).c_str(), nullptr), kMinScale);
        if (!(cam.scale > 0.0)) return std::nullopt;
        const int limbs = BigFixed::limbsFor(cam.scale);
        auto x = BigFixed::parse(text.substr(0, a), limbs);
        auto y = BigFixed::parse(text.substr(a + 1, b - a - 1), limbs);
        if (!x || !y) return std::nullopt;
        cam.cx = *x;
        cam.cy = *y;
        return cam;
    }

    // "X,Y,SCALE" with enough digits to reproduce the view (inverse of parse).
    std::string toString() const {
        const int digits = std::max(17, int(-std::log10(scale)) + 8);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6e", scale);
        return cx.toString(digits) + "," + cy.toString(digits) + "," + buf;
    }

    // Center precision only grows, so zooming back in keeps the exact location.
    void growPrecision(double forScale) {
        const int limbs = BigFixed::limbsFor(forScale);
        if (limbs > cx.fracLimbs()) cx.setFracLimbs(limbs);
        if (limbs > cy.fracLimbs()) cy.setFracLimbs(limbs);
    }
};

} // namespace fb
