#pragma once

namespace fb {

// 2D view over the complex plane. Kept in double on the CPU; the GPU gets floats.
// Screen coordinates are framebuffer pixels, origin top-left.
struct Camera2D {
    double cx = -0.5;
    double cy = 0.0;
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
        const double wx = cx + ox * scale;
        const double wy = cy - oy * scale;
        scale *= factor;
        cx = wx - ox * scale;
        cy = wy + oy * scale;
    }
};

} // namespace fb
